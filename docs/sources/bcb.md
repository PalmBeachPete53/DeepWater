---
id: bcb
name: Banco Central do Brasil
rows: [research_bcb, nat_brazil]
catalogue: ["Banco Central do Brasil"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Banco Central do Brasil

Brazil's central bank has three public doors: SGS (time series by numeric code), Olinda (OData services such as the Focus
market expectations and PTAX exchange rates) and an open data portal listing them.

## Contents
**The open data portal holds 4,261 data sets** (`dadosabertos.bcb.gov.br/dataset`): BCB/Depef 1,962 (financial inclusion
indicators), BCB/Dstat 1,390 (statistics), Sistema Financeiro Nacional 596 (one per institution), BCB/Depec 212, BCB/Demab
22, BCB/Deban 18, BCB/Desig 16, BCB/Mecir 8, BCB/Secre 4, BCB/Deafi 3. By group: Inclusão Financeira 1,954, Economia e
Finanças 1,438, Câmbio e Capitais 4. By format: HTML 4,248, JSON 4,241, CSV 3,596, wsdl 3,578, API 656, OData 57.

**[`catalog/bcb.tsv`](catalog/bcb.tsv) lists 1,666 of them, the six statistical organisations** (Dstat, Depec, Demab, Deban,
Mecir, Desig), with the slug, title and organisation; **1,618 slugs start with the SGS code** (`433-...`). The portal has
no SGS-wide catalogue, and SGS itself offers none. SGS holds more codes than the portal lists: its codes run past 28000.

**SGS**, one value series per code, Brazil only. App's series, span verified from `dados?formato=json`:

| Code | Holds | First to last |
|---|---|---|
| `433`, `13522` | IPCA monthly, over 12 months | 1980-01, 1980-12 to 2026-09 |
| `4466` | Core IPCA, trimmed means | 1991-12 to 2026-09 |
| `24369` | Unemployment (PNAD) | 2012-03 to 2026-09 |
| `28763` | Formal jobs (CAGED) | 1992-01 to 2026-08 |
| `1788` | Monetary base | 1946-02 to 2026-09 |
| `27791`, `27810`, `27813`, `27815` | M1, M2, M3, M4 | 2001/2002 to 2026-09 |
| `3546` | Reserves | 1971-01 to 2026-10 |
| `20539`, `20714` | Credit outstanding, average rate | 1988-06, 2011-03 to 2026-09 |
| `22707`, `22701` | Trade balance, current account | 1995-01 to 2026-09 |
| `13762` | General government gross debt | 2006-12 to 2026-09 |
| `21859`, `28503` | Industrial production | 2002-01 to 2026-08 |
| `4393` | Consumer confidence | 1999-04 to 2026-10 |
| `1373` | Vehicle production | 1993-02 to 2026-06 |
| `432` | Selic target, daily | daily, window limit below |

**Olinda services called** (OData v4, `$metadata`):
- `Expectativas` v1, the Focus survey. Sets: `ExpectativaMercadoMensais`, `ExpectativasMercadoTrimestrais`,
  `ExpectativasMercadoAnuais`, `ExpectativasMercadoSelic`, `ExpectativasMercadoInflacao12Meses`,
  `ExpectativasMercadoInflacao24Meses`, `ExpectativasMercadoTop5Anuais`, `ExpectativasMercadoTop5Mensais`,
  `ExpectativaMercadoTop5Trimestral`, `ExpectativasMercadoTop5Selic`, `ExpectativasMercadoTop5Inflacao12Meses`,
  `ExpectativasMercadoTop5Inflacao24Meses`, `DatasReferencia`. Fields: `Indicador`, `IndicadorDetalhe`, `Data`,
  `DataReferencia`, `Reuniao`, `Media`, `Mediana`, `DesvioPadrao`, `Minimo`, `Maximo`, `numeroRespondentes`, `baseCalculo`,
  `tipoCalculo`.
- `PTAX` v1, official exchange rates. Sets: `Moedas`, and functions `CotacaoDolarDia`, `CotacaoDolarPeriodo`,
  `CotacaoMoedaDia`, `CotacaoMoedaPeriodo`, `CotacaoMoedaPeriodoFechamento`, `CotacaoMoedaAberturaOuIntermediario`. Fields:
  `cotacaoCompra`, `cotacaoVenda`, `paridadeCompra`, `paridadeVenda`, `dataHoraCotacao`, `tipoBoletim`.
- `taxaJuros` v2, lending rates by bank. Sets: `TaxasJurosMensalPorMes`, `TaxasJurosDiariaPorInicioPeriodo`,
  `ParametrosConsulta`, `ConsultaUnificada`, `ConsultaDatas`. Fields: `Modalidade`, `Segmento`, `InstituicaoFinanceira`,
  `cnpj8`, `Posicao`, `TaxaJurosAoMes`, `TaxaJurosAoAno`.

## How to call it
- SGS: `GET https://api.bcb.gov.br/dados/serie/bcdata.sgs.433/dados?formato=json` or `.../dados/ultimos/1?formato=json`,
  `dataInicial` and `dataFinal` as `dd/MM/yyyy`. Answer `[{"data":"dd/MM/yyyy","valor":"text"}]`.
- Olinda: `GET https://olinda.bcb.gov.br/olinda/servico/Expectativas/versao/v1/odata/$metadata`.
- Portal: `GET https://dadosabertos.bcb.gov.br/dataset?organization=dstat&page=1`, 20 per page.
- Traps: **daily SGS series refuse the whole history**: 406 "janela de consulta de, no máximo, 10 anos". **An unknown code
  answers 200 with an HTML page**, not an error. `dadosabertos` robots.txt forbids `/api/` and asks a crawl delay of 10 s,
  so CKAN's `package_list` and `package_search` were not called. `olinda.bcb.gov.br/olinda/servico/` has no index (404).

## What the app reads
- `research_bcb`, row at `src/SourcesPage.cpp:533`; Countries › Brazil, `src/CountryData.cpp:1622`, provider `bcb` in
  `resources/country_indicators.json`: the 23 entries over the codes above, keeps `data` and `valor`.
- `nat_brazil`, row at `src/SourcesPage.cpp:718`; `src/NationalSources.cpp:36`: `13522`, last 15 months, for the Map.
  The row's other three calls go to IBGE: `docs/sources/ibge.md`.

## Not read yet
- **`Expectativas`**: Desk › Forecasts could show the Focus survey by indicator and horizon.
- **`PTAX`**: official BRL fixings for Forex.
- `taxaJuros`: lending rates per bank.
- Other SGS: `catalog/bcb.tsv`, by slug, for balance-of-payments and public finance series (`10608-...`, `Conta financeira`).

## Calls made
- `GET https://dadosabertos.bcb.gov.br/robots.txt` → 200, 123 B (forbids `/api/`, crawl delay 10)
- `GET https://dadosabertos.bcb.gov.br/api/3/action/package_list`, `.../package_search?rows=1000` → not sent, robots.txt
- `GET https://dadosabertos.bcb.gov.br/dataset?page=1` → 200, 54 KB; then pages 2 to 12 → 200, stopped to narrow the job
- `GET https://dadosabertos.bcb.gov.br/dataset?organization=<org>&page=<n>` for dstat, depec, demab, deban, mecir, desig,
  at a 10.5 s pace → all 200, 1,666 data sets
- `GET https://olinda.bcb.gov.br/olinda/servico/Expectativas/versao/v1/odata/$metadata` → 200, 9.5 KB
- `GET https://olinda.bcb.gov.br/olinda/servico/PTAX/versao/v1/odata/$metadata` → 200, 5.3 KB
- `GET https://olinda.bcb.gov.br/olinda/servico/taxaJuros/versao/v2/odata/$metadata` → 200, 2.4 KB
- `GET https://olinda.bcb.gov.br/olinda/servico/` → 404
- `GET https://api.bcb.gov.br/dados/serie/bcdata.sgs.433/dados/ultimos/1?formato=json` → 200, 38 B
- `GET https://api.bcb.gov.br/dados/serie/bcdata.sgs.1/dados?formato=json` → 406, window limit
- `GET https://api.bcb.gov.br/dados/serie/bcdata.sgs.999999/dados/ultimos/1?formato=json` → 200, an HTML page
- `GET https://api.bcb.gov.br/dados/serie/bcdata.sgs.<code>/dados?formato=json` for the 20 monthly codes above → all 200
- `GET https://api.bcb.gov.br/dados/serie/bcdata.sgs.432/dados/ultimos/1?formato=json` → 200
