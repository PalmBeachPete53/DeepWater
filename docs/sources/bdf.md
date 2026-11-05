---
id: bdf
name: Banque de France (Webstat)
rows: []
catalogue: ["Banque de France"]
access: free key
probed: 2026-11-05
confidence: called     # the series catalogue answered; observations need a key, which was not used
---

# Banque de France (Webstat)

Webstat: credit, rates, money, balance of payments, business surveys, non-financial accounts and European series the Bank
republishes, one catalogue entry per series.

## Contents
An Opendatasoft catalogue of 43,290 entries, complete in `catalog/bdf.tsv.gz` (id, publisher, title, modified). Each entry is
**one series** whose description holds its dimensions ("Secteur", "Périodicité", "Zone géographique", ...). Its records are empty (`has_records` is false) except for
`tableaux_rapports_preetablis`, the list of prepared reports (fields `type`, `thematique_fr/en`, `title_fr/en`, `date_publication`,
`date_mise_a_jour`, `lien`, `piece_jointe`).

Publishers (facet): Banque de France 24,214; Eurostat 12,583; ACPR 3,092; National central banks and ECB 2,684; BCE 136; EURONEXT
23; INSEE 5; Banca d'Italia 4; Banco de España 4; more at low counts (13 in all). 2 languages; `modified` 2026 for 43,289.

Families, the first token of the id (53 in all):
`icp` 6,954 and `hicp` 5,349 consumer prices of Europe; `conj2` 4,296 and `conj` 1,116 monthly business surveys, including
regional ("Normandie - Industrie manufacturière - carnet de commandes"); `cnf` 3,719 financial accounts and MFI claims; `bpm6`
3,554 balance of payments; `bsi` 2,728 and `bsi1` 2,302 bank balance sheets; `acpr` 2,697 and `acpr_ass_rc` 522 insurance and banking
supervision; `cftdc` 2,310 credit by region; `diren` 1,413 mobilised credit by sector and firm size; `dwa1` 563 household wealth;
`fatso` 555 outward FATS; `tcn3` 524 short-term NEU paper; `mir1` 365 and `mir` 139 lending rates; `qfagg` 352; `bls` 329 lending
survey; `ra_t` 318; `det2` 306; `icb` 305; `ifi` 291; `cnfsi` 268; `exr` 101 exchange rates; `stbs` 123; `sa`, `cer`, `sc1`, `ivf1`,
`ame`, `cons`, `pea`, `fm`, `ofc`, `pme`. `themes`, `faq`, `webstat`, `glossary` and `sources` entries are site pages.

Frequencies and periods are per series and were not aggregated; the site says "more than 37,000 data series".

## How to call it
- Catalogue, no key: `GET https://webstat.banque-france.fr/api/explore/v2.1/catalog/datasets?limit=100`; everything:
  `GET .../catalog/exports/csv?select=dataset_id,publisher,title,modified&limit=-1` (about 7.6 MB, a minute); facets `.../catalog/facets`.
- Observations: `.../datasets/<id>/records` returns an empty list. `datasets/observations` and `datasets/series` are 404 without
  identification. The site's menu points to an API "developer portal" with a login, which fits the catalogue's "free key";
  its documentation was not read.
- robots.txt allows everything.

## What the app reads
Nothing. No row of the Sources list reads Webstat.

## Not read yet
`conj` and `conj2` business surveys, `mir` lending rates, `bls` lending survey, `diren` and `cftdc` credit, for Countries › France,
beside INSEE. Needs a free key for values.

## Calls made
Made on 2026-11-05, sequentially per host with a pause, `User-Agent: Deepwater/1.0`. 12 calls.

- `GET https://webstat.banque-france.fr/robots.txt` → 200, 77 B
- `GET https://webstat.banque-france.fr/api/explore/v2.1/catalog/datasets?limit=100` → 200, 986.0 KB
- `GET https://webstat.banque-france.fr/api/explore/v2.1/catalog/facets` → 200, 2.2 KB
- `GET https://webstat.banque-france.fr/api/explore/v2.1/catalog/datasets?limit=5&select=dataset_id,metas.default.title&where=dataset_id%20like%20%22obs%22` → 400, 153 B
- `GET https://webstat.banque-france.fr/api/explore/v2.1/catalog/datasets?limit=20&select=dataset_id,has_records&where=has_records%3Dtrue` → 200, 100 B
- `GET https://webstat.banque-france.fr/api/explore/v2.1/catalog/datasets/conj2-m-r28-s-in-000cz-cctsm000-10/records?limit=1` → 200, 33 B
- `GET https://webstat.banque-france.fr/api/explore/v2.1/catalog/datasets/conj2-m-r28-s-in-000cz-cctsm000-10` → 200, 9.0 KB
- `GET https://webstat.banque-france.fr/api/explore/v2.1/catalog/exports/csv?select=dataset_id,publisher,title,modified&limit=-1` → 200, 7.6 MB
- `GET https://webstat.banque-france.fr/api/explore/v2.1/catalog/datasets/observations/records?limit=1` → 404, 105 B
- `GET https://webstat.banque-france.fr/api/explore/v2.1/catalog/datasets/series/records?limit=1` → 404, 99 B
- `GET https://webstat.banque-france.fr/api/explore/v2.1/catalog/datasets/tableaux_rapports_preetablis` → 200, 5.4 KB
- `GET https://webstat.banque-france.fr/en/pages/guide-migration-api/` → 200, 457.0 KB
