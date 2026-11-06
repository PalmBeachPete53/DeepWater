---
id: metacopier
name: MetaCopier
rows: []
catalogue: []
access: paid
probed: 2026-11-06
confidence: docs
---

# MetaCopier

A paid cloud service that holds connections to MetaTrader and other trading accounts and exposes them over a REST API. Not a data source: the MetaTrader page sends orders through it.

## Contents
The accounts of a key: their information (balance, equity, currency, `environment` LIVE or DEMO, synced every minute), symbols (`lotSize`, `digits`, `minimalVolume`, `maximalVolume`, `stepVolume`, base and quote currency), quotes (`bid`, `ask`), positions. No tick value on a symbol.

## How to call it
Spec: `https://api.metacopier.io/rest/api/documentation/v3/api-docs` (OpenAPI, 183 paths). Auth: header `X-API-KEY`, a project's key or an account's. Trading calls on the account's regional host (`api-newyork`, `api-london`, `api-berlin`, `api-singapore`.metacopier.io), others on `api.metacopier.io`. `POST /rest/api/v1/accounts/{id}/positions` takes `symbol`, `orderType`, `volume` in lots, `openPrice`, `stopLoss`, `takeProfit`, `requestId` (0 to 999, against double sends), `comment`; it answers 204 with nothing, and the position's comment gets an `API|<requestId>|` prefix. Risk in percent exists only as a PRO feature on copiers (`FeatureRiskPerTradeDTO`). No rate limit stated.

## What the app reads
`src/broker/MetaCopierClient.cpp`: accounts, information, symbols, quote, positions; open and close a position, close all. **Written from the spec and tested against a fake server (tests/quant/broker); not yet run against the real service.**

## Not read yet
Copiers and strategies, risk limits, the TradingView webhook, the socket API, the news and economic calendar.

## Calls made
`GET https://api.metacopier.io/rest/api/documentation/v3/api-docs` → 200, 773 KB. `GET https://api-london.metacopier.io/rest/api/documentation/v3/api-docs` → 200, 48 KB. `GET https://docs.metacopier.io/rest-api/api` → 200.
