---
id: dnb
name: De Nederlandsche Bank
rows: []
catalogue: ["De Nederlandsche Bank"]
access: free key
probed: 2026-11-05
confidence: refused     # the website answers 403 Access Denied to our User-Agent; no API documentation could be read
---

# De Nederlandsche Bank

The Netherlands' central bank. Nothing about its contents could be verified from here.

## Contents
Not read. `www.dnb.nl/en/statistics/data-search/` and `www.dnb.nl/robots.txt` both answered 403 Access Denied (a CDN page). `api.dnb.nl` exists: it answers 404 "Resource not found" at the root and at a guessed path. `api-portal.dnb.nl` does not resolve.

## How to call it
Not known. `access: free key` is a guess from the 404 style of an API gateway, not a statement of DNB. Stopped there, as the rule says: no second identity was tried. The public catalogue's "DNB statistics API, no key" is not confirmed.

## What the app reads
Nothing.

## Not read yet
Unknown. For Dutch banking and monetary series, try the ECB (`ecb.md`) and the BIS (`bis.md`) first.

## Calls made
Made on 2026-11-05, sequentially per host with a pause, `User-Agent: Deepwater/1.0`. 5 calls.

- `GET https://api.dnb.nl/statisticsdata/v2024100101/dataset/list` → 404, 54 B
- `GET https://www.dnb.nl/en/statistics/data-search/` → 403, 406 B
- `GET https://api-portal.dnb.nl/` → ERR, 0 B
- `GET https://www.dnb.nl/robots.txt` → 403, 378 B
- `GET https://api.dnb.nl/` → 404, 54 B
