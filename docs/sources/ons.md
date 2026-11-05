---
id: ons
name: Office for National Statistics (UK)
rows: [research_ons]
catalogue: ["Office for National Statistics"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Office for National Statistics

UK official statistics: tens of thousands of macro time series, thousands of dataset pages, and a filterable data set API that holds mostly the census.

## Contents
Three layers, each with its own list.

- **Time series**, by CDID (a 4-character code) inside a data set. The search API counts 58,776; `catalog/ons_timeseries.tsv.gz` has 54,096 of them (cdid, dataset, title, uri, release date), since search stops at offset 10,000 and the rest had to be reached by filter. All the series the app reads are in it. Data sets (41 found, by size): `RAGV` 6,255 regional GVA; `UKEA` 5,884 accounts; `BB` 5,417 Blue Book; `CAPSTK`
  5,060 capital stocks; `MM23` 4,070 consumer prices; `PB` 3,423 Pink Book; `PPI` 3,383 producer prices; `DIOP` 2,582 production; `MRET` 1,991 trade; `LMS` 1,967 labour market; `MM22` 1,391; `QNA` 1,358 quarterly accounts; `CT` 1,156; `MQ5` 967; `PN2` 917; `PUSF` 668 public finance; `EDP`-`EDP4`; `DRSI` 621 retail; `IOS1` 594 services output; `NBS` 497; `EMP` 458; `UNEM` 337; `CXNV` 322; `PNBP` 285 balance of payments; `PRDY` 242 productivity;
  `SPPI` 216; `MGDP` 207 monthly GDP; `TOPSI` 200; `WGDP`, `AM`, `PGDP`, `PSE`, `PROF`, `RPRD`, `UCST`, `MM19`, `srs`.
- A series' JSON (`.../timeseries/d7g7/mm23/data`): `years`, `quarters`, `months` (each entry `date`, `value`, and more), `sourceDatasets`, `relatedDatasets`, `relatedDocuments`, `versions`, `type`, `uri`, `description` (`title`, `cdid`, `datasetId`, `datasetUri`, `unit`, `preUnit`, `releaseDate`, `nextRelease`, `date`, `number`, `keyNote`, `contact`). `D7G7` covers 1989 JAN to 2026 SEP.
- **Dataset pages**: 3,949 `dataset_landing_page` items, in `catalog/ons_datasets.tsv.gz` (title, uri, release date, summary): `peoplepopulationandcommunity` 2,169, `economy` 795, `employmentandlabourmarket` 498, `datasets` 337, `businessindustryandtrade` 149. A page's `current/data` lists its files, for example `mm23.csv`, `mm23.xlsx`, `mm23.csdb`.
- **CMD** (customise my data) `datasets`: 338 data sets, each with its dimensions in `catalog/ons.tsv` (id, type, title, frequency, national statistic, last updated, edition, version, dimensions, alert). `cantabular_flexible_table` 161 and `cantabular_multivariate_table` 126 are the census 2021; `filterable` 11, unmarked 39, `static` 1 hold the rest, for example `retail-sales-index` (`time`, `geography`, `unofficialstandardindustrialclassification`, `prices`, `seasonaladjustment`), `labour-market`, `gdp-to-four-decimal-places`, `regional-gdp-by-quarter`, `uk-spending-on-cards`. 33 carry an alert, for example `cpih01` and `trade`: "This page is no longer being updated".
- Other content types in search: `static_adhoc` 11,843, `release` 9,920, `static_foi` 8,570, `bulletin` 5,900, `visualisation` 3,372, `article` 3,296, `dataset` 1,945.
- UK and its regions.

## How to call it
- Any page: `https://www.ons.gov.uk/<uri>/data` gives its JSON.
- Search: `https://api.beta.ons.gov.uk/v1/search?content_type=timeseries&limit=1000&offset=0`, with `dataset_ids=MM23`, `uri_prefix=/economy/inflationandpriceindices`, `q=D7G7`. **Results stop at offset 10,000** (empty beyond). `sort=release_date` and `sort=relevance` found no extra data sets, so 4,680 series remain unlisted; use `q=` for a code.
- CMD: `https://api.beta.ons.gov.uk/v1/datasets?limit=1000`, then each `links.latest_version.href`, and `/dimensions`, `/options`. Topics: `/v1/topics`.
- `www.ons.gov.uk/robots.txt` is 404. `timeseriestool/data` and `datalist/data` are 404 now.

## What the app reads
Row `research_ons` (`src/SourcesPage.cpp:525`), loader `src/CountryData.cpp:1786`; keeps `months`, or `quarters`, or `years`, date and value. 29 uses in `resources/country_indicators.json:51` to `:955`, all still published in October 2026 or the last GDP round:
- Output `ihyq/qna`, `ihyr/qna`, saving `nrjs/ukea`; productivity `lzvb/prdy`.
- Labour `mgsx`, `mgrz`, `mgsc`, `mgwy`, `ap2y`, `kac3`, `lf24` in `lms`.
- Prices `d7g7`, `d7oe`, `d7bt`, `dko8`, `d7nn` in `mm23`.
- Trade `ikbj`, `ikbh`, `ikbi` in `mret`; balance of payments `hbop`, `aa6h` in `pnbp`; debt `hf6x/pusf`.
- Production `k222`, `k22a` in `diop`; retail `j5ek`, `j5eb` in `drsi`.

## Not read yet
`MGDP` monthly GDP; `PPI` producer prices; CPIH series in `MM23`; `PB` by-country balance of payments; `uk-spending-on-cards`; `online-job-advert-estimates`; `index-private-housing-rental-prices`, for Countries › United Kingdom.

## Calls made
Made on 2026-11-05, sequentially per host with a pause, `User-Agent: Deepwater/1.0`. 672 calls.

- `GET https://www.ons.gov.uk/robots.txt` → 404, 102.3 KB
- `GET https://api.beta.ons.gov.uk/v1/datasets?limit=1000` → 200, 471.1 KB
- `GET https://api.beta.ons.gov.uk/v1/datasets?limit=1000` → 200, 471.1 KB
- `GET https://www.ons.gov.uk/economy/inflationandpriceindices/timeseries/d7g7/mm23/data` → 200, 122.6 KB
- `GET https://www.ons.gov.uk/datalist/data?filter=datasets&size=50` → 404, 103.5 KB
- `GET https://www.ons.gov.uk/timeseriestool/data?query=&size=10&page=1` → 404, 103.5 KB
- `GET https://www.ons.gov.uk/economy/inflationandpriceindices/datasets/consumerpriceindices/data` → 200, 1.9 KB
- `GET https://api.beta.ons.gov.uk/v1/datasets/{id}/editions/{edition}/versions/{n}` × 341 → 200: 341, 1.3 MB in all; first `https://api.beta.ons.gov.uk/v1/datasets/cpih01`
- `GET https://api.beta.ons.gov.uk/v1` → 200, 1.9 KB
- `GET https://api.beta.ons.gov.uk/v1/search?content_type=timeseries&limit=3` → 200, 3.8 KB
- `GET https://api.beta.ons.gov.uk/v1/search?content_type=dataset_landing_page&limit=3` → 200, 15.5 KB
- `GET https://api.beta.ons.gov.uk/v1/topics` → 200, 4.5 KB
- `GET https://www.ons.gov.uk/economy/inflationandpriceindices/datasets/consumerpriceindices/current/data` → 200, 24.9 KB
- `GET https://api.beta.ons.gov.uk/v1/search?content_type=timeseries&dataset_ids=MM23&limit=2` → 200, 3.6 KB
- `GET https://api.beta.ons.gov.uk/v1/search?content_type=timeseries&q=D7G7&limit=2` → 200, 579 B
- `GET https://api.beta.ons.gov.uk/v1/search?content_type=timeseries&uri_prefix=/economy/inflationandpriceindices&limit=1` → 200, 970 B
- `GET https://api.beta.ons.gov.uk/v1/search?content_type=timeseries&limit=1&offset=9999` → 200, 3.1 KB
- `GET https://api.beta.ons.gov.uk/v1/search?content_type=timeseries&limit=1&offset=10000` → 200, 2.7 KB
- `GET https://api.beta.ons.gov.uk/v1/search?content_type=…&uri_prefix=…&limit=…&offset=…` × 122 → 200: 122, 36.6 MB in all; first `https://api.beta.ons.gov.uk/v1/search?content_type=timeseries&uri_prefix=/businessindustryandtrade/changestobusiness/mergersandacquisitions/timeseries&limit=1000&offset=0`
- `GET https://api.beta.ons.gov.uk/v1/search?content_type=…&dataset_ids=…&limit=…&offset=…` × 157 → 200: 157, 48.4 MB in all; first `https://api.beta.ons.gov.uk/v1/search?content_type=timeseries&dataset_ids=AM&limit=1000&offset=0`
- `GET https://api.beta.ons.gov.uk/v1/search?content_type=…&sort=…&limit=…&offset=…` × 20 → 200: 20, 7.2 MB in all; first `https://api.beta.ons.gov.uk/v1/search?content_type=timeseries&sort=release_date&limit=1000&offset=0`
- `GET https://api.beta.ons.gov.uk/v1/search?content_type=…&limit=…&offset=…&sort=…` × 15 → 200: 15, 6.7 MB in all; first `https://api.beta.ons.gov.uk/v1/search?content_type=timeseries&limit=1000&offset=0&sort=title`
