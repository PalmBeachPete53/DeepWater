---
id: anthropic
name: Anthropic API
rows: [anthropic]
catalogue: []
access: paid
probed: 2026-11-05
confidence: docs   # no key was used and no call was made: from the app's code and Anthropic's API reference as bundled with Claude Code on this date (cached 2026-10-06)
---

# Anthropic API

The Claude models' API. It is a service the app sends text to, not a data source: nothing here feeds a table or a chart.

## Contents
Endpoints the app calls:
- `POST /v1/messages`: streamed conversation.
- `GET /v1/models`: `data[]`, the app keeps `id` and `display_name`. The reference also lists `created_at`, `max_input_tokens`, `max_tokens`, `capabilities`.

Other endpoints in the reference: `POST /v1/messages/count_tokens`, `POST /v1/messages/batches`, `POST /v1/files`, `GET /v1/models/{id}`, Managed Agents under `/v1/agents`.

## How to call it
Base `https://api.anthropic.com`. Headers: `x-api-key`, `anthropic-version` (the app sends `2026-10-21`), `content-type: application/json`. Key from console.anthropic.com, in Sources › AI.

## What the app reads
Row `anthropic`, `src/SourcesPage.cpp:824`. Key check: `GET /v1/models?limit=1` `src/SourcesPage.cpp:1444`, also `src/ClaudeClient.h:41`.
- Sends `src/ClaudeClient.cpp:51-68`: `model`, `max_tokens`, `stream: true`, `messages`, `system`, `output_config.effort`, `thinking: {type: "adaptive", display: "summarized"}`; on models that take it, `fallbacks: "default"` with header `anthropic-beta: server-side-fallback-2026-07-01`.
- Reads the event stream `src/ClaudeClient.cpp:112-134`: `message_start` (`message.model`, `usage.input_tokens`, `usage.cache_read_input_tokens`), deltas `text` and `thinking`, `message_delta` (`stop_reason`, `usage.output_tokens`), `error.message`.
- Models `src/ClaudeClient.cpp:151-169`; built-in list if none answers (`knownModels`, `:28`).
- Used from `src/AiPage.cpp`. Project notes say Claude in a slot gets no data from the app, as the user decided.

## Not read yet
Token counting before a send; prompt caching (`cache_control`), of which the app already reads the usage field; the models' `max_input_tokens` and `capabilities`, to size or grey out the picker.

## Calls made
None. The key was not read, as instructed.
