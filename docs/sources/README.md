# What the sources hold

The Sources tab says **who** the app can read. This folder says **what is inside each of them**: which data sets,
which fields, which countries, how far back, how often, and what the app reads against what it leaves untouched. It
holds structure only, never the figures. Look here before deciding that a data set does not exist, or before adding a
source for something an existing one already offers.

- [`INDEX.md`](INDEX.md): one line per source, and a topic index (who has what).
- `<id>.md`: one card per source the app reads, or per body of the catalogue with a real API.
- `catalog/`: long lists the cards point to (`<id>.tsv`, `*.jsonl`, gzipped over 1 MB). `zgrep` reads them all; they are
  not meant to be read through.
- [`catalogue-bodies.md`](catalogue-bodies.md), `catalog/bodies.jsonl`: what each of the bodies of
  `resources/data_sources.json` publishes, by country, with the calls behind it. `catalog/bodies_probe.jsonl.gz` is
  the raw first call of each, by `tools/source-inventory/probe_sites.py`.
- [`catalogue-corrections.md`](catalogue-corrections.md): what was corrected in the catalogue on 5 November 2026 (links,
  access, name variants), and [`catalogue-api-audit.md`](catalogue-api-audit.md), the audit behind the `api` corrections.
- [`app-findings.md`](app-findings.md): what the calls suggested about the app, each confirmed or disproved, and what was done.

Every statement comes from a call made on the date in the card, or says it doesn't. Sources change: when a card
matters for a decision, repeat its catalogue call.

How far to trust it: a card's `confidence` says whether its source answered. In `catalogue-bodies.md`, 599 of the 1,298
bodies could not be read at their site (refused, dead, script-only, not found): their subjects are those Trading
Economics credited to them on 1 November 2026, unchecked at the source. Sections marked "General knowledge" are not from
a call. Keyed sources (FRED, EIA, BEA and others) were inventoried without their keys.

`tools/source-inventory/merge_bodies.py` rebuilds `catalogue-bodies.md`; `build_index.py` rebuilds `INDEX.md` from the
cards and its own topic table.

## Format of a card

```markdown
---
id: oecd                              # file name; the id of SourceFlow.cpp's Sources table when it has one
name: Organisation for Economic Co-operation and Development
rows: [itf_road, research_oecd, oecd_housing, oecd_kei]   # ids of the Sources list (SourcesPage.cpp); [] if none
catalogue: ["OECD", "Organisation for Economic Co-operation and Development"]   # exact `name`s in data_sources.json
access: free, no key                  # free, no key | free key | free account | paid | scraped web page | local
probed: 2026-11-05
confidence: called                    # called: its catalogue or a sample answered | docs: public documentation only,
                                      # with why | refused: it refused, with how
---

# Name

One or two sentences: what kind of information lives here.

## Contents
The structure, from the source's own answers: its data sets or tables (count, then the families, the important ones by
code), their dimensions and fields, countries covered, frequency, first and last period, update rhythm. Exact counts,
exact codes. A list over ~40 entries goes to `catalog/<id>.tsv` and this section gives the families and counts.

## How to call it
Base URL, catalogue call, data call (a real working example), auth, formats, rate limits, traps met.

## What the app reads
Row by row: the data set or endpoint, the fields it keeps, `file:line`.

## Not read yet
What the source offers beyond that and could serve the app: the concrete data sets by code, and the page it could feed.

## Calls made
`GET <url>` → status, size. Every call this card rests on, failures included. Never a key in a URL.
```

No figures: codes, names, labels, field names, date ranges and counts are structure; prices, rates and other values
are not. A truncated field sample may show its type, not its value (`price: number`).

Refresh: `tools/source-inventory/` holds the scripts for the long jobs; each card's *Calls made* is enough for the rest.
