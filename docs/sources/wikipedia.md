---
id: wikipedia
name: Wikipedia (English)
rows: [research_wikipedia]
catalogue: []
access: free, no key
probed: 2026-11-05
confidence: called   # the published pages were called; the Action API the app uses was not, because en.wikipedia.org's robots.txt has `Disallow: /w/` for all agents
---

# Wikipedia (English)

The app reads two pages of it through the MediaWiki Action API, as the source of index members: the list of S&P 500 companies and the Dow Jones navigation template.

## Contents
**`List_of_S&P_500_companies`** (HTML page, 2 tables, sections "S&P 500 component stocks", "See also", "References", "External links"). Table `id="constituents"`, 504 rows with its header, columns: `Symbol`, `Security`, `GICS Sector`, `GICS Sub-Industry`, `Headquarters Location`, `Date added`, `CIK`, `Founded`. A second table on the same page is a navigation box. The app asks for `section=1`.

**`Template:Dow_Jones_Industrial_Average_companies`**: a navigation box of links to the member companies, no columns of data.

The wikitext of the API was not seen here; `parseSp500` expects `{{NyseSymbol|…}}`-style templates in the symbol cell, `||`-separated or line-separated cells, and rows split on `|-`.

## How to call it
- The app's call: `GET https://en.wikipedia.org/w/api.php?action=parse&page=List_of_S%26P_500_companies&prop=wikitext&format=json&formatversion=2&section=1`, reading `parse.wikitext`. Not repeated here.
- **robots.txt** lists `Allow: /w/api.php?action=mobileview&`, `Allow: /w/load.php?`, then `Disallow: /w/`, so by its letter `action=parse` is closed to bots. MediaWiki's own API:Etiquette page documents the Action API for programs, so this is a difference between two of Wikimedia's texts rather than a clear prohibition; a person should decide. The HTML pages under `/wiki/` are allowed.
- API:Etiquette's summary: "Set an informative User-Agent string with contact information, or you may be IP-blocked without notice." The app sends `Deepwater/0.1 (desktop app; research page)`, with **no contact information** (`src/ResearchData.cpp:853`).

## What the app reads
Row `research_wikipedia`, `src/SourcesPage.cpp:441` (probe `:443`).
- `Client::indexes` `src/ResearchData.cpp:1907`, daily: S&P 500 `:1951`, Dow `:1958`; Nasdaq 100 comes from Nasdaq's API (`:1965`), with sectors taken from Wikipedia's rows.
- `parseSp500` `src/ResearchData.cpp:593`: the symbol, the link target and name of `Security`, and `GICS Sector`. `navboxTargets` `:632`: the Dow's link targets, matched to S&P rows by target.

## Not read yet
From the `constituents` table:
- `GICS Sub-Industry`: finer sectors for Research › Sectors.
- `CIK`: a direct key into SEC filings (`data.sec.gov`) without a ticker lookup.
- `Date added`: index membership history for backtests.
- `Headquarters Location`, `Founded`: Research's company page.

The page's selected changes table, if present in other sections, was not checked: the HTML scan found 2 tables only.

## Calls made
- `GET https://en.wikipedia.org/robots.txt` → 200, 28275 bytes.
- `https://en.wikipedia.org/w/api.php?action=parse&page=List_of_S%26P_500_companies&prop=wikitext&format=json&formatversion=2&section=1` → not called, robots.txt disallows it for `Deepwater/0.1 (desktop app; research page)`.
- `https://en.wikipedia.org/w/api.php?action=parse&page=Template:Dow_Jones_Industrial_Average_companies&prop=wikitext&format=json&formatversion=2` → not called, robots.txt disallows it for `Deepwater/0.1 (desktop app; research page)`.
- `GET https://en.wikipedia.org/wiki/List_of_S%26P_500_companies` → 200, 571857 bytes.
- `GET https://en.wikipedia.org/wiki/Template:Dow_Jones_Industrial_Average_companies` → 200, 82182 bytes.
- `GET https://www.mediawiki.org/robots.txt` → 200, 15254 bytes.
- `GET https://www.mediawiki.org/wiki/API:Etiquette` → 200, 95852 bytes.
