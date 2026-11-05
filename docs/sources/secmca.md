---
id: secmca
name: Consejo Monetario Centroamericano (SECMCA)
rows: []
catalogue: ["Consejo Monetario Centroamericano", "Consejo Monetario Centroamericanono", "SEMCA - Consejo Monetario Centroamericano", "Central American Monetary Council", "SECMCA - Consejo Monetario Centroamericano"]
access: free account
probed: 2026-11-05
confidence: called
---

# SECMCA

The secretariat of the Central American Monetary Council: prices, output, rates, money, credit, external accounts and public finances of Central America, with a documented API.

## Contents
Four databases, **4,775 variables** in all: [`catalog/secmca.tsv`](catalog/secmca.tsv) (database, theme, table, code, label in Spanish).

| Database | Themes or tables | Variables |
|---|---|---|
| SECMCA Datos | 86 themes | 1,078 |
| EMFA (monetary and financial) | 5 themes, 26 tables: `PANORAMAS_FINANCIEROS, AGREGADOS_MONETARIOS, ACT_PAS_OSD_BD, CREDITO_OSD_BC, COMP_CAPT_ACT_EXTER_OSD_BC` | 520 |
| ESEA (external sector) | 4 themes, 16 forms: `SALD_BRUT_DEU_EXTERN, BALANZA_DE_PAGOS, POS_INV_INTER, ACT_RESERV_LIQUID_EN_ME` | 2,254 |
| EFPA (public finance) | 17 tables, `GFSM2014_E1` to `E4`, `C1` to `C12`, `PSDS2011_C1` | 923 |

Web page's headings for SECMCA Datos: Precios, Producción, Reservas internacionales netas, Tipos de cambio, Remesas familiares, Comercio exterior, Inversión extranjera directa, Balanza de pagos, Tasas de interés y encaje, Agregados monetarios y depósitos, Crédito, Finanzas del Gobierno Central, Deuda pública.

SECMCA Datos by theme: countries `CRI` 75, `GTM` 75, `HND` 75, `NIC` 74, `DOM` 74, `SLV` 72, `PAN` 70, `BLZ` 32, and the aggregates `CA5`, `CA6`, `CA7` 12 each. Frequency: monthly 66, annual 11, quarterly 8, daily 1. Units: percent 33, US dollars 30, national currency 24, index 19, % of GDP 10, national currency per USD 7. Each theme carries its `variables, paises, frecuencias, valoraciones, ajustes, unidades`.

Example range: `DPFC` (external debt) in Costa Rica, 2003-01-01 to 2026-06-30. Other ranges come from the same call. Historical statistics 1960-2017 and a banking system set (ESB) are on the site's pages (not read).

## How to call it
- Base `https://secmca-api.secmca.org/simafir_api/ws`. OpenAPI: `/v3/api-docs` (44 paths), Swagger at `/swagger-ui/index.html`.
- Open, no login: `/public/v1/tema`, `/public/v1/emfa/tema`, `/public/v1/esea/tema`, `/public/efpa/cuadros`, `/public/efpa/criterios`, `/public/v1/rango-fechas/{variable}/{pais}`.
- **Data was refused:** `/public/v1/obtenerDatos/porCriterio/CRI/DPFC/USD/M?startPeriod=2026-01` answered 401 "No tiene permisos para consumir este recurso!". The API has `POST /auth/login`; no account was tried. The `obtenerDatos` paths and `tipoCambio` thus remain unverified, despite the `public` in the path.
- Trap: countries are ISO3 (`CRI`; `CR` gives 404). A variable is not in every database: `DPFC` answers under SECMCA Datos and 404 under EMFA.
- The catalogue says `api: web` under five spellings of the name (one, `Centroamericanono`, a typo).

## What the app reads
Nothing.

## Not read yet
**Economy › Countries:** monthly prices, rates, reserves, remittances for Honduras, Nicaragua, El Salvador, Guatemala, Costa Rica, the Dominican Republic, Panama, Belize, once the data calls are settled.

## Calls made
All on 2026-11-05 with `User-Agent: Deepwater/1.0`. Base as above.
- `GET /v3/api-docs` → 200, 128,289 bytes. `GET /swagger-ui/index.html` → 200, 734 bytes.
- `GET /public/v1/tema` → 200, 211,377. `/public/v1/emfa/tema` → 200, 41,351. `/public/v1/esea/tema` → 200, 432,554. `/public/efpa/cuadros` → 200, 90,887. `/public/efpa/criterios` → 200, 2,328.
- `GET /public/v1/rango-fechas/DPFC/CR` → 404, 78 bytes. `/public/v1/rango-fechas/DPFC/CRI` → 200, 52. `/public/v1/tipoCambioExterno/rango-fechas/TC/CRI` → 404, 77. `/public/v1/emfa/rango-fechas/DPFC/CRI` → 404, 79.
- `GET /public/v1/obtenerDatos/porCriterio/CRI/DPFC/USD/M?startPeriod=2026-01` → 401, 45 bytes.
- Earlier probe of `https://www.secmca.org/secmcadatos` and the site's headings: `catalog/bodies_probe.jsonl.gz`.
