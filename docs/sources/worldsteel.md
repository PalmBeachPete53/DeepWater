---
id: worldsteel
name: World Steel Association
rows: []
catalogue: ["World Steel Association"]
access: paid
probed: 2026-11-05
confidence: called
---

# World Steel Association

Steel production, use and trade. A yearly free report; monthly and detailed data are by subscription, and the viewer asks for a login.

## Contents
Data pages (`/data/`): Steel Data Viewer ("Monthly crude steel production data"), Annual steel data ("Production, trade, and use"), Our subscriptions, World Steel in Figures, Top steel producers, Steel impact, Steel use by sector, Benchmarking systems, About our data, Sustainability Indicators, Safety and health data report. The viewer and annual pages show "Login | worldsteel members | Subscribe" and no data in the text read.

**World Steel in Figures 2026** (free, read online or PDF) by chapter:
- Crude steel production: world 1950 to 2025; top 50 companies 2025; major countries 2024 and 2025; by process 2025; continuously-cast 2023 to 2025; monthly 2022 to 2025.
- Steel use: geographical distribution 2015 and 2025; apparent use and per capita 2021 to 2025.
- Raw materials: pig iron 2024, 2025; direct reduced iron 2021 to 2025; iron ore 2024; world iron ore trade 2025; ferrous scrap 2024, 2025.
- Trade: steel trade in products 1975 to 2025; volume of trade 2000 to 2025; exports by product 2021 to 2025; trade by area 2025; major importers and exporters 2025.
- Indirect trade 2000 to 2024; major indirect importers and exporters 2024; true steel use 2020 to 2024; apparent and true use per capita 2024.

Shop (`Data` subject, 9 items): Monthly crude steel and iron production statistics, subscription, €1,590; Stainless Steel in Figures 2026, subscription, €599; World Steel in Figures, 2024, 2025 and 2026, free; Indirect Trade in Steel (2014 to 2024), €5,900; Steel Statistical Yearbook 2025, subscription, €730; older reports.

## How to call it
- `https://worldsteel.org/data/` and its pages; the report at `/data/world-steel-in-figures/world-steel-in-figures-2026/`. No API found.
- `robots.txt` allows these pages and closes `/extranet-asset/`, `/wp-admin/` and one yearbook PDF by name, `/wp-content/uploads/Steel-Statistical-Yearbook-2021.pdf`: respect it.

## What the app reads
Nothing.

## Not read yet
**Map › supply chains:** the free report's production and trade tables, one yearly fetch and PDF parsing. The monthly press-release figures were not looked at.

## Calls made
All on 2026-11-05 with `User-Agent: Deepwater/1.0`.
- `GET https://worldsteel.org/robots.txt` → 200, 2,251 bytes.
- `GET https://worldsteel.org/data/` → 200, 137,344 bytes.
- `GET https://worldsteel.org/data/steel-data-viewer/` → 200, 128,162 bytes.
- `GET https://worldsteel.org/data/annual-production-steel-data/` → 200, 127,262 bytes.
- `GET https://worldsteel.org/data/world-steel-in-figures/world-steel-in-figures-2026/` → 200, 430,124 bytes.
- `GET https://worldsteel.org/publications/bookshop/?filter_publication-subject=steel-data-and-statistics` → 200, 153,515 bytes.
