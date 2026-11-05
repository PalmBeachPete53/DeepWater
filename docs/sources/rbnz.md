---
id: rbnz
name: Reserve Bank of New Zealand
rows: []
catalogue: ["Reserve Bank of New Zealand"]
access: scraped web page
probed: 2026-11-05
confidence: refused
---

# Reserve Bank of New Zealand

New Zealand's central bank. Its site answered with a "Website unavailable" page behind Cloudflare, so its statistics could not be listed.

## Contents
Nothing observed first-hand.

From general knowledge, not from a call: the Bank's statistics are XLSX series by letter, `B` interest rates and exchange rates, `C` lending, `D`
deposits, `M` economic indicators and the Survey of Expectations. Treat it as a lead only.

## How to call it
`www.rbnz.govt.nz/statistics` answers 403 with a 31 KB page. Its `robots.txt` answers 200: `User-agent: *` allows all, with `Content-Signal:
search=yes,ai-train=no,use=reference`, a reservation of rights under EU Directive 2019/790 article 4, and a long list of crawlers blocked by name, AI ones
among them (`ClaudeBot`, `GPTBot`, `anthropic-ai`). So a program isn't welcome even where the file allows it. `src/DeskViewsMacro.cpp:551` already
records the refusal.

## What the app reads
Nothing from the RBNZ. Desk › Rates takes New Zealand's yields from OECD KEI (`src/DeskViewsMacro.cpp:551`), meeting dates are bundled
(`src/DeskData.cpp:62`).

## Not read yet
Unknown. BIS and OECD carry New Zealand's policy rate and some yields.

## Calls made
- `GET https://www.rbnz.govt.nz/statistics` → 403, 31 KB
- `GET https://www.rbnz.govt.nz/robots.txt` → 200, 2.7 KB
