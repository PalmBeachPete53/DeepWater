---
id: iep
name: Institute for Economics and Peace
rows: []
catalogue: ["Institute for Economics and Peace"]
access: free account
probed: 2026-11-05
confidence: called
---

# Institute for Economics and Peace

Publisher of the Global Peace Index and related indices, as reports and a map site. The data files are behind a request form and a licence.

## Contents
Products named by the site: Global Peace Index, Global Terrorism Index, Ecological Threat Report, Positive Peace Report; on Vision of Humanity also the Mexico, US, UK and Northern Ireland Peace Indexes.

**Global Peace Index**: 163 countries, 23 qualitative and quantitative indicators, three domains (Societal Safety and Security; Ongoing Domestic and International Conflict; Militarisation), annual; latest report 2026. The list of indicators is in the report's methodology, which was not read.

Formats: PDF reports open (`Global-Peace-Index-2026-Report.pdf`, `Positive-Peace-Report-2026.pdf`); data files by form only. The maps page was read as text, without its interactive data.

## How to call it
- `https://www.economicsandpeace.org/`, `/global-peace-index/`, `/reports/`, `/consulting/data-licensing/`; `https://www.visionofhumanity.org/maps/`.
- No API found. `robots.txt` leaves these pages open.
- **Licence:** the site says non-commercial use by non-profits and research institutions is CC BY-NC-SA 4.0 after a form; any use by a company, private or public, needs a paid commercial licence before downloading. Check this before putting it in the app.

## What the app reads
Nothing.

## Not read yet
**Economy › Countries:** GPI score and rank, subject to the licence above.

## Calls made
All on 2026-11-05 with `User-Agent: Deepwater/1.0`.
- `GET https://www.economicsandpeace.org/robots.txt` → 200, 199 bytes.
- `GET https://www.visionofhumanity.org/robots.txt` → 200, 200 bytes.
- `GET https://www.economicsandpeace.org/` → 200, 121,958 bytes.
- `GET https://www.economicsandpeace.org/global-peace-index/` → 200, 94,335 bytes.
- `GET https://www.economicsandpeace.org/consulting/data-licensing/` → 200, 150,848 bytes.
- `GET https://www.visionofhumanity.org/maps/` → 200, 134,470 bytes (twice).
