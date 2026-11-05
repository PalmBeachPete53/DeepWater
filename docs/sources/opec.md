---
id: opec
name: Organization of the Petroleum Exporting Countries
rows: []
catalogue: ["Organization of the Petroleum Exporting Countries", "OPEC"]
access: scraped web page
probed: 2026-11-05
confidence: refused
---

# OPEC

Oil statistics: the Annual Statistical Bulletin and the monthly report. Its sites refuse the app's identity with a Cloudflare block, so nothing here was verified at the source.

## Contents
From documentation and third parties only:
- **Annual Statistical Bulletin**: 61st edition, ASB-2026, launched 29 April 2026 (OPEC press release, by search). Interactive site `asb.opec.org`; a 2019 release says its app downloads complete time series back to 1960; the options today were not confirmed. An older edition has a section "Oil prices".
- Third-party copies exist (KAPSARC's open data for the 2015 edition; unofficial extracts). They were not used.
- **Monthly Oil Market Report and the OPEC basket price**: the search found nothing on data files, so this card says nothing about their format.

## How to call it
No call answered. `www.opec.org`, its `robots.txt` and `asb.opec.org` give 403 ("Sorry, you have been blocked") to `User-Agent: Deepwater/1.0`. The block was left alone, as asked. For questions, the bulletins name `dataqueries@opec.org`.

## What the app reads
Nothing.

## Not read yet
**Desk › Markets, oil's sheet; Map › supply chains:** production by member and the basket price, if OPEC grants access. The EIA and the Eurostat `nrg_cb_oilm` cards cover oil from other angles.

## Calls made
All on 2026-11-05 with `User-Agent: Deepwater/1.0`.
- `GET https://www.opec.org/robots.txt` → 403, 5,977 bytes.
- `GET https://www.opec.org/opec_web/en/data_graphs/40.htm` → 403, 5,977 bytes.
- `GET https://asb.opec.org/` → 403, 5,977 bytes.
- Web search "OPEC Annual Statistical Bulletin data download tables Monthly Oil Market Report OPEC basket price dataset": result titles and summaries only, among them `www.opec.org/pr-detail/1784601-29-april-2026.html`, `www.opec.org/pr-detail/331-03-jun-2019.html`, `opennetzero.org/dataset/opec-annual-statistical-bulletin`.
