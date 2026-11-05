---
id: other-hosts
name: Other hosts in the code
rows: []
catalogue: []
access: scraped web page
probed: 2026-11-05
confidence: called
---

# Other hosts in the code

The hosts in `src/` that no other card owns: what the app gets from each, and what's there. Several are only links opened in a browser.

## Contents

| Host | What the app does with it | What's there |
|---|---|---|
| `raw.githubusercontent.com` | 23 uses, all filter lists: `uBlockOrigin/uAssets` and `brave/adblock-lists` | see [adblock](adblock.md) |
| `www.investing.com` | 3 RSS feeds: `news_1`, `news_25`, `news_301` | 27 feeds at `/webmaster-tools/rss`, below |
| `seekingalpha.com` | 1 RSS feed: `market_currents.xml` | Feed page, below |
| `www.fxstreet.com` | 1 RSS feed: `/rss/news` | `/rss` is a second feed, not a list; same elements |
| `www.bis.org` | 1 RSS feed: `doclist/cbspeeches.rss` | RSS 1.0 with a `cb:` namespace, see [newsfeeds](newsfeeds.md). A guess at a feed index, `/list/rss/index.htm`, was a 404 |
| `www.boj.or.jp` | Meeting dates: `/en/mopo/mpmsche_minu/index.htm`, `parseBojMeetings` | 2 tables (2026, 2027), 23 PDFs: statements, outlook reports, opinions, minutes |
| `www.snb.ch` | Meeting dates: `/en/services-events/digital-services/event-schedule`, `parseSnbMeetings` matches "Monetary policy assessment of…" | A schedule of speeches, assessments, bulletin and annual results dates |
| `www.eia.gov` | Only the Sources row's probe `/opendata/` | Bulk files, below. The API on `api.eia.gov` belongs to another card |
| `fc.yahoo.com` | A Yahoo session: GET for its cookie, answers 404 by design | Nothing else: 404 HTML of 4.6 KB. The cookie's name wasn't logged |
| `economics.td.com` | Link only, opened in the browser | `ca-forecast-tables`: 13 HTML tables, below |
| `www.kbc.com` | Link only | `economics/forecasts/world.html`: 4 tables, below |
| `www.mufgresearch.com` | Link only | `forecasts/`: 5 tables, below |
| `www.rbc.com` | Link only | `monthly-forecast-update/`: no HTML table, PDFs `MFU-<Mon>-CA.pdf`, `MFU-<Mon>-US.pdf`, `Forecast-tablesFX.pdf` |
| `part-timequant.com` | Link only: help page of the ported Quant Terminal (`src/quant/ui/App.cpp:78`) | Help and what's new; no data |
| `www.part-timequant.com` | A link with a campaign query (`src/quant/server/Ptq.cpp:18`). The optional connector talks to `http://127.0.0.1:8787` or `PTQ_URL`, and reads a licence in `~/.ptq` | A commercial backtest and practice product; no public data |
| `api.telegram.org` | Only named in a hint: a user's own alert webhook URL (`src/quant/ui/Panels.cpp:1169`); the app POSTs `{text}` (`src/quant/server/Alerts.cpp:236`). The app holds no token | Telegram's Bot API; `core.telegram.org/bots/api` documents it. Not a data source |
| `www.youtube.com` | Link only, inside the app: live channels `@markets`, `@YahooFinance`, `@CNBCtelevision` (`src/DeskViewsMarket.cpp:794-796`) | Video pages. Its robots.txt was read; no data taken |

Also in `src/`, neither read nor opened: `www.w3.org` (XML namespaces), `github.com` (a licence notice, `src/quant/NOTICE:5`), `http://localhost` and `127.0.0.1` (the app's own servers).

**Investing.com feeds** at `/webmaster-tools/rss`, by file name:
- Analysis: `121899`, `286`, `290`, `market_overview`, `stock`, `forex`, `commodities`, `302`, `bonds`, `320`.
- News: `news`, `news_1` forex, `news_11` commodities and futures, `news_25` stock market, `news_95` economic indicators, `news_14` economy, `news_301` cryptocurrency, `news_357` insider trading, `news_356` company, `news_1062` earnings, `news_1065` investment ideas, `news_1060` SWOT, `news_1061` analyst ratings, `news_1063` earnings call transcripts, `news_355` press releases, `news_1064` SEC filings, `investing_news`.

**Seeking Alpha** (`about.seekingalpha.com/feeds`): `/feed.xml`, `/market_currents.xml`, `/listing/most-popular-articles.xml`, `/tag/<x>.xml` (`editors-picks`, `forex`, `ipo-analysis`, `long-ideas`, `wall-st-breakfast`, `etf-portfolio-strategy`, countries), `/sector/<x>.xml` (`financial`, `healthcare`, `utilities`, `transcripts`), `/api/sa/combined/<TICKER>.xml`.

**EIA bulk** (`/opendata/bulk/manifest.txt`, JSON `dataset`, 29 entries): `AEO.2014` to `AEO.2026` (12), `IEO` and 4 editions, `COAL`, `EBA`, `ELEC`, `EMISS`, `INTL`, `NG`, `NUC_STATUS`, `PET`, `PET_IMPORTS`, `SEDS`, `STEO`, `TOTAL`. Each has `last_updated`, `category_id`, `name`, `data_set`, `identifier`, `title`, `description`, `keyword`, `publisher`, `person`, `mbox`, `accessLevel`, `accessLevelComment`, `accessURL`, `webService`, `format`, `temporal`, `modified`. 31 zip links on the page.

**Forecast tables of the banks the app only links to:**
- **TD** (`td`): spot and forecast rates (Canada, US, 2026 to 2028), FX against USD, EUR, JPY, CAD, 8 commodities, Canadian and US outlook by quarter (GDP components, labour, prices, housing), G7 and Europe, global GDP by region, long-term US and Canada, provinces' home sales and prices.
- **KBC** (`kbc`): real GDP and inflation (25 economies or more), policy rates, 10-year yields, 19 exchange rates, end of period.
- **MUFG** (`mufg`): FX 33 rows by quarter (DXY and pairs, Q4 2026 to Q3 2027), GDP and CPI by year (2025 to 2027, 35 economies, Gulf states included), policy rates (31), government 2Y and 10Y of 4 areas.
- **RBC**: PDFs only.

## How to call it
Plain `GET` as in the table. Not for automated use, per the app's own note (`src/DeskViewsMacro.cpp:1050`): "RBC and MUFG don't allow theirs to be extracted or quoted without consent, and TD and KBC state no such permission, so their figures stay on their pages." Those clauses aren't on the pages called, which doesn't rule out separate terms.

**Investing.com terms** (`/about-us/terms-and-conditions`): "It is prohibited to use, store, reproduce, display, modify, transmit or distribute the data contained in this website without the explicit prior written permission of Fusion Media and/or the data provider." The app reads and caches three feeds. Its `robots.txt` answers 403, its feeds 200.

## What the app reads
- `src/NewsData.cpp:41-42`, `:49`, `:52-53`: the feeds.
- `src/DeskViewsTech.cpp:584`: BIS.
- `src/DeskData.cpp:70-71`: BoJ and SNB, daily.
- `src/SourcesPage.cpp:523`: EIA's probe.
- `src/ResearchData.cpp:802`: `fc.yahoo.com`.
- `src/DeskViewsMacro.cpp:1054-1055`: the four banks' links.

## Not read yet
- Investing.com's `news_95`, `news_14`, subject to its terms.
- BoJ's PDFs: "Outlook for Economic Activity and Prices", opinions and minutes for Desk › Notes.
- SNB's schedule beyond assessments: bulletin, balance of payments and results dates, for the Calendar.
- EIA's bulk `STEO`, `INTL`, `PET_IMPORTS`: whole data sets without the key, for the Map's energy views.
- TD's quarterly tables are the widest of the four; extracting them needs a look at TD's terms first.

## Calls made
- `GET https://www.boj.or.jp/robots.txt` → 404, 3297 bytes.
- `GET https://www.boj.or.jp/en/mopo/mpmsche_minu/index.htm` → 200, 42362 bytes.
- `GET https://www.snb.ch/robots.txt` → 200, 64 bytes.
- `GET https://www.snb.ch/en/services-events/digital-services/event-schedule` → 200, 170627 bytes.
- `GET https://economics.td.com/robots.txt` → 404, 10355 bytes.
- `GET https://economics.td.com/ca-forecast-tables` → 200, 1820873 bytes.
- `GET https://www.kbc.com/robots.txt` → 200, 419 bytes.
- `GET https://www.kbc.com/en/economics/forecasts/world.html` → 200, 109941 bytes.
- `GET https://www.mufgresearch.com/robots.txt` → 200, 84 bytes.
- `GET https://www.mufgresearch.com/forecasts/` → 200, 77707 bytes.
- `GET https://www.rbc.com/robots.txt` → 200, 37 bytes.
- `GET https://www.rbc.com/en/economics/monthly-forecast-update/` → 200, 342461 bytes.
- `GET https://part-timequant.com/robots.txt` → 200, 92 bytes.
- `GET https://part-timequant.com/tradingview/help` → 200, 104479 bytes.
- `GET https://www.part-timequant.com/robots.txt` → 200, 92 bytes.
- `GET https://www.part-timequant.com/` → 200, 122086 bytes.
- `GET https://fc.yahoo.com/robots.txt` → 404, 4595 bytes.
- `GET https://fc.yahoo.com/` → 404, 4585 bytes.
- `GET https://www.eia.gov/robots.txt` → 200, 718 bytes.
- `GET https://www.eia.gov/opendata/` → 200, 72027 bytes.
- `GET https://www.youtube.com/robots.txt` → 200, 792 bytes.
- `GET https://core.telegram.org/robots.txt` → 404, 153 bytes.
- `GET https://core.telegram.org/bots/api` → 200, 860230 bytes.
- `GET https://www.investing.com/webmaster-tools/rss` → 200, 189200 bytes.
- `GET https://seekingalpha.com/page/feeds` → 200, 185867 bytes.
- `GET https://www.fxstreet.com/rss` → 200, 22035 bytes.
- `GET https://www.bis.org/list/rss/index.htm` → 404, 14155 bytes.
- `GET https://www.eia.gov/opendata/bulk/manifest.txt` → 200, 28398 bytes.
- `GET https://www.investing.com/about-us/terms-and-conditions` → 200, 165736 bytes.
- `HEAD https://fc.yahoo.com/` → 404, Content-Length 4583.
