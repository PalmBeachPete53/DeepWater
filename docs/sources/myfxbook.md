---
id: myfxbook
name: Myfxbook
rows: [myfxbook]
catalogue: []
access: free account
probed: 2026-11-05
confidence: docs   # no account was used: the API page and the no-session answer were called; the outlook fields come from its documentation and the app's parser
---

# Myfxbook

A site where retail traders link their forex accounts. Its API gives the community's long and short positioning by symbol, and the logged-in user's own accounts.

## Contents
The API page lists 13 methods: `login`, `logout`, `get-my-accounts`, `get-watched-accounts`, `get-open-orders`, `get-open-trades`, `get-history`, `get-daily-gain`, `get-data-daily`, `get-gain`, `get-custom-widget`, `get-community-outlook`, `get-community-outlook-by-country`.

Parameters shown by the page's examples:
- `session`: `get-my-accounts`, `get-watched-accounts`, `get-community-outlook`, `logout`.
- `session`, `id`: `get-open-orders`, `get-open-trades`, `get-history` (the page says the history is limited to the last 50 transactions).
- `session`, `id`, `start`, `end`: `get-daily-gain`, `get-data-daily`, `get-gain`.
- `session`, `symbol`: `get-community-outlook-by-country`, with a `countries` list and `code`.
- `email`, `password`: `login`.

`get-community-outlook` (documentation's example):
- `error`, `message`.
- `symbols[]`: `name`, `shortPercentage`, `longPercentage`, `shortVolume`, `longVolume`, `longPositions`, `shortPositions`, `totalPositions`, `avgShortPrice`, `avgLongPrice`.
- `general`: `demoAccountsPercentage`, `realAccountsPercentage`, `profitablePercentage`, `nonProfitablePercentage`, `fundsWon`, `fundsLost`, `averageDeposit`, `averageAccountProfit`, `averageAccountLoss`, `totalFunds`.

Symbols: 90 on the user's first real answer, per the project's notes of 2026-11-05 (not recounted here). Only a current snapshot; no history endpoint for the outlook.

## How to call it
- Base `https://www.myfxbook.com/api/`, methods as `<method>.json`, a free account.
- `GET /api/login.json?email=&password=` gives a `session`, which then travels in every URL. Both are secrets in the URL: the app percent-encodes them by hand and keeps them out of error messages (`src/Myfxbook.cpp:17-21`, `:58`, `:136`).
- Limit on the API page: free version 100 requests per 24 hours for `get-community-outlook`; paid 2880 in the methods table, 2800 in the page's changelog.
- Without a session, `get-community-outlook.json` answers HTTP 200 with `{"error": true, "message": ...}`: the status doesn't tell failures apart.
- Terms: `https://www.myfxbook.com/terms` answered 403 to this call, so **they were not read**.

## What the app reads
Row `myfxbook`, `src/SourcesPage.cpp:639`. Client: `src/Myfxbook.cpp`, at most one request per 30 minutes (`src/Myfxbook.cpp:13`).
- `login.json` `src/Myfxbook.cpp:54`, field `session` (`src/desk/Positioning.cpp:88`). `logout.json` `src/Myfxbook.cpp:82`.
- `get-community-outlook.json` `src/Myfxbook.cpp:118`, parsed in `src/desk/Positioning.cpp:30`: `symbols[]` (or `outlook[]`) `name`, `longPercentage`, `shortPercentage`, `longVolume`, `shortVolume`, `longPositions`, `shortPositions`, `avgLongPrice`, `avgShortPrice`; `general.demoAccountsPercentage`. An `error` containing "session" drops the session.
- Shown in Desk › Positioning; `longPercentage` is noted daily in Desk history as `crowd/<symbol>` (`src/DeskHistory.cpp:103`).
- The answer is cached as `cache_myfxbook_outlook.bin` (`src/Myfxbook.cpp:15`).

## Not read yet
- `get-community-outlook-by-country`: positioning by country for one symbol, for Desk › Positioning.
- `general`'s `profitablePercentage`, `fundsWon`, `fundsLost`, `averageDeposit`: how the crowd is faring, beside the positioning.
- `totalPositions`.
- The user's own `get-history`, `get-open-trades`, `get-daily-gain` for linked accounts: a way to fill the Journal without a statement file.

## Calls made
- `GET https://www.myfxbook.com/robots.txt` → 403, 5383 bytes.
- `GET https://www.myfxbook.com/api/get-community-outlook.json` → 200, 52 bytes.
- `GET https://www.myfxbook.com/api` → 200, 365114 bytes.
- `GET https://www.myfxbook.com/terms` → 403, 5368 bytes.

The `session` shown in the documentation's example was not used. No login was attempted.
