/* C ABI over Brave's adblock-rust engine (third_party/adblock-ffi/src/lib.rs). Every function is safe to call
 * with null and never panics across the boundary. The engine may be used from several threads at once. */
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AdblockEngine AdblockEngine;

/* An engine from `len` bytes of UTF-8 filter rules (ABP / uBlock Origin syntax). Slow: build it off the UI thread. */
AdblockEngine *adblock_engine_new(const char *rules, size_t len);
/* An engine from `adblock_engine_serialize`'s bytes; null when the format no longer matches. */
AdblockEngine *adblock_engine_deserialize(const uint8_t *bytes, size_t len);
uint8_t *adblock_engine_serialize(const AdblockEngine *engine, size_t *out_len);
void adblock_bytes_free(uint8_t *bytes, size_t len);
void adblock_engine_free(AdblockEngine *engine);

/* 1: block; 0: allow; -1: the URL couldn't be parsed. request_type: "script", "image", "stylesheet",
 * "subdocument", "xmlhttprequest", "font", "media", "ping", "websocket", "object", "other", "document". */
int32_t adblock_engine_check(const AdblockEngine *engine, const char *url, const char *source_url, const char *request_type);

/* JSON {"hide": [...], "exceptions": [...], "generichide": bool, "script": "..."}; free with adblock_string_free. */
char *adblock_engine_cosmetic(const AdblockEngine *engine, const char *url);
/* JSON array of the generic hiding selectors for a page with these classes and ids (JSON arrays), minus the
 * exceptions (the JSON array from adblock_engine_cosmetic); free with adblock_string_free. */
char *adblock_engine_hidden_selectors(const AdblockEngine *engine, const char *classes_json, const char *ids_json,
                                      const char *exceptions_json);
void adblock_string_free(char *s);

/* The same rules as WebKit content-blocker rules (a JSON array for WKContentRuleListStore), network blocking and
 * element hiding alike; *out_count receives the number of rules. Slow (seconds): call it off the UI thread. Free
 * with adblock_string_free. */
char *adblock_content_blocking_json(const char *rules, size_t len, size_t *out_count);

#ifdef __cplusplus
}
#endif
