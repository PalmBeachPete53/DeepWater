---
id: huggingface
name: Hugging Face Hub
rows: [huggingface]
catalogue: []
access: free, no key
probed: 2026-11-05
confidence: called
---

# Hugging Face Hub

A catalogue of machine-learning models, data sets and demo apps, with their files. The app uses it to find and download local models. A token is optional and opens gated repositories.

## Contents
**`GET /api/models`** (a list, no `lastModified` or `gated` by default): `_id`, `id`, `modelId`, `likes`, `private`, `downloads`, `tags` (list), `pipeline_tag`, `library_name`, `createdAt`; `trendingScore` when no search is given.

**`GET /api/models/<id>?blobs=true`** (one model, 484 files in the sample): the above plus `author`, `sha`, `lastModified`, `gated`, `disabled`, `model-index`, `config`, `cardData`, `gguf` (`architecture`, `total`, `totalFileSize`), `spaces`, `usedStorage`, and `siblings[]`.
- `siblings[]`: `rfilename`, `blobId`, `size`; large files add `lfs` {`sha256`, `size`, `pointerSize`}.
- `cardData` in the sample: `license`, `library_name`, `pipeline_tag`, `tags`, `base_model`, `base_model_relation`.

**`GET /<id>/resolve/main/<file>`**: the file, redirected to a cached URL.

**`GET /api/whoami-v2`**: without a token, 401 `{"error": ...}`.

**Also there, not used:**
- `GET /api/datasets`: `_id`, `id`, `author`, `disabled`, `gated`, `lastModified`, `likes`, `trendingScore`, `private`, `sha`, `description`, `downloads`, `tags`, `createdAt`, `key`.
- `GET /api/spaces`: `_id`, `id`, `likes`, `trendingScore`, `private`, `sdk`, `tags`, `createdAt`.

## How to call it
No key for public repositories; `Authorization: Bearer <token>` for gated ones. Query of the search: `search`, `limit`, `sort`, `direction`. robots.txt: `Allow: /`. A download resumes with `Range`.

## What the app reads

Corrected on 5 November 2026: the app's search now asks for `gated` and `lastModified` with `expand[]`.
Row `huggingface`, `src/SourcesPage.cpp:826`. Client: `src/AiModels.cpp`, User-Agent `Deepwater/0.1`.
- Search `src/AiModels.cpp:44-77`: `search`, `limit=30`, `sort=downloads`, `direction=-1`; keeps `id`, `pipeline_tag`, `library_name`, `downloads`, `likes`, `gated`, `lastModified` or `createdAt`.
- Files `src/AiModels.cpp:80-105`: `siblings[].rfilename`, `size`, `lfs.sha256`.
- Download `src/AiModels.cpp:172`, into `<app data>/models`, checked against the SHA-256 when given.
- Token check `src/SourcesPage.cpp:1445`; probe `src/AiModels.h:61`. Opens a model's page `src/AiPage.cpp:216`.

**Mismatch:** the search reads `gated` and `lastModified` (`src/AiModels.cpp:71-72`), but this list answer doesn't carry them. So `gated` stays false in the search and the date falls back to `createdAt`, unless the API adds them on another query. The real status only shows when the files are listed (401 or 403). `expand[]=gated&expand[]=lastModified` is the usual way to ask for them; not tested here.

## Not read yet
- `gguf` and `tags` of a model: architecture and total size before listing its files, and filtering to what llama.cpp can run.
- `cardData.license`, for showing a licence before a download.
- `/api/datasets`: data sets for AI › ML.

## Calls made
- `GET https://huggingface.co/robots.txt` → 200, 68 bytes.
- `GET https://huggingface.co/api/models?search=gguf&limit=2&sort=downloads&direction=-1` → 200, 1072 bytes.
- `GET https://huggingface.co/api/models?limit=1` → 200, 490 bytes.
- `GET https://huggingface.co/api/whoami-v2` → 401, 41 bytes.
- `GET https://huggingface.co/api/models/audio-cpp/audio.cpp-gguf?blobs=true` → 200, 116913 bytes.
- `HEAD https://huggingface.co/audio-cpp/audio.cpp-gguf/resolve/main/README.md` → 200, Content-Length 13156.
- `GET https://huggingface.co/api/datasets?limit=1` → 200, 774 bytes.
- `GET https://huggingface.co/api/spaces?limit=1` → 200, 238 bytes.

The HEAD above was to a 13 KB README, no model file was downloaded.
