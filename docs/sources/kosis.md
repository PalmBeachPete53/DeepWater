---
id: kosis
name: KOSIS, Statistics Korea
rows: []
catalogue: ["Statistics Korea", "KOSTAT - Statistics Korea", "Statistics Korea (KOSTAT)"]
access: free key
probed: 2026-11-05
confidence: docs
---

# KOSIS, Statistics Korea

Korea's national statistics portal, for every statistics-producing agency, run by the renamed Ministry of Data and Statistics
(국가데이터처, `mods.go.kr`). The API needs a key, absent here, so the structure comes from the developer guide.

## Contents
From `openapi/index/index.jsp` (figures dated there 2023-06-08): **291,012 statistical tables**, 14 list views, 1,530
survey descriptions, 1,467 key indicators. The current count isn't stated.

List views (`vwCd`, from the guide): `MT_ZTITLE` domestic by theme, `MT_OTITLE` domestic by agency, `MT_GTITLE01`, `02`, `03`
e-regional indicators (theme, area, topic), `MT_CHOSUN_TITLE` pre-1945 (1908 to 1943), `MT_HANKUK_TITLE` statistical yearbook,
`MT_STOP_TITLE` discontinued statistics, `MT_RTITLE` international, `MT_RTITLE01` regional international, `MT_BUKHAN` North
Korea, `MT_TM1_TITLE` by target, `MT_TM2_TITLE` by issue, `MT_ETITLE` English KOSIS.

Fields of a list row: `VW_CD`, `VW_NM`, `LIST_ID`, `LIST_NM`, `ORG_ID`, `TBL_ID`, `TBL_NM`, `STAT_ID`, `SEND_DE`, `REC_TBL_SE`.

Fields of a data row: `ORG_ID`, `TBL_ID`, `TBL_NM`, `C1`..`C8` with `C*_OBJ_NM`, `C*_OBJ_NM_ENG`, `C*_NM`, `C*_NM_ENG` (up to eight
classifications), `ITM_ID`, `ITM_NM`, `ITM_NM_ENG`, `UNIT_ID`, `UNIT_NM`, `UNIT_NM_ENG`, `PRD_SE`, `PRD_DE`, `DT`, `LST_CHN_DE`.

## How to call it
- List: `GET https://kosis.kr/openapi/statisticsList.do?method=getList&apiKey=<key>&vwCd=MT_ZTITLE&parentId=&format=json&jsonVD=Y`.
- Data: `GET https://kosis.kr/openapi/Param/statisticsParameterData.do?method=getList&apiKey=<key>&orgId=<org>&tblId=<tbl>&objL1=ALL&itmId=ALL&prdSe=M&startPrdDe=202601&endPrdDe=202609&format=json&jsonVD=Y`.
- Others in the guide: bulk data, statistics descriptions, table descriptions, integrated search, key indicators (`devGuide_030101`, `0401`,
  `060101`, `0701`, `080101`). JSON and SDMX.
- `openapi/openApiCodeList.do` downloads the Excel code list (344 KB), and `file/index/openStateExpl.xls` the open state; both were
  downloaded, not parsed (no Excel reader used).
- Traps: no key gives **200** with `{"err":"10","errMsg":"인증KEY값이 누락되었습니다."}`. The notices of 2026-02-05 and 2026-07-09
  announce a per-minute call limit and the end of HTTP.

## What the app reads
Nothing: no row in `src/SourcesPage.cpp`. South Korean data come through the Bank of Korea (`docs/sources/bok.md`), which
republishes many of them.

## Not read yet
With a key: population, labour (economically active population survey), household income and spending, industrial activity,
service production, which KOSIS holds at the most detail and by region; North Korea under `MT_BUKHAN`. Table IDs not read.

## Calls made
- `GET https://kosis.kr/robots.txt` → 200, 223 B (allows all but Googlebot's search)
- `GET https://kosis.kr/openapi/statisticsList.do?method=getList&format=json&jsonVD=Y&vwCd=MT_ZTITLE&parentListId=` (no key) → 200, `err` 10
- `GET https://kosis.kr/openapi/index/index.jsp` → 200, 21 KB
- `GET https://kosis.kr/eng/` → 200, 51 KB
- `GET https://kosis.kr/openapi/devGuide/devGuide_0101List.do` → 200, 46 KB
- `GET https://kosis.kr/openapi/devGuide/devGuide_0201List.do` → 200, 93 KB
- `GET https://kosis.kr/openapi/openApiCodeList.do` → 200, 344 KB (Excel)
- `GET https://kosis.kr/openapi/file/index/openStateExpl.xls` → 200, 154 KB
