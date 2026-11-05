---
id: itc
name: International Trade Centre
rows: []
catalogue: ["International Trade Centre"]
access: scraped web page
probed: 2026-11-05
confidence: refused
---

# International Trade Centre

A joint UN and WTO agency, by general knowledge rather than a call, behind the trade tools Trade Map and Market Access Map. Its main site refuses the app's identity; Trade Map's page answers but is a JavaScript app, so its contents are not known from a call.

## Contents
Nothing verified. `www.trademap.org` loads ("Trade Map beta is loading") without showing data in the page text. The search made for this card returned nothing on Trade Map's coverage or registration rules. The catalogue lists two countries for it, North Korea and Somalia, 6 Trading Economics pages.

Trade by reporter, partner and product is in Comtrade (`un.md`), which answers.

## How to call it
`https://intracen.org/resources/data-and-analysis` gives 403 "Just a moment…", a Cloudflare challenge. The block was left alone, as asked. No API is known.

## What the app reads
Nothing.

## Not read yet
Nothing to propose before Trade Map's terms are known.

## Calls made
All on 2026-11-05 with `User-Agent: Deepwater/1.0`.
- `GET https://intracen.org/resources/data-and-analysis` → 403, 5,519 bytes.
- `GET https://www.trademap.org/` → 200, 13,871 bytes.
- Web search shared with `afdb.md`: nothing on ITC came back.
