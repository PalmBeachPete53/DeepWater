---
id: nbschina
name: National Bureau of Statistics of China
rows: []
catalogue: ["National Bureau of Statistics of China"]
access: scraped web page
probed: 2026-11-05
confidence: refused
---

# National Bureau of Statistics of China

China's national statistics. The classic database route answered 403 from a WAF, the new database is a JavaScript application with no public API, and only the
release pages on `www.stats.gov.cn` answer. The catalogue's "data.stats.gov.cn easyquery (JSON), no key" no longer holds from here.

## Contents
Observed on `www.stats.gov.cn/sj/`, the site's data section:
- `zxfb/` releases (数据发布), one HTML page per release: GDP, PMI, prices of production materials, industrial profits, culture firms' revenue; dated `t20261103_...`.
- `ndsj/` China Statistical Yearbook, 18 editions from 2008 to 2025 linked, each with `indexch.htm`.
- `tjgb/` communiqués: annual (`ndtjgb`), and for each census (`rkpcgb`, `jjpcgb`, `nypcgb`, `gypcgb`, `scpcgb`, `jbdwpcgb`, `rdpcgb`, `qttjgb`).
- `pcsj/` census data: population, basic units; HTML and PDF.

English home (`www.stats.gov.cn/english/`) links Monthly, Quarterly, Annual, Census, Other Data and Yearbook under `Statisticaldata/`. Those two paths tried answered a
stub of about 200 bytes, the content is loaded by a script. It also links `data.stats.gov.cn/dg/website/page.html#/pc/national/en/home` as "Statistical Database".

From general knowledge, not from a call: the database held `hgyd` (national monthly), `hgjd` (quarterly), `hgnd` (annual), `fsyd`, `fsjd`, `fsnd` (by province), `csyd`
(cities), by a tree of indicators (`zb`), with `wds` and `dfwds` as query. Treat it as a lead only.

## How to call it
- `GET https://data.stats.gov.cn/easyquery.htm?m=getTree&dbcode=hgyd&wdcode=zb&id=` → 403, `reason:UrlACL`: the WAF forbids that URL.
- `data.stats.gov.cn/english/` → 404 JSON. `data.stats.gov.cn/dg/website/page.html` → 200, a low-code application (`dsf`) whose scripts load on demand. Its config
  (`common/config_project.js`) names roots `/dg/chart/`, `/dg/website/`. No API documented, and no attempt was made further.
- `www.stats.gov.cn` answers normally: HTML release pages.

## What the app reads
Nothing: no row in `src/SourcesPage.cpp`. China reaches the Map through IMF, BIS and the World Bank, some of them from NBS.

## Not read yet
Release pages could be parsed for headline figures (`zxfb/` by date), which is scraping. A user with a browser can reach the new database.

## Calls made
- `GET https://data.stats.gov.cn/easyquery.htm?m=getTree&dbcode=hgyd&wdcode=zb&id=` → 403, 291 B, `reason:UrlACL`
- `GET https://data.stats.gov.cn/english/` → 404, 145 B
- `GET https://data.stats.gov.cn/dg/website/page.html` → 200, 3.2 KB
- `GET https://data.stats.gov.cn/dg/website/common/config_project.js` → 200, 7.8 KB
- `GET https://www.stats.gov.cn/sj/` → 200, 95 KB
- `GET https://www.stats.gov.cn/sj/ndsj/` → 200, 79 KB
- `GET https://www.stats.gov.cn/english/` → 200, 25 KB
- `GET https://www.stats.gov.cn/english/Statisticaldata/` → 200, 148 B
- `GET https://www.stats.gov.cn/english/Statisticaldata/MonthlyData/` → 200, 209 B
