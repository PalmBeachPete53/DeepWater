---
id: adblock
name: Ad blocking filter lists
rows: [adblock]
catalogue: []
access: free, no key
probed: 2026-11-05
confidence: called
---

# Ad blocking filter lists

The 28 text lists the news reader's ad blocker downloads: Brave's default set, on four hosts. They hold blocking rules, nothing about markets.

## Contents
`catalog/adblock.tsv` has every list with its URL, bytes, lines, rule counts by kind, the list's own title and expiry. Totals on this date: 28 lists, 8.88 MB, 207,167 rules.

| Host | Lists | Bytes | Which |
|---|---|---|---|
| `raw.githubusercontent.com` | 23 | 4.00 MB | 16 `uBlockOrigin/uAssets/master/filters/` (`filters.txt`, `filters-2020` to `-2026`, `filters-general`, `badware`, `resource-abuse`, `unbreak`, `quick-fixes`, `ubo-link-shorteners`, `privacy`, `annoyances-cookies`), 7 `brave/adblock-lists/master/brave-lists/` (`brave-unbreak`, `-specific`, `-social`, `-sugarcoat`, `-firstparty`, `-firstparty-regional`, `-cookie-specific`) |
| `easylist.to` | 2 | 3.58 MB | `easylist.txt`, `easyprivacy.txt` |
| `secure.fanboy.co.nz` | 2 | 1.27 MB | `fanboy-cookiemonster_ubo.txt`, `fanboy-mobile-notifications.txt` |
| `malware-filter.gitlab.io` | 1 | 0.04 MB | `urlhaus-filter-agh-online.txt` |

The largest are EasyList (2.06 MB, 77,813 rules), EasyPrivacy (1.51 MB, 56,487 rules), EasyList Cookie (1.22 MB, 28,184 rules). `brave-unbreak.txt` holds a single comment line and no rule.

Format: one rule per line, `!` comments (`[Adblock Plus …]` headers), `! Title:`, `! Expires:` (from 8 hours for Quick fixes and 12 hours for URLhaus to 9 days for the Fanboy lists). The kinds counted are network, exception (`@@`), cosmetic (`##`, `#@#`, `#?#`) and scriptlet (`##+js(`). The counting is a line-by-line approximation of adblock syntax.

## How to call it
`GET` each URL, no key. Size only: `HEAD` gives `Content-Length` at `raw.githubusercontent.com` and `malware-filter.gitlab.io`; `easylist.to` and `secure.fanboy.co.nz` gave none. Refresh: `python3 -I tools/source-inventory/adblock.py <repo root>`.

## What the app reads
Row `adblock`, `src/SourcesPage.cpp:811`. List `src/AdBlock.cpp:64-92`, taken from Brave's `list_catalog.json` on 2026-11-01 (the code's comment, `:60`).
- Every list whole, with User-Agent `Deepwater/0.1 (desktop app; ad blocker lists)` (`src/AdBlock.cpp:150`), once a day (`KeepSecs`, `:26`). Compiled by adblock-rust (`third_party/adblock-ffi`); kept as `cache_adblock-list-<n>.bin` and `cache_adblock-webkit.bin`.

## Not read yet
Brave's catalogue also has regional lists and annoyance lists not enabled by default (per the code's comment, it keeps only `default_enabled`); it was not called here.

## Calls made
Each list once: `GET <url>` → status and bytes in `catalog/adblock.tsv`, all 28 answered 200, by `tools/source-inventory/adblock.py` (which also read each host's `robots.txt`, without logging it; no list was disallowed).

- `GET https://raw.githubusercontent.com/robots.txt` → 404, 14 bytes.
- `HEAD https://raw.githubusercontent.com/brave/adblock-lists/master/brave-lists/brave-cookie-specific.txt` → 200, Content-Length 24641.
- `GET https://easylist.to/robots.txt` → 200, 1248 bytes.
- `HEAD https://easylist.to/easylist/easylist.txt` → 200, Content-Length absent.
- `GET https://secure.fanboy.co.nz/robots.txt` → 404, 170 bytes.
- `HEAD https://secure.fanboy.co.nz/fanboy-mobile-notifications.txt` → 200, Content-Length absent.
- `GET https://malware-filter.gitlab.io/robots.txt` → 403, 5514 bytes.
- `HEAD https://malware-filter.gitlab.io/malware-filter/urlhaus-filter-agh-online.txt` → 200, Content-Length 35764.
