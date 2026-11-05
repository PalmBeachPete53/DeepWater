---
id: github
name: GitHub
rows: [github]
catalogue: []
access: free key
probed: 2026-11-05
confidence: called
---

# GitHub

The app's own updates: the release `latest-main` of its private repository, which holds the latest successful build of
`main`. Not a data source: nothing here feeds a table or a chart.

## Contents
- `GET /repos/PalmBeachPete53/DeepWater/releases/tags/latest-main`: `body` (the CI's `commit:` and `time:` lines), `assets[]`
  with `name`, `url`, `size`, `digest` (`sha256:...`).
- `GET .../compare/<base>...<head>`: `commits[].commit.message`, oldest first.
- `GET <asset url>` with `Accept: application/octet-stream`: a 302 to GitHub's storage.

## How to call it
- Base `https://api.github.com`. `Authorization: Bearer <token>`; a private repository answers 404 without one.
- `If-None-Match` with the last `ETag`: a 304 doesn't count against the rate limit.
- Trap: follow the asset's redirect by hand, without the token.

## What the app reads
Row `github` (`src/SourcesPage.cpp`). All of the above, in `src/Updater.cpp`; parsing in `src/UpdateRelease.cpp`. See README › Updates.

The news of a build comes through a relay, since GitHub has no WebSocket: `wss://ntfy.sh/<channel>/ws` (free, no account), events `open`, `keepalive` every 45 s, `message`. The CI posts `new build` to `https://ntfy.sh/<channel>`. Not trusted: it only causes a read of the release.

## Not read yet
Nothing it needs.

## Calls made
- `GET https://ntfy.sh/<test channel>/json` for 60 s with a `POST` to it → `open`, `message` (publish 200 in 0.3 s), `keepalive` after 45 s; `/ws` → 101.
- `GET .../releases/tags/latest-main` from the app with the GitHub CLI's token → 404: no release published on 5 Nov 2026.
- `GET /repos/PalmBeachPete53/DeepWater` from the app → 200.
- The rest was exercised against a local server of the same shape, not GitHub: the first published build is its real test.
