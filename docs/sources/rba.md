---
id: rba
name: Reserve Bank of Australia
rows: []
catalogue: ["Reserve Bank of Australia"]
access: scraped web page
probed: 2026-11-05
confidence: refused
---

# Reserve Bank of Australia

Australia's central bank. Its site answered "Access Denied" (Akamai) to this inventory, on the tables index and on a data file. Its
statistical tables could not be listed.

## Contents
Nothing observed first-hand.

From general knowledge, not from a call: the RBA's Statistical Tables are CSV and XLSX files by letter, for example `A` Reserve Bank, `B`
banks, `C` credit and charge cards, `D` money and credit, `E` household and business balance sheets, `F` interest rates (`F1` money market, `F2`
government bond yields, `F11` exchange rates, `F17` zero-coupon curve), `G` inflation and inflation expectations, `H` economic activity,
`I` external, `J` Statement on Monetary Policy. The file names `csv/f1-data.csv` and `xls/f02hist.xls` are known to exist for browsers.

## How to call it
No route for a program. A 403 came on `www.rba.gov.au/statistics/tables/` and `.../csv/f1-data.csv`. The body probe of 2026-11-05 got the
same (`catalog/bodies_probe.jsonl.gz`), and `src/DeskViewsMacro.cpp:551` already records "the central banks refuse programs".

## What the app reads
Nothing from the RBA. Desk › Rates fills Australia's yields from OECD KEI (`src/DeskViewsMacro.cpp:551`), and meeting dates are bundled
(`desk::bundledMeetings`, `src/DeskData.cpp:62`). Countries › Australia reads the ABS: `docs/sources/abs.md`.

## Not read yet
Unknown, for the reason above. A user could download the files by hand. BIS also carries Australian policy rates and credit.

## Calls made
- `GET https://www.rba.gov.au/statistics/tables/` → 403, Access Denied, 394 B
- `GET https://www.rba.gov.au/statistics/tables/csv/f1-data.csv` → 403, Access Denied, 421 B
