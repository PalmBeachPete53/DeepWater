---
id: wef
name: World Economic Forum
rows: []
catalogue: ["World Economic Forum"]
access: scraped web page
probed: 2026-11-05
confidence: refused
---

# World Economic Forum

Report-based indices. The site refuses the app's identity with a 403 from its content network, on `robots.txt` as well, so the card rests on documentation and nothing was verified by a call.

## Contents
From third-party documentation only, not from the source:
- Global Competitiveness Index 4.0, 2019 report: a report page listing a Dataset (Excel), the full report and the Executive Opinion Survey (PDF). A University of Gothenburg record (QoG) gives 140 countries for its 2018 cross-section; the 2013-2014 edition is said to cover 148 economies. Which editions and years are still downloadable was not established.
- Global Gender Gap: nothing found on its data files.
- The catalogue says 304 pages of Trading Economics cite the body, for 152 countries.

## How to call it
No call answered. `https://www.weforum.org/publications/` and `/robots.txt` give 403 "Access Denied" (Akamai reference page) to `User-Agent: Deepwater/1.0`. The block was left alone, as asked. No API is known. Terms: the 2017-2018 report asks people who redistribute the data to promote compliance with its terms, according to the same search.

## What the app reads
Nothing.

## Not read yet
**Economy › Countries:** GCI scores, if the Excel file can be fetched by hand.

## Calls made
All on 2026-11-05 with `User-Agent: Deepwater/1.0`.
- `GET https://www.weforum.org/robots.txt` → 403, 379 bytes (twice).
- `GET https://www.weforum.org/publications/` → 403, 382 bytes.
- Web search "World Economic Forum Global Competitiveness Index dataset Executive Opinion Survey Global Gender Gap data download coverage": result titles and summaries only, among them `datafinder.qog.gu.se/dataset/wef`, `datafinder.qog.gu.se/variable/wef_gci`, `www.weforum.org/publications/how-to-end-a-decade-of-lost-productivity-growth/downloads`.
