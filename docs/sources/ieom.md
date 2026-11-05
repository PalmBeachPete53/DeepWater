---
id: ieom
name: Institut d'émission d'outre-mer (IEOM)
rows: []
catalogue: ["Institute of Overseas Issuance"]
access: scraped web page
probed: 2026-11-05
confidence: called
---

# IEOM

The central bank of the French Pacific territories, and the site its statistics share with the French overseas departments. Business climate, credit, balance of payments and failures, as interactive charts by territory.

## Contents
Ten themes on `/Statistiques`, under four headings:

| Heading (page) | `id_theme` | Theme |
|---|---|---|
| Conjoncture | 101 | Indicateur du climat des affaires |
| | 102 | Soldes d'opinion |
| Données bancaires | 201 | Encours de crédit |
| | 202 | Taux de créances douteuses |
| | 203 | Coût du crédit |
| | 204 | Épargne bancaire |
| | 205 | Tarifs bancaires |
| Balance des paiements | 301 | Soldes de balance des paiements |
| Entreprises | 401 | Défaillances |
| | 402 | Délais de paiement |

Theme 101 read: from a quarterly business survey, centred on 100; geographies Guadeloupe, Guyane, Martinique, La Réunion, Mayotte, Nouvelle-Calédonie, Polynésie française; updated 20 October 2026. Date range not shown in the text read. `/Publications` holds 3,763 publications by the earlier probe.

## How to call it
- `https://www.ieom.fr/Statistiques?id_theme=101`. Charts by Highcharts with an export module, and no csv or xls link in the page. No API found; the chart data's source was not looked for.
- `robots.txt` closes only technical folders such as `/ecrire/`.
- The catalogue's English name "Institute of Overseas Issuance" is a translation of this body, listed for New Caledonia.

## What the app reads
Nothing.

## Not read yet
**Economy › Countries:** New Caledonia and French Polynesia, if the chart data can be reached.

## Calls made
All on 2026-11-05 with `User-Agent: Deepwater/1.0`.
- `GET https://www.ieom.fr/robots.txt` → 200, 472 bytes.
- `GET https://www.ieom.fr/Statistiques` → 200, 32,720 bytes (twice).
- `GET https://www.ieom.fr/Statistiques?id_theme=101` → 200, 44,567 bytes.
- Earlier probe of `/Publications` and `/Statistiques`: `catalog/bodies_probe.jsonl.gz`.
