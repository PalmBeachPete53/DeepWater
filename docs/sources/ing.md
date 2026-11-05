---
id: ing
name: ING Think
rows: [research_ing]
catalogue: []
access: scraped web page
probed: 2026-11-05
confidence: called
---

# ING Think

The research site of ING: articles and snaps by economists, and a page of forecasts by quarter in plain HTML tables. An RSS feed lists the newest articles.

## Contents
**Forecast page** `https://think.ing.com/forecasts/`: 7 tabs holding 8 HTML tables, with a monthly PDF of the same forecasts.

| Tab id | Title | Tables and row groups | Columns |
|---|---|---|---|
| `growth-gdp` | Growth (GDP) | 42 rows: QoQ% annualised (G10 and euro area), YoY% (EMEA, CIS, Asia) | 6 quarters, 3 years |
| `inflation-cpi` | Inflation (CPI) | 40 rows: same areas, YoY% average | 6 quarters, 3 years |
| `policy-rates` | Policy rates | 29 rows: G10, EMEA, Asia; end of period | 5 quarters, then the 4th quarter of the year after |
| `month-rates` | 3 month rates | 23 rows: G10, EMEA, Asia | same |
| `year-yields` | Bonds & Swaps | 52 rows: UST 2/5/10/30Y, SOFR swaps, Bund 2/5/10/30Y, EURIBOR swaps, other G10 10Y, EMEA 10Y | same |
| `fx` | FX | 41 rows: G10, EMEA, CIS, Latam, Asia; a second label column gives the pair | 5 quarters |
| `commodities` | Commodities | 2 tables: oil and natural gas (`Brent ($/bbl)`, `Dutch TTF (EUR/MWh)`); metals (copper, aluminium, nickel, zinc, lead, gold, silver, iron ore) | 5 quarters, 2 years |

Each pane gives "Last updated: <day month>". The forecast is monthly; the PDF is `https://think.ing.com/uploads/pdf-forecasts/<Month>_<Year>_INGF.pdf`.

**RSS** `https://think.ing.com/rss/`: 10 items, spanning 23.5 hours in this sample. Item elements: `title`, `link`, `guid`, `description`, `dc:date`, `dc:subject` (a region or theme label: "FX", "Rates", "Commodities daily", "North America, United States"). No `pubDate`.

**Site sections** seen in links: `latest`, `snaps`, `articles`, `reports`, `opinions`, `bundles`, `topics`.

## How to call it
- `GET https://think.ing.com/forecasts/` and `GET https://think.ing.com/rss/`, no key. robots.txt allows both.
- Trap: the page is parsed by its `tab-pane` structure; a redesign breaks it and the app says so.
- Terms (`/about/terms-of-use/`): for content whose copyright is ING's, "ING grants you the license to use, distribute, reproduce, modify, adapt, publicly perform and publicly display such Content by mentioning ING Bank N.V."; content owned by third parties is excluded. The app states the source in Desk › Forecasts (`src/DeskViewsMacro.cpp:983`).

## What the app reads
Row `research_ing`, `src/SourcesPage.cpp:647`.
- Forecasts: `src/DeskViewsMacro.cpp:1078`, every 12 hours. `desk::parseIngForecasts` `src/desk/CentralBanks.cpp:248` keeps, per tab, `id`, `title`, the "Last updated" text, and per table the group, the period columns, each row's labels and values. Shown in Desk › Forecasts.
- Notes: `src/DeskViewsTech.cpp:580`, hourly, through `news::Wire::parseFeed` (`title`, `link`, `description` cut to 400 characters, `dc:date`). The feed has no creator, so the publisher stays empty.

## Not read yet
- Bond yields and swap rates by tenor (`year-yields`) beside the market curves in Desk › Rates; Desk › Forecasts shows them on their own tab today.
- The monthly PDF, for its commentary.
- `dc:subject` of the feed: filtering Desk › Notes by region or theme.

## Calls made
- `GET https://think.ing.com/robots.txt` → 200, 20470 bytes.
- `GET https://think.ing.com/rss/` → 200, 9167 bytes.
- `GET https://think.ing.com/forecasts/` → 200, 146911 bytes.
- `GET https://think.ing.com/about/terms-of-use/` → 200, 88192 bytes.
