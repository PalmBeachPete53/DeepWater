---
id: estat
name: e-Stat and the Statistics Bureau of Japan
rows: [research_estat]
catalogue: ["Statistics Bureau of Japan"]
access: free key
probed: 2026-11-05
confidence: docs
---

# e-Stat and the Statistics Bureau of Japan

Japan's portal of government statistics, from every ministry. The API needs an application ID, absent here, so the structure
comes from the public portal pages and the manual, not from `getStatsList`.

## Contents
From `www.e-stat.go.jp/stat-search/database` (2026-11-05): **227,205 database tables** and 1,498,184 files. 4,545 pages of
data sets at the default page size. Its fields (`bunya_l`):

`01` 国土・気象 land and weather, `02` 人口・世帯 population, `03` 労働・賃金 labour and wages, `04` 農林水産業 agriculture, `05`
鉱工業 mining and manufacturing, `06` 商業・サービス業 commerce and services, `07` 企業・家計・経済 firms, households, economy, `08`
住宅・土地・建設 housing and construction, `09` エネルギー・水 energy and water, `10` 運輸・観光 transport and tourism, `11` 情報通信
ICT and science, `12` 教育・文化 education, `13` 行財政 government finance, `14` 司法・安全・環境 justice and environment, `15` 社会保障
social security, `16` 国際 international, `99` others.

By publisher: the portal's filter lists Cabinet Office, Internal Affairs, Finance, National Tax Agency, MEXT, MHLW, MAFF, METI,
MLIT, Environment and their agencies.

Table the app reads, seen through the public view (`dbview?sid=0004052037`): 消費者物価指数 2025年基準, **published
2026-10-30**, four axes: 表章項目 3 (values), 2025年基準品目 797 items, 地域 67 areas, 時間軸 794 periods (years and months).

## How to call it
- Base: `https://api.e-stat.go.jp/rest/3.0/app/json/`. Application ID required (`appId`, free on registration).
- From the manual (`api-info/e-stat-manual3-0`), seven functions: `getStatsList` (table list), `getMetaInfo` (axes and codes),
  `getStatsData`, `postDataset`, `refDataset`, `getDataCatalog`, `getStatsDatas` (bulk). One call gives at most 100,000 values.
- Data: `getStatsData?appId=<id>&statsDataId=0004052037&cdTab=3&cdCat01=0001&cdArea=00000&metaGetFlg=N&cntGetFlg=N&limit=100000`.
  Answer `GET_STATS_DATA.STATISTICAL_DATA.DATA_INF.VALUE`, cells `@time`, `$`.
- Trap: a missing or wrong ID answers **200** with `RESULT.STATUS` 100 and a message, not an HTTP error. `@time` is `YYYY00MMMM` for
  months, `YYYY000000` for years.
- `www.e-stat.go.jp/dbview?sid=<id>` shows a table's axes without a key (3 MB of HTML).

## What the app reads
`research_estat`, row at `src/SourcesPage.cpp:588`; Countries › Japan, `src/CountryData.cpp:1454`, keeps `@time` and `$`. Provider
`estat` in `resources/country_indicators.json`, table `0004052037`: CPI level and % over a year (`cdCat01` `0001`), seasonally
adjusted monthly change (`0901`), less fresh food (`0161`), general services (`0228`), less fresh food and energy (`0178`),
all with `cdArea=00000`.

## Not read yet
With the ID, `getStatsList` by field:
- Labour Force Survey, Monthly Labour Survey, Family Income and Expenditure Survey, Retail Trade, Industrial Production (METI), Housing
  Starts (MLIT): these are the survey names of `03`, `06`, `05`, `07` and `08` as commonly known; their table IDs were not read.

## Calls made
- `GET https://api.e-stat.go.jp/rest/3.0/app/json/getStatsList?limit=1` (no ID) → 200, 304 B, `STATUS` 100
- `GET https://www.e-stat.go.jp/api/api-info/e-stat-manual3-0` → 200, 359 KB
- `GET https://www.e-stat.go.jp/stat-search/database?page=1&layout=dataset` → 200, 336 KB
- `GET https://www.e-stat.go.jp/dbview?sid=0004052037` → 200, 3.2 MB
- `GET https://data.e-stat.go.jp/lodw/sparql?query=SELECT%20%2A%20WHERE%20%7B%3Fs%20%3Fp%20%3Fo%7D%20LIMIT%201&output=json` → 404
