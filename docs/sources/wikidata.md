---
id: wikidata
name: Wikidata
rows: [research_wikidata]
catalogue: []
access: free, no key
probed: 2026-11-05
confidence: called   # property pages on www.wikidata.org were called; query.wikidata.org/sparql, which the app uses, was not: its robots.txt says `Disallow: /sparql`
---

# Wikidata

A structured knowledge base. The app queries it for companies listed on an exchange: head counts, and links between manufacturers, products and brands for its supply-chain view.

## Contents
Properties the app's queries use (label and datatype from each property's page):

| Property | Label | Type | Used for |
|---|---|---|---|
| `P414` | stock exchange | Item | finding a company by its listing |
| `P249` | ticker symbol | String | qualifier on `P414` |
| `P1128` | employees | Quantity | head count |
| `P585` | point in time | Point in time | qualifier: the head count's date |
| `P176` | manufacturer | Item | who makes a product |
| `P178` | developer | Item | whose product it is |
| `P1716` | brand | Item | whose product it is |
| `P127` | owned by | Item | a brand's listed owner; subsidiaries |
| `P749` | parent organization or unit | Item | same |
| `P452` | industry | Item | whether an entity is a company |

Related properties that exist, not used (each page called):

| Property | Label | Type |
|---|---|---|
| `P2139` | total revenue | Quantity |
| `P2295` | net profit | Quantity |
| `P3362` | operating income | Quantity |
| `P2403` | total assets | Quantity |
| `P2137` | total equity | Quantity |
| `P2226` | market capitalization | Quantity |
| `P355` | child organization or unit | Item |
| `P1830` | owner of | Item |
| `P1056` | product or material produced | Item |
| `P159` | headquarters location | Item |
| `P571` | inception | Point in time |
| `P946` | ISIN | External identifier |
| `P5531` | Central Index Key | External identifier |
| `P1278` | Legal Entity Identifier | External identifier |

Coverage of these properties across companies wasn't measured: that needs SPARQL. Percentages or supplier shares don't exist in the app's query results (project note, Research tab).

## How to call it
- **SPARQL**: `GET https://query.wikidata.org/sparql?query=<url-encoded>` with `Accept: application/sparql-results+json`. Answer: `results.bindings[]`, each variable `{type, value}`. The app's query ends `LIMIT 4000` and its comment says the aggregated form ran past the time limit (`src/ResearchData.cpp:1667`).
- **robots.txt** of `query.wikidata.org`: `Disallow: /sparql`, `Disallow: /bigdata`. By its letter the app's own calls are closed to bots, though Wikidata's "Data access" page presents the query service as an access route for programs, and asks for "the lowest timeout that makes sense". A person should decide. Not called here.
- **Other routes on Wikidata's Data access page**: Linked Data Interface (`Special:EntityData`), Wikibase REST API, MediaWiki Action API, Wikibase GraphQL, Recent Changes stream.
- Property pages `https://www.wikidata.org/wiki/Property:P414` are allowed.
- User-Agent: as on [wikipedia](wikipedia.md), Wikimedia asks for contact information and the app sends none.

## What the app reads
Row `research_wikidata`, `src/SourcesPage.cpp:444` (probe `:446`). All through `Client`, `src/ResearchData.cpp`, flag `Wiki`.
- `employees` `:1590`: `p:P414/pq:P249` for the ticker, `P1128` with `P585`; variables `n`, `date`; one point per year, cached 30 days. Shown by `src/ResearchCompanyViews.cpp:572`.
- `supplyChain` `:1665`: two queries, suppliers and customers, over `P176`, `P178`, `P1716`, `P127`, `P749`, `P452`; variables `co`, `p`, `pLabel`, `b`, `bLabel`, `isBrand`, `coBrand`, `hasBrand`, `bCompany`, `bSub`, `owner`, `ownerLabel`. Labels in `en,mul,zh,ja,ko,fr,de`.
- Listings `:1779`: `P414`, `P249` for the partners found, turned into Yahoo symbols; cached 7 days.
- Links to `www.wikidata.org/wiki/<item>`: `src/ResearchMarketViews.cpp:1321`.

## Not read yet
- `P946`, `P5531`, `P1278`: identifiers that would join a company to SEC filings or to other sources without ticker matching.
- `P2139`, `P2295`, `P2403`: financials with dates, for companies outside the US filings Research reads.
- `P355`, `P1830`: subsidiaries, direct, for Research › SPLC.
- `P1056`: what a company produces, for the same view.

## Calls made
- `GET https://query.wikidata.org/robots.txt` → 200, 95 bytes.
- `https://query.wikidata.org/sparql?query=ASK%7B%7D` → not called, robots.txt disallows it for `Deepwater/0.1 (desktop app; research page)`.
- `GET https://www.wikidata.org/robots.txt` → 200, 15896 bytes.
- `GET https://www.wikidata.org/wiki/Wikidata:Data_access` → 200, 141636 bytes.
- `GET https://www.wikidata.org/wiki/Property:P414` → 200, 81716 bytes.
- `GET https://www.wikidata.org/wiki/Property:P249` → 200, 143003 bytes.
- `GET https://www.wikidata.org/wiki/Property:P1128` → 200, 111008 bytes.
- `GET https://www.wikidata.org/wiki/Property:P585` → 200, 140418 bytes.
- `GET https://www.wikidata.org/wiki/Property:P176` → 200, 115065 bytes.
- `GET https://www.wikidata.org/wiki/Property:P178` → 200, 128905 bytes.
- `GET https://www.wikidata.org/wiki/Property:P1716` → 200, 99263 bytes.
- `GET https://www.wikidata.org/wiki/Property:P127` → 200, 187053 bytes.
- `GET https://www.wikidata.org/wiki/Property:P749` → 200, 121806 bytes.
- `GET https://www.wikidata.org/wiki/Property:P452` → 200, 110893 bytes.
- `GET https://www.wikidata.org/wiki/Property:P355` → 200, 127922 bytes.
- `GET https://www.wikidata.org/wiki/Property:P1830` → 200, 120662 bytes.
- `GET https://www.wikidata.org/wiki/Property:P2139` → 200, 137398 bytes.
- `GET https://www.wikidata.org/wiki/Property:P2295` → 200, 103895 bytes.
- `GET https://www.wikidata.org/wiki/Property:P2403` → 200, 124212 bytes.
- `GET https://www.wikidata.org/wiki/Property:P2137` → 200, 113549 bytes.
- `GET https://www.wikidata.org/wiki/Property:P3362` → 200, 97696 bytes.
- `GET https://www.wikidata.org/wiki/Property:P2226` → 200, 83933 bytes.
- `GET https://www.wikidata.org/wiki/Property:P946` → 200, 111540 bytes.
- `GET https://www.wikidata.org/wiki/Property:P5531` → 200, 155188 bytes.
- `GET https://www.wikidata.org/wiki/Property:P1278` → 200, 197013 bytes.
- `GET https://www.wikidata.org/wiki/Property:P1056` → 200, 141126 bytes.
- `GET https://www.wikidata.org/wiki/Property:P159` → 200, 161001 bytes.
- `GET https://www.wikidata.org/wiki/Property:P571` → 200, 224709 bytes.
