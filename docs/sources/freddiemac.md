---
id: freddiemac
name: Freddie Mac
rows: []
catalogue: ["Freddie Mac"]
access: free, no key                  # the loan-level data set wants a free account and a licence
probed: 2026-11-05
confidence: called
---

# Freddie Mac

US mortgage rates (the weekly PMMS), a house price index, and the loan-level data of its book.

## Contents
- **PMMS, Primary Mortgage Market Survey**: `/pmms/docs/PMMS_history.csv`, columns `date, pmms30, pmms30p, pmms15, pmms15p, pmms51,
  pmms51p, pmms51m, pmms51spread` (rate and points for 30-year, 15-year, 5/1 ARM), weekly from 4/2/1971; `/pmms/docs/historicalweeklydata.xlsx`,
  sheet `Full History`; `PMMS_Readme.txt`; `PMMS_Publication_Calendar.pdf`; archive at `/pmms/pmms_archives`.
- **FMHPI**: `/fmac-resources/research/docs/fmhpi_master_file.csv`, columns `Year, Month, GEO_Type, GEO_Name, GEO_Code, Index_NSA, Index_SA`.
  `GEO_Type`: `US` (1), `State` (51), `CBSA` (387). Monthly, 1975-01 to 2026-09, 272,619 rows. `fmhpi.txt` and a technical description.
- **Single Family Loan-Level Dataset**: `/research/datasets/sf-loanlevel-dataset`. Two files, Origination and Monthly Performance;
  layout in `file_layout_july_2026.xlsx` (credit score, MSA, occupancy, original UPB, property type, loan purpose, seller, current UPB,
  loan age, modification, current rate, recoveries, costs, ...). The page links a licensing agreement; the data was not downloaded, only the layout was read.
- **COFI**: `/fmac-resources/research/docs/monthly_cofi.xlsx`, Federal Cost of Funds Index, on `/research/datasets/cofi`.
- `/research/datasets` lists these. `/research/indices`, `/research/insight` hold notes.

## How to call it
`GET https://www.freddiemac.com/pmms/docs/PMMS_history.csv`. No key. `robots.txt` excludes `/pmms/pmmsthin.html`, `/pmms/pmmsthick.html` and
the data request form, not the files. `PMMS_Readme.txt` is UTF-16. Trap: the file layout's name carries a date.

## What the app reads
Nothing directly. `MORTGAGE30US` comes through FRED: `src/EconomyData.cpp:259`, `resources/country_indicators.json:833`.

## Not read yet
- PMMS 15-year and 5/1 columns, without FRED's key, for Economy › Housing.
- FMHPI by state and CBSA: for a US states map; the file is 16 MB.

## Calls made
- `GET https://www.freddiemac.com/robots.txt` → 200, 2,302 bytes
- `GET https://www.freddiemac.com/research/datasets` → 200, 83,654 bytes
- `GET https://www.freddiemac.com/pmms` → 200, 102,456 bytes
- `GET https://www.freddiemac.com/pmms/pmms_archives` → 200, 88,964 bytes
- `GET https://www.freddiemac.com/pmms/docs/PMMS_history.csv` → 200, 97,591 bytes
- `GET https://www.freddiemac.com/pmms/docs/historicalweeklydata.xlsx` → 200, 218,880 bytes
- `GET https://www.freddiemac.com/research/indices/house-price-index` → 200, 106,911 bytes
- `GET https://www.freddiemac.com/fmac-resources/research/docs/fmhpi_master_file.csv` → 200, 16,472,112 bytes
- `GET https://www.freddiemac.com/research/datasets/sf-loanlevel-dataset` → 200, 89,205 bytes
- `GET https://www.freddiemac.com/research/datasets/multifamily-loan-performance-database` → 404, 74,870 bytes
- `GET https://www.freddiemac.com/research/insight` → 200, 94,079 bytes
- `GET https://www.freddiemac.com/investors/monthly-volume-summary` → 404, 74,135 bytes
- `GET https://www.freddiemac.com/fmac-resources/research/pdf/file_layout_july_2026.xlsx` → 200, 13,753 bytes
- `GET https://www.freddiemac.com/research/datasets/cofi` → 200, 85,496 bytes
- `GET https://www.freddiemac.com/pmms/docs/PMMS_Readme.txt` → 200, 2,830 bytes
