//! A small C ABI over Brave's `adblock` crate (adblock-rust), for Deepwater's article reader.
//!
//! One engine is built from the text of the filter lists (ABP / uBlock Origin syntax, concatenated), then asked
//! whether a network request should be blocked, and for the cosmetic (element hiding) rules of a page. Every
//! entry point catches panics, so a bad list or URL can never bring the application down. The engine is `Sync`
//! (the regex cache is behind a mutex), so requests can be checked from Chromium's IO thread while the UI
//! thread asks for cosmetic rules.

use adblock::lists::ParseOptions;
use adblock::request::Request;
use adblock::{Engine, FilterSet};
use std::collections::HashSet;
use std::ffi::{c_char, CStr, CString};
use std::panic::{catch_unwind, AssertUnwindSafe};

pub struct AdblockEngine {
    engine: Engine,
}

fn cstr<'a>(p: *const c_char) -> Option<&'a str> {
    if p.is_null() {
        return None;
    }
    unsafe { CStr::from_ptr(p) }.to_str().ok()
}

fn out(s: String) -> *mut c_char {
    CString::new(s.replace('\0', "")).map(CString::into_raw).unwrap_or(std::ptr::null_mut())
}

/// Builds an engine from `len` bytes of UTF-8 filter rules. Null when the rules aren't UTF-8 or on a panic.
/// The build takes a second or two for Brave's default lists: call it off the UI thread.
#[no_mangle]
pub extern "C" fn adblock_engine_new(rules: *const c_char, len: usize) -> *mut AdblockEngine {
    if rules.is_null() {
        return std::ptr::null_mut();
    }
    let bytes = unsafe { std::slice::from_raw_parts(rules as *const u8, len) };
    let text = match std::str::from_utf8(bytes) {
        Ok(t) => t,
        Err(_) => return std::ptr::null_mut(),
    };
    catch_unwind(AssertUnwindSafe(|| {
        let mut set = FilterSet::new(false);
        set.add_filter_list(text.to_string(), ParseOptions::default());
        let engine = Engine::new_with_filter_set(set);
        Box::into_raw(Box::new(AdblockEngine { engine }))
    }))
    .unwrap_or(std::ptr::null_mut())
}

/// Rebuilds an engine from `adblock_engine_serialize`'s bytes (a cache; the format may change between versions,
/// in which case this returns null and the lists have to be parsed again).
#[no_mangle]
pub extern "C" fn adblock_engine_deserialize(bytes: *const u8, len: usize) -> *mut AdblockEngine {
    if bytes.is_null() {
        return std::ptr::null_mut();
    }
    let data = unsafe { std::slice::from_raw_parts(bytes, len) };
    catch_unwind(AssertUnwindSafe(|| {
        let mut engine = Engine::new_with_list_text("");
        match engine.deserialize(data) {
            Ok(()) => Box::into_raw(Box::new(AdblockEngine { engine })),
            Err(_) => std::ptr::null_mut(),
        }
    }))
    .unwrap_or(std::ptr::null_mut())
}

/// The engine as bytes for `adblock_engine_deserialize`; `*out_len` receives the length. Free with
/// `adblock_bytes_free`. Null on failure.
#[no_mangle]
pub extern "C" fn adblock_engine_serialize(e: *const AdblockEngine, out_len: *mut usize) -> *mut u8 {
    if e.is_null() || out_len.is_null() {
        return std::ptr::null_mut();
    }
    let engine = unsafe { &(*e).engine };
    catch_unwind(AssertUnwindSafe(|| {
        let mut v = engine.serialize().into_boxed_slice();
        unsafe { *out_len = v.len() };
        let p = v.as_mut_ptr();
        std::mem::forget(v);
        p
    }))
    .unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn adblock_bytes_free(bytes: *mut u8, len: usize) {
    if !bytes.is_null() {
        unsafe { drop(Box::from_raw(std::slice::from_raw_parts_mut(bytes, len))) };
    }
}

#[no_mangle]
pub extern "C" fn adblock_engine_free(e: *mut AdblockEngine) {
    if !e.is_null() {
        unsafe { drop(Box::from_raw(e)) };
    }
}

/// 1 when the request should be blocked, 0 when it may go, -1 when the URL can't be parsed.
/// `request_type`: "script", "image", "stylesheet", "subdocument", "xmlhttprequest", "font", "media", "ping",
/// "websocket", "object", "other", "document".
#[no_mangle]
pub extern "C" fn adblock_engine_check(
    e: *const AdblockEngine,
    url: *const c_char,
    source_url: *const c_char,
    request_type: *const c_char,
) -> i32 {
    if e.is_null() {
        return 0;
    }
    let (Some(url), Some(kind)) = (cstr(url), cstr(request_type)) else { return -1 };
    let source = cstr(source_url).unwrap_or("");
    let engine = unsafe { &(*e).engine };
    catch_unwind(AssertUnwindSafe(|| match Request::new(url, source, kind, "GET") {
        Ok(req) => {
            if engine.check_network_request(&req).should_block() {
                1
            } else {
                0
            }
        }
        Err(_) => -1,
    }))
    .unwrap_or(0)
}

/// The cosmetic resources of a page, as JSON: {"hide": [selectors], "exceptions": [selectors],
/// "generichide": bool, "script": "..."}. Free with `adblock_string_free`.
#[no_mangle]
pub extern "C" fn adblock_engine_cosmetic(e: *const AdblockEngine, url: *const c_char) -> *mut c_char {
    if e.is_null() {
        return std::ptr::null_mut();
    }
    let Some(url) = cstr(url) else { return std::ptr::null_mut() };
    let engine = unsafe { &(*e).engine };
    catch_unwind(AssertUnwindSafe(|| {
        let r = engine.url_cosmetic_resources(url);
        let mut hide: Vec<&String> = r.hide_selectors.iter().collect();
        hide.sort();
        let mut exceptions: Vec<&String> = r.exceptions.iter().collect();
        exceptions.sort();
        let v = serde_json::json!({
            "hide": hide,
            "exceptions": exceptions,
            "generichide": r.generichide,
            "script": r.injected_script,
        });
        out(v.to_string())
    }))
    .unwrap_or(std::ptr::null_mut())
}

/// The generic hiding selectors that apply to a page whose elements carry these classes and ids (JSON arrays of
/// strings), minus `exceptions` (the JSON array from `adblock_engine_cosmetic`). A JSON array of selectors; free
/// with `adblock_string_free`.
#[no_mangle]
pub extern "C" fn adblock_engine_hidden_selectors(
    e: *const AdblockEngine,
    classes_json: *const c_char,
    ids_json: *const c_char,
    exceptions_json: *const c_char,
) -> *mut c_char {
    if e.is_null() {
        return std::ptr::null_mut();
    }
    let parse = |p: *const c_char| -> Vec<String> {
        cstr(p)
            .and_then(|s| serde_json::from_str::<Vec<String>>(s).ok())
            .unwrap_or_default()
    };
    let classes = parse(classes_json);
    let ids = parse(ids_json);
    let exceptions: HashSet<String> = parse(exceptions_json).into_iter().collect();
    let engine = unsafe { &(*e).engine };
    catch_unwind(AssertUnwindSafe(|| {
        let selectors = engine.hidden_class_id_selectors(&classes, &ids, &exceptions);
        out(serde_json::to_string(&selectors).unwrap_or_else(|_| "[]".to_string()))
    }))
    .unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn adblock_string_free(s: *mut c_char) {
    if !s.is_null() {
        unsafe { drop(CString::from_raw(s)) };
    }
}

/// The same rules as WebKit content-blocker rules (a JSON array for `WKContentRuleListStore`), network blocking
/// and element hiding alike; `*out_count` receives the number of rules. Rules WebKit's format can't express are
/// left out. Null on failure. Slow (seconds for Brave's lists): call it off the UI thread.
#[no_mangle]
pub extern "C" fn adblock_content_blocking_json(rules: *const c_char, len: usize, out_count: *mut usize) -> *mut c_char {
    if rules.is_null() {
        return std::ptr::null_mut();
    }
    let bytes = unsafe { std::slice::from_raw_parts(rules as *const u8, len) };
    let Ok(text) = std::str::from_utf8(bytes) else { return std::ptr::null_mut() };
    catch_unwind(AssertUnwindSafe(|| {
        let mut set = FilterSet::new(true);
        set.add_filter_list(text.to_string(), ParseOptions::default());
        match set.into_content_blocking() {
            Ok((cb_rules, _)) => {
                if !out_count.is_null() {
                    unsafe { *out_count = cb_rules.len() };
                }
                out(serde_json::to_string(&cb_rules).unwrap_or_else(|_| "[]".to_string()))
            }
            Err(()) => std::ptr::null_mut(),
        }
    }))
    .unwrap_or(std::ptr::null_mut())
}
