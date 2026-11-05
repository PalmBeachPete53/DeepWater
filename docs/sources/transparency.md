---
id: transparency
name: Transparency International
rows: []
catalogue: ["Transparency International"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Transparency International

The Corruption Perceptions Index: a yearly score and rank per country, published as an Excel file and a report. No API found.

## Contents
One data set, one workbook a year. `CPI2025_Results.xlsx` holds six sheets:

| Sheet | Size | Columns |
|---|---|---|
| `CPI2025` | A1:Z186, about 182 countries | Country / Territory, ISO3, Region, CPI 2025 score, Rank, Standard error 2025, Number of sources, Lower CI, Upper CI, then the 13 source columns |
| `CPI Timeseries 2012 - 2025` | A1:BB186 | Country, ISO3, Region, then CPI score, Rank, Sources, Standard error for each year from 2025 back to 2012 |
| `CPI 2025 Regional Averages` | A1:V32 | Region, average score and N per year from 2025 back |
| `Significant Changes since 2012 ` | A1:P85 | Country, ISO3, Region, CPI2025, Reference Year, score then, Direction of Change, Change, Category |
| `CPI 2025 Trends` | A1:J186 | Country, ISO3, Region, scores 2025, 2024, 2021, 2016, and the 1, 5 and 10-year change |
| `CPI Historical` | A1:X2498, 2,498 rows | Country, ISO3, Year, Region, CPI score, Rank, Standard error, Number of sources, Lower CI, Upper CI, sources |

Source columns: African Development Bank CPIA, Bertelsmann Sustainable Governance, Bertelsmann Transformation Index, Economist Intelligence Unit, Freedom House Nations in Transit, Global Insights, IMD World Competitiveness Yearbook, PERC Asia Risk Guide, PRS International Country Risk Guide, Varieties of Democracy, World Bank CPIA, World Economic Forum EOS, World Justice Project Rule of Law Index; the historical sheet adds the Bribe Payers survey.

The page's year selector runs from 2025 back to at least 2001 (seen on the 2024 page). Rhythm: annual, the 2025 file embargoed until 10 February 2026.

## How to call it
- Page: `https://www.transparency.org/en/cpi/2025`, links to the workbook, methodology and maps.
- Data: `https://images.transparencycdn.org/images/CPI2025_Results.xlsx`. The name changes (`CPI2024-Results-and-trends.xlsx` the year before): take it from the page.
- `robots.txt` leaves these pages open. The earlier probe of `/en` got a 403 (`catalog/bodies_probe.jsonl.gz`); the calls below answered.

## What the app reads
Nothing.

## Not read yet
**Economy › Countries, Government tab:** `CPI Historical` gives score and rank by country and year in one sheet.

## Calls made
All on 2026-11-05 with `User-Agent: Deepwater/1.0`.
- `GET https://www.transparency.org/robots.txt` → 200, 745 bytes.
- `GET https://www.transparency.org/en/cpi/2024` → 200, 326,710 bytes.
- `GET https://www.transparency.org/en/cpi/2025` → 200, 324,619 bytes.
- `GET https://images.transparencycdn.org/images/CPI2025_Results.xlsx` → 200, 3,071,002 bytes; sheet names, sizes and header text kept.
