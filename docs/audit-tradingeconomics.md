# Audit des sources de données de tradingeconomics.com

*Deepwater · 1er novembre 2026 · crawl des pages publiques du site*

## 1. Objet et méthode

Trading Economics (TE) publie des pages « pays × indicateur » (`/france/inflation-cpi`), des pages de marchés
(devises, indices, obligations, matières premières, cryptos), des notations, des prévisions et un calendrier. Chaque
page indicateur crédite une source en bas de sa description :

```html
<span class='source-present'>source: <a class='source-name' href='http://www.bea.gov'>U.S. Bureau of Economic Analysis</a></span>
```

L'audit a lu **toutes les pages déclarées par le site lui-même** : son `sitemap.xml` (21 890 URL ; `robots.txt` ne
contient qu'une ligne `Sitemap:` et n'interdit rien). Chaque page a été téléchargée une fois (HTML compressé,
~57 Ko), la source et son lien extraits, et les pages sans balise source classées par type. Les pages de prévision
(`/forecast/…`), de classement (`/country-list/…`), de matières premières (`/commodity/…`), les rapports et les
jours fériés n'ont pas de source nationale : elles sont comptées à part.

Contraintes rencontrées :

- À 8 connexions simultanées le site a répondu `403 Access denied` au bout d'un millier de requêtes. Le crawl a été
  repris avec 3 connexions et plus d'une seconde entre deux requêtes (environ 100 pages par minute, soit 3 h 30
  pour le site), en respectant le blocage (pause de 5 minutes sur un 403, pas de rotation d'identité).
- Trois pages `/calendar` par pays répondent `405` ; elles sont sans source de toute façon.
- La page « databases » de l'API TE résume sa propre architecture : « 20 thousand indicators directly from national
  sources, 1 million series from the US Federal Reserve (FRED), 1 million series from World Bank development
  indicators, 20 million COMTRADE export/import statistics, EUROSTAT data ». Les « 20 millions d'indicateurs » du
  slogan sont donc pour l'essentiel les lignes Comtrade.

Les artefacts : `resources/data_sources.json` (le catalogue livré avec l'application : chaque source, son site, son
type, son mode d'accès, les pays et indicateurs qu'elle alimente) et ce document. Les scripts de crawl et de
construction sont des scripts Python d'une session de travail, non versionnés.

## 2. Ce que TE utilise, par famille

| Famille | Ce que TE en tire | Accès |
|---|---|---|
| **Instituts statistiques nationaux** (INSEE, Destatis, ISTAT, INE, ONS, BEA, BLS, Census, Statistics Canada, ABS, e-Stat, NBS Chine, IBGE, INEGI, KOSTAT…) | PIB, prix, emploi, production, ventes, commerce, démographie : la grande majorité des pages pays | Gratuit (publications officielles ; API pour une trentaine d'entre eux) |
| **Banques centrales** (Fed, BCE, BoE, BoJ, BPC, RBI, BCB, Banxico, SNB, Riksbank, Norges Bank, RBA, RBNZ…) | Taux directeurs, agrégats monétaires, crédit, réserves, balance des paiements, taux de change | Gratuit (API ou CSV pour les grandes) |
| **Ministères et agences** (Trésor, douanes, ministères des finances, EIA, CFTC…) | Finances publiques, dette, commerce extérieur, énergie | Gratuit |
| **Compilations internationales** : Eurostat, FMI, Banque mondiale, OCDE, BRI, BCE, ONU/Comtrade, OIT | Les mêmes chiffres nationaux harmonisés, et les pays sans publication accessible | Gratuit, API sans clé (Comtrade : clé gratuite, quota) |
| **FRED** (Fed de Saint-Louis) | « 1 million de séries » américaines et internationales, par État et comté | Gratuit, clé gratuite |
| **Instituts privés** : S&P Global (PMI), Ifo, ZEW, GfK, Conference Board, ISM, Université du Michigan, ADP, Westpac–Melbourne Institute, ANZ, NAB, Caixin, Jibun, HCOB… | Enquêtes de conjoncture, confiance, PMI, emploi privé | Chiffre-titre gratuit (communiqués), séries payantes |
| **Agences de notation** : S&P, Moody's, Fitch, DBRS | Pages `/rating` (TE y ajoute sa propre note composite) | Notations publiques, données payantes |
| **Marchés** (pages `/currency`, `/stock-market`, `/government-bond-yield`, `/commodity/…`) | Cours temps réel ou différés, sans source nommée (« OTC Interbank », bourses) | Flux payants chez TE ; cours différés gratuits ailleurs (Yahoo, bourses, BCE pour les changes) |
| **Trading Economics** | Prévisions (« global macro models and analysts expectations »), indices TE (métaux, céréales, crack spread…), note de crédit composite | Propriétaire |

## 3. Résultats du crawl

Le sitemap déclare 21 890 pages ; la passe de complétude (les liens des 235 pages `/indicators` par pays absents du
sitemap) en a ajouté 27 (`icu-beds`). **21 917 pages lues, toutes avec une réponse 200** après reprise des 63 pages
refusées pendant le blocage. Les pages `/currency`, `living-wage-family`, `wages-low-skilled` et `wages-high-skilled`
refusent un client sans navigateur (403) mais ont fini par répondre ; elles n'ont pas de source nationale (les pages
salaires citent WageIndicator, payant).

Répartition des pages : 17 473 pages « pays × indicateur » avec une source nommée (le cœur de l'audit), 3 782 pages
pays sans balise source (devises, bourses, rendements obligataires, notations, calendrier, jours fériés, stocks de
gaz), 255 pages de prévisions, 178 classements mondiaux, 114 matières premières, 66 rapports et une vingtaine de
pages de marchés et d'accueil. Les tableaux ci-dessous ne comptent que les pages avec une source nommée ; chaque
source est comptée une fois par page (TE ne nomme qu'une source par page).

Pages analysées : 21917 (dont 17473 pages indicateur × pays avec une source nommée, 3782 pages pays sans balise source : marchés, notations, devises).

| Type de source | Sources distinctes | Pages indicateur |
|---|---:|---:|
| Ministère / agence publique | 701 | 2094 |
| Office statistique national | 335 | 6343 |
| Banque centrale | 232 | 3283 |
| Institut privé / association | 151 | 849 |
| Organisation internationale | 49 | 4906 |

| Accès | Sources distinctes | Pages indicateur |
|---|---:|---:|
| gratuit (données publiques) | 1317 | 16626 |
| chiffre-titre gratuit, série payante | 142 | 743 |
| payant | 9 | 106 |

### Les 60 sources les plus utilisées

| # | Source | Type | Accès | Pays | Pages | Accès programmatique |
|---:|---|---|---|---:|---:|---|
| 1 | [World Bank](https://www.worldbank.org/) | Organisation internationale | gratuit (données publiques) | 212 | 1310 | Indicators API (JSON/CSV), no key |
| 2 | [EUROSTAT](https://ec.europa.eu/eurostat/) | Organisation internationale | gratuit (données publiques) | 41 | 803 | REST API: JSON-stat and SDMX, no key |
| 3 | [World Health Organization](https://coronavirus.data.gov.uk) | Organisation internationale | gratuit (données publiques) | 176 | 527 | web |
| 4 | [AKAMAI]() | Institut privé / association | chiffre-titre gratuit, série payante | 194 | 388 | web |
| 5 | [Transparency International](https://www.transparency.org/en) | Organisation internationale | gratuit (données publiques) | 188 | 375 | web |
| 6 | [OECD](https://www.oecd.org/) | Organisation internationale | gratuit (données publiques) | 66 | 335 | OECD SDMX API, no key |
| 7 | [World Economic Forum](https://www.weforum.org) | Organisation internationale | gratuit (données publiques) | 152 | 304 | web |
| 8 | [SIPRI](https://www.sipri.org) | Organisation internationale | gratuit (données publiques) | 157 | 193 | web |
| 9 | [Institute for Economics and Peace](http://economicsandpeace.org) | Organisation internationale | gratuit (données publiques) | 162 | 162 | web |
| 10 | [Office for National Statistics](http://www.ons.gov.uk/) | Office statistique national | gratuit (données publiques) | 1 | 130 | ONS API, no key |
| 11 | [Statistics Canada](https://www.statcan.gc.ca) | Office statistique national | gratuit (données publiques) | 1 | 110 | Web Data Service (JSON), no key |
| 12 | [Australian Bureau of Statistics](http://www.abs.gov.au) | Office statistique national | gratuit (données publiques) | 1 | 104 | ABS Data API (SDMX), no key |
| 13 | [World Gold Council](http://www.gold.org) | Organisation internationale | gratuit (données publiques) | 103 | 103 | web |
| 14 | [Statistics Norway](https://www.ssb.no) | Office statistique national | gratuit (données publiques) | 1 | 92 | SSB PxWeb API, no key |
| 15 | [European Central Bank](https://www.ecb.europa.eu/home/html/index.en.html) | Organisation internationale | gratuit (données publiques) | 22 | 90 | ECB Data Portal API (SDMX), no key |
| 16 | [Central Bank of West African States (BCEAO)](http://www.bceao.int) | Organisation internationale | gratuit (données publiques) | 10 | 89 | web |
| 17 | [S&P Global](https://www.pmi.spglobal.com/public) | Institut privé / association | payant | 47 | 86 | press releases (headline figures free, series paid) |
| 18 | [Central Statistics Office Ireland](https://www.cso.ie/) | Office statistique national | gratuit (données publiques) | 1 | 84 | PxStat API, no key |
| 19 | [Statistics Sweden](https://www.scb.se/) | Office statistique national | gratuit (données publiques) | 1 | 84 | SCB PxWeb API, no key |
| 20 | [National Bureau of Statistics of China](http://www.stats.gov.cn/) | Office statistique national | gratuit (données publiques) | 1 | 80 | data.stats.gov.cn easyquery (JSON), no key |
| 21 | [National Institute of Statistics (ISTAT)](https://www.istat.it) | Office statistique national | gratuit (données publiques) | 1 | 76 | SDMX API (esploradati), no key |
| 22 | [Statistics Denmark](https://www.dst.dk/) | Office statistique national | gratuit (données publiques) | 1 | 76 | StatBank API, no key |
| 23 | [Statistics New Zealand](https://www.stats.govt.nz/) | Office statistique national | gratuit (données publiques) | 1 | 76 | Infoshare and Aotearoa Data Explorer (CSV), no key |
| 24 | [Statistics Finland](https://www.stat.fi) | Office statistique national | gratuit (données publiques) | 1 | 74 | PxWeb API, no key |
| 25 | [Bank for International Settlements](https://www.bis.org/) | Organisation internationale | gratuit (données publiques) | 51 | 72 | BIS SDMX API, no key |
| 26 | [Statistics Portugal](http://www.ine.pt) | Office statistique national | gratuit (données publiques) | 1 | 71 | INE API (JSON), no key |
| 27 | [U.S. Energy Information Administration](http://www.eia.gov) | Ministère / agence publique | gratuit (données publiques) | 58 | 71 | EIA API v2, free key |
| 28 | [Statistics Netherlands](https://www.cbs.nl/) | Office statistique national | gratuit (données publiques) | 1 | 70 | CBS OData API, no key |
| 29 | [Federal Statistical Office](https://www.destatis.de) | Office statistique national | gratuit (données publiques) | 1 | 69 | GENESIS-Online API, free account |
| 30 | [Statistical Office of the Slovak Republic](https://slovak.statistics.sk/) | Office statistique national | gratuit (données publiques) | 1 | 69 | web |
| 31 | [U.S. Bureau of Labor Statistics](http://www.bls.gov/) | Office statistique national | gratuit (données publiques) | 1 | 69 | BLS API v1 (no key, 25 requests a day) and v2 (free key) |
| 32 | [INSEE, France](https://www.insee.fr) | Office statistique national | gratuit (données publiques) | 1 | 68 | API Melodi (SDMX-JSON), no key |
| 33 | [Central Bureau of Statistics, Israel](https://www.cbs.gov.il/) | Office statistique national | gratuit (données publiques) | 1 | 66 | CBS OData API, no key |
| 34 | [Instituto Nacional de Estadística y Geografía (INEGI)](https://www.inegi.org.mx/) | Office statistique national | gratuit (données publiques) | 1 | 66 | INEGI API, free key |
| 35 | [Turkish Statistical Institute](https://www.tuik.gov.tr/) | Office statistique national | gratuit (données publiques) | 1 | 66 | web tables (XLS) |
| 36 | [Statistics South Africa](http://www.statssa.gov.za/) | Office statistique national | gratuit (données publiques) | 1 | 65 | web tables (XLSX) |
| 37 | [European Commission](http://ec.europa.eu) | Organisation internationale | gratuit (données publiques) | 25 | 64 | web |
| 38 | [Czech Statistical Office](https://www.czso.cz) | Office statistique national | gratuit (données publiques) | 1 | 63 | web |
| 39 | [Instituto Nacional de Estadística y Censos (INDEC)](https://www.indec.gob.ar/) | Office statistique national | gratuit (données publiques) | 1 | 62 | web |
| 40 | [National Statistical Institute, Bulgaria](http://www.nsi.bg) | Office statistique national | gratuit (données publiques) | 1 | 62 | web |
| 41 | [Central Bank of Sri Lanka](https://www.cbsl.gov.lk) | Banque centrale | gratuit (données publiques) | 1 | 61 | web |
| 42 | [Central Statistics Office, Mauritius](http://statsmauritius.govmu.org) | Office statistique national | gratuit (données publiques) | 1 | 61 | PxStat API, no key |
| 43 | [World Steel Association](https://www.worldsteel.org/) | Organisation internationale | gratuit (données publiques) | 61 | 61 | web |
| 44 | [National Statistics Institute (INE)](https://www.ine.es/) | Office statistique national | gratuit (données publiques) | 1 | 60 | INEbase JSON API, no key |
| 45 | [Statistical Office of the Republic of Slovenia](https://www.stat.si) | Office statistique national | gratuit (données publiques) | 1 | 60 | web |
| 46 | [Statistics Singapore](https://www.singstat.gov.sg/) | Office statistique national | gratuit (données publiques) | 1 | 60 | SingStat Table Builder API, no key |
| 47 | [Census and Statistics Department, Hong Kong](https://www.censtatd.gov.hk) | Office statistique national | gratuit (données publiques) | 1 | 59 | C&SD web API, no key |
| 48 | [Statistics Estonia](https://www.stat.ee/en) | Office statistique national | gratuit (données publiques) | 1 | 59 | PxWeb API, no key |
| 49 | [General Authority for Statistics, Saudi Arabia](https://www.stats.gov.sa) | Office statistique national | gratuit (données publiques) | 1 | 56 | web |
| 50 | [Central Statistical Bureau of Latvia](https://stat.gov.lv/en) | Office statistique national | gratuit (données publiques) | 1 | 55 | web |
| 51 | [Department of Statistics, Malaysia](https://www.dosm.gov.my) | Office statistique national | gratuit (données publiques) | 1 | 55 | web |
| 52 | [Federal State Statistics Service](https://rosstat.gov.ru/) | Office statistique national | gratuit (données publiques) | 1 | 55 | web tables |
| 53 | [National Statistical Service of Greece](https://www.statistics.gr/) | Office statistique national | gratuit (données publiques) | 1 | 55 | web |
| 54 | [Philippine Statistics Authority](https://psa.gov.ph) | Office statistique national | gratuit (données publiques) | 1 | 55 | OpenSTAT, no key |
| 55 | [The Bank of Korea](https://www.bok.or.kr) | Banque centrale | gratuit (données publiques) | 2 | 55 | ECOS API, free key |
| 56 | [Ministry of Statistics and Programme Implementation (MOSPI)](https://www.mospi.gov.in/) | Office statistique national | gratuit (données publiques) | 1 | 54 | MOSPI eSankhyiki (CSV), no key |
| 57 | [Statistics Austria](https://www.statistik.at/) | Office statistique national | gratuit (données publiques) | 1 | 54 | STATcube (CSV), no key |
| 58 | [Kosovo Agency of Statistics](http://ask.rks-gov.net) | Office statistique national | gratuit (données publiques) | 1 | 53 | web |
| 59 | [Statistics Lithuania](https://osp.stat.gov.lt/) | Office statistique national | gratuit (données publiques) | 1 | 53 | web |
| 60 | [Ministry of Finance](https://www.finances.gov.mr/) | Ministère / agence publique | gratuit (données publiques) | 28 | 52 | web |

### Pages sans balise source (catégories)

| Page | Nombre |
|---|---:|
| holidays | 209 |
| forecast | 206 |
| calendar | 194 |
| rating | 191 |
| indicators | 183 |
| report | 182 |
| currency | 178 |
| exports-by-country | 177 |
| exports-by-category | 177 |
| imports-by-category | 177 |
| imports-by-country | 177 |
| news | 165 |
| stock-market | 96 |
| living-wage-individual | 70 |
| living-wage-family | 69 |
| wages-high-skilled | 66 |
| wages-low-skilled | 66 |
| government-bond-yield | 59 |
| co2-emissions | 46 |
| coronavirus-vaccination-total%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20 | 41 |
| coronavirus-vaccination-rate%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20 | 41 |
| consumer-confidence-economic-expectations | 40 |
| withholding-tax-rate | 35 |
| consumer-confidence-current-conditions | 33 |
| consumer-confidence-financial-expectations | 32 |
| coronavirus-vaccination-rate%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20 | 31 |
| coronavirus-vaccination-total%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20 | 31 |
| consumer-confidence-major-purchases-expectations | 28 |
| residential-property-prices | 26 |
| coronavirus-vaccination-rate%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20 | 26 |
| coronavirus-vaccination-total%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20 | 26 |
| consumer-confidence-unemployment-expectations | 25 |
| 2-year-note-yield | 24 |
| coronavirus-vaccination-rate%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20 | 24 |
| coronavirus-vaccination-total%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20%20 | 24 |
| natural-gas-stocks-capacity | 21 |
| natural-gas-stocks-inventory | 21 |
| 3-month-bill-yield | 20 |
| consumer-confidence-savings-expectations | 20 |
| 6-month-bill-yield | 19 |

### Toutes les sources

| Source | Type | Accès | Pays | Pages | Site |
|---|---|---|---:|---:|---|
| World Bank | Organisation internationale | gratuit (données publiques) | 212 | 1310 | https://www.worldbank.org/ |
| EUROSTAT | Organisation internationale | gratuit (données publiques) | 41 | 803 | https://ec.europa.eu/eurostat/ |
| World Health Organization | Organisation internationale | gratuit (données publiques) | 176 | 527 | https://coronavirus.data.gov.uk |
| AKAMAI | Institut privé / association | chiffre-titre gratuit, série payante | 194 | 388 |  |
| Transparency International | Organisation internationale | gratuit (données publiques) | 188 | 375 | https://www.transparency.org/en |
| OECD | Organisation internationale | gratuit (données publiques) | 66 | 335 | https://www.oecd.org/ |
| World Economic Forum | Organisation internationale | gratuit (données publiques) | 152 | 304 | https://www.weforum.org |
| SIPRI | Organisation internationale | gratuit (données publiques) | 157 | 193 | https://www.sipri.org |
| Institute for Economics and Peace | Organisation internationale | gratuit (données publiques) | 162 | 162 | http://economicsandpeace.org |
| Office for National Statistics | Office statistique national | gratuit (données publiques) | 1 | 130 | http://www.ons.gov.uk/ |
| Statistics Canada | Office statistique national | gratuit (données publiques) | 1 | 110 | https://www.statcan.gc.ca |
| Australian Bureau of Statistics | Office statistique national | gratuit (données publiques) | 1 | 104 | http://www.abs.gov.au |
| World Gold Council | Organisation internationale | gratuit (données publiques) | 103 | 103 | http://www.gold.org |
| Statistics Norway | Office statistique national | gratuit (données publiques) | 1 | 92 | https://www.ssb.no |
| European Central Bank | Organisation internationale | gratuit (données publiques) | 22 | 90 | https://www.ecb.europa.eu/home/html/index.en.html |
| Central Bank of West African States (BCEAO) | Organisation internationale | gratuit (données publiques) | 10 | 89 | http://www.bceao.int |
| S&P Global | Institut privé / association | payant | 47 | 86 | https://www.pmi.spglobal.com/public |
| Central Statistics Office Ireland | Office statistique national | gratuit (données publiques) | 1 | 84 | https://www.cso.ie/ |
| Statistics Sweden | Office statistique national | gratuit (données publiques) | 1 | 84 | https://www.scb.se/ |
| National Bureau of Statistics of China | Office statistique national | gratuit (données publiques) | 1 | 80 | http://www.stats.gov.cn/ |
| National Institute of Statistics (ISTAT) | Office statistique national | gratuit (données publiques) | 1 | 76 | https://www.istat.it |
| Statistics Denmark | Office statistique national | gratuit (données publiques) | 1 | 76 | https://www.dst.dk/ |
| Statistics New Zealand | Office statistique national | gratuit (données publiques) | 1 | 76 | https://www.stats.govt.nz/ |
| Statistics Finland | Office statistique national | gratuit (données publiques) | 1 | 74 | https://www.stat.fi |
| Bank for International Settlements | Organisation internationale | gratuit (données publiques) | 51 | 72 | https://www.bis.org/ |
| Statistics Portugal | Office statistique national | gratuit (données publiques) | 1 | 71 | http://www.ine.pt |
| U.S. Energy Information Administration | Ministère / agence publique | gratuit (données publiques) | 58 | 71 | http://www.eia.gov |
| Statistics Netherlands | Office statistique national | gratuit (données publiques) | 1 | 70 | https://www.cbs.nl/ |
| Federal Statistical Office | Office statistique national | gratuit (données publiques) | 1 | 69 | https://www.destatis.de |
| Statistical Office of the Slovak Republic | Office statistique national | gratuit (données publiques) | 1 | 69 | https://slovak.statistics.sk/ |
| U.S. Bureau of Labor Statistics | Office statistique national | gratuit (données publiques) | 1 | 69 | http://www.bls.gov/ |
| INSEE, France | Office statistique national | gratuit (données publiques) | 1 | 68 | https://www.insee.fr |
| Central Bureau of Statistics, Israel | Office statistique national | gratuit (données publiques) | 1 | 66 | https://www.cbs.gov.il/ |
| Instituto Nacional de Estadística y Geografía (INEGI) | Office statistique national | gratuit (données publiques) | 1 | 66 | https://www.inegi.org.mx/ |
| Turkish Statistical Institute | Office statistique national | gratuit (données publiques) | 1 | 66 | https://www.tuik.gov.tr/ |
| Statistics South Africa | Office statistique national | gratuit (données publiques) | 1 | 65 | http://www.statssa.gov.za/ |
| European Commission | Organisation internationale | gratuit (données publiques) | 25 | 64 | http://ec.europa.eu |
| Czech Statistical Office | Office statistique national | gratuit (données publiques) | 1 | 63 | https://www.czso.cz |
| Instituto Nacional de Estadística y Censos (INDEC) | Office statistique national | gratuit (données publiques) | 1 | 62 | https://www.indec.gob.ar/ |
| National Statistical Institute, Bulgaria | Office statistique national | gratuit (données publiques) | 1 | 62 | http://www.nsi.bg |
| Central Bank of Sri Lanka | Banque centrale | gratuit (données publiques) | 1 | 61 | https://www.cbsl.gov.lk |
| Central Statistics Office, Mauritius | Office statistique national | gratuit (données publiques) | 1 | 61 | http://statsmauritius.govmu.org |
| World Steel Association | Organisation internationale | gratuit (données publiques) | 61 | 61 | https://www.worldsteel.org/ |
| National Statistics Institute (INE) | Office statistique national | gratuit (données publiques) | 1 | 60 | https://www.ine.es/ |
| Statistical Office of the Republic of Slovenia | Office statistique national | gratuit (données publiques) | 1 | 60 | https://www.stat.si |
| Statistics Singapore | Office statistique national | gratuit (données publiques) | 1 | 60 | https://www.singstat.gov.sg/ |
| Census and Statistics Department, Hong Kong | Office statistique national | gratuit (données publiques) | 1 | 59 | https://www.censtatd.gov.hk |
| Statistics Estonia | Office statistique national | gratuit (données publiques) | 1 | 59 | https://www.stat.ee/en |
| General Authority for Statistics, Saudi Arabia | Office statistique national | gratuit (données publiques) | 1 | 56 | https://www.stats.gov.sa |
| Central Statistical Bureau of Latvia | Office statistique national | gratuit (données publiques) | 1 | 55 | https://stat.gov.lv/en |
| Department of Statistics, Malaysia | Office statistique national | gratuit (données publiques) | 1 | 55 | https://www.dosm.gov.my |
| Federal State Statistics Service | Office statistique national | gratuit (données publiques) | 1 | 55 | https://rosstat.gov.ru/ |
| National Statistical Service of Greece | Office statistique national | gratuit (données publiques) | 1 | 55 | https://www.statistics.gr/ |
| Philippine Statistics Authority | Office statistique national | gratuit (données publiques) | 1 | 55 | https://psa.gov.ph |
| The Bank of Korea | Banque centrale | gratuit (données publiques) | 2 | 55 | https://www.bok.or.kr |
| Ministry of Statistics and Programme Implementation (MOSPI) | Office statistique national | gratuit (données publiques) | 1 | 54 | https://www.mospi.gov.in/ |
| Statistics Austria | Office statistique national | gratuit (données publiques) | 1 | 54 | https://www.statistik.at/ |
| Kosovo Agency of Statistics | Office statistique national | gratuit (données publiques) | 1 | 53 | http://ask.rks-gov.net |
| Statistics Lithuania | Office statistique national | gratuit (données publiques) | 1 | 53 | https://osp.stat.gov.lt/ |
| Ministry of Finance | Ministère / agence publique | gratuit (données publiques) | 28 | 52 | https://www.finances.gov.mr/ |
| State Statistics Service of Ukraine | Office statistique national | gratuit (données publiques) | 1 | 52 | https://stat.gov.ua/en |
| Statistical Service of the Republic of Cyprus | Office statistique national | gratuit (données publiques) | 1 | 52 | https://www.cystat.gov.cy/en/ |
| United Nations Economic Commission for Europe (UNECE) | Organisation internationale | gratuit (données publiques) | 51 | 51 | https://w3.unece.org/pxweb/en/ |
| Instituti i Statistikave (INSTAT) | Office statistique national | gratuit (données publiques) | 1 | 50 | http://www.instat.gov.al |
| Banco Central del Paraguay | Banque centrale | gratuit (données publiques) | 1 | 49 | https://www.bcp.gov.py/ |
| Central Statistical Office of Poland (GUS) | Office statistique national | gratuit (données publiques) | 1 | 49 | https://new.stat.gov.pl/ |
| Croatian Bureau of Statistics | Office statistique national | gratuit (données publiques) | 1 | 49 | https://www.dzs.hr |
| Hungarian Central Statistical Office | Office statistique national | gratuit (données publiques) | 1 | 48 | http://www.ksh.hu |
| National Statistics Office of Georgia | Office statistique national | gratuit (données publiques) | 1 | 48 | http://geostat.ge |
| U.S. Census Bureau | Office statistique national | gratuit (données publiques) | 1 | 48 | https://www.census.gov/ |
| Eastern Caribbean Central Bank | Organisation internationale | gratuit (données publiques) | 4 | 47 | https://www.eccb-centralbank.org/ |
| General Statistics Office of Vietnam | Office statistique national | gratuit (données publiques) | 1 | 47 | https://www.gso.gov.vn |
| National Bureau of Statistics, Nigeria | Office statistique national | gratuit (données publiques) | 1 | 46 | http://www.nigerianstat.gov.ng/ |
| National Statistical Office of Mongolia | Office statistique national | gratuit (données publiques) | 1 | 46 | http://www.1212.mn |
| Instituto Brasileiro de Geografia e Estatística (IBGE) | Office statistique national | gratuit (données publiques) | 1 | 45 | https://www.ibge.gov.br |
| National Statistics Office, Malta | Office statistique national | gratuit (données publiques) | 1 | 45 | http://www.nso.gov.mt |
| Statistical Office of the Republic of Serbia | Office statistique national | gratuit (données publiques) | 1 | 45 | https://www.stat.gov.rs |
| Statistics Iceland | Office statistique national | gratuit (données publiques) | 1 | 45 | http://www.statice.is |
| The State Statistical Committee of the Republic of Azerbaijan | Office statistique national | gratuit (données publiques) | 1 | 45 | http://www.stat.gov.az |
| Bank of Thailand | Banque centrale | gratuit (données publiques) | 1 | 44 | https://www.bot.or.th |
| Central Bank of Costa Rica | Banque centrale | gratuit (données publiques) | 1 | 44 | https://www.bccr.fi.cr |
| IMF | Organisation internationale | gratuit (données publiques) | 37 | 44 | https://www.imf.org |
| National Planning Council, State of Qatar | Ministère / agence publique | gratuit (données publiques) | 1 | 44 | https://www.npc.qa/en/Pages/default.aspx |
| National Bank of Belgium | Banque centrale | gratuit (données publiques) | 1 | 43 | https://www.nbb.be/en |
| National Statistical Service of the Republic of Armenia | Office statistique national | gratuit (données publiques) | 1 | 43 | http://www.armstat.am |
| Palestinian Central Bureau of Statistics | Office statistique national | gratuit (données publiques) | 1 | 43 | http://www.pcbs.gov.ps |
| Statistics Indonesia | Office statistique national | gratuit (données publiques) | 1 | 43 | https://www.bps.go.id/ |
| Banco Central del Ecuador | Banque centrale | gratuit (données publiques) | 1 | 42 | https://www.bce.fin.ec |
| Bosnia and Herzegovina Agency for Statistics (BHAS) | Office statistique national | gratuit (données publiques) | 1 | 42 | http://bhas.gov.ba |
| Central Bank of Dominican Republic | Banque centrale | gratuit (données publiques) | 1 | 42 | http://www.bancentral.gov.do |
| Dane, Colombia | Office statistique national | gratuit (données publiques) | 1 | 42 | https://www.dane.gov.co |
| Institutul National de Statistica | Office statistique national | gratuit (données publiques) | 1 | 42 | http://www.insse.ro |
| State Statistical Office of the Republic of Macedonia | Office statistique national | gratuit (données publiques) | 1 | 42 | https://www.stat.mk |
| Central Bureau of Statistics, Namibia | Office statistique national | gratuit (données publiques) | 1 | 41 | https://nsa.org.na/ |
| Instituto Nacional de Estadística de Bolivia | Office statistique national | gratuit (données publiques) | 1 | 41 | https://www.ine.gob.bo/ |
| Agency of Statistics of the Republic of Kazakhstan | Office statistique national | gratuit (données publiques) | 1 | 40 | https://www.stat.gov.kz/ |
| European Automobile Manufacturers’ Association (ACEA) | Institut privé / association | chiffre-titre gratuit, série payante | 31 | 40 | https://www.acea.auto/ |
| National Bureau of Statistics (NBS) - Tanzania | Office statistique national | gratuit (données publiques) | 1 | 40 | http://www.nbs.go.tz |
| National Institute of Statistics - Tunisia | Office statistique national | gratuit (données publiques) | 1 | 40 | http://www.ins.tn/ |
| Pakistan Bureau of Statistics | Office statistique national | gratuit (données publiques) | 1 | 40 | http://www.pbs.gov.pk |
| ANSD, Senegal | Office statistique national | gratuit (données publiques) | 1 | 39 | http://www.ansd.sn |
| STATEC, Luxembourg | Office statistique national | gratuit (données publiques) | 1 | 39 | https://statistiques.public.lu/fr.html |
| Statistics and Census Service, Government of Macao SAR | Office statistique national | gratuit (données publiques) | 1 | 39 | http://www.dsec.gov.mo |
| Swiss Federal Statistical Office | Office statistique national | gratuit (données publiques) | 1 | 39 | https://www.bfs.admin.ch |
| U.S. Bureau of Economic Analysis | Office statistique national | gratuit (données publiques) | 1 | 39 | http://www.bea.gov |
| Banco Central De Honduras | Banque centrale | gratuit (données publiques) | 1 | 37 | http://www.bch.hn |
| Central Reserve Bank of El Salvador | Banque centrale | gratuit (données publiques) | 1 | 37 | http://www.bcr.gob.sv |
| Haut Commissariat au Plan | Office statistique national | gratuit (données publiques) | 1 | 37 | https://www.hcp.ma/ |
| Banque Centrale du Congo | Banque centrale | gratuit (données publiques) | 1 | 36 | https://www.bcc.cd/ |
| Central Reserve Bank of Peru | Banque centrale | gratuit (données publiques) | 1 | 36 | http://www.bcrp.gob.pe |
| Instituto Nacional De Estatistica, Mozambique | Office statistique national | gratuit (données publiques) | 1 | 36 | https://www.ine.gov.mz/ |
| National Statistical Committee of the Republic of Belarus | Office statistique national | gratuit (données publiques) | 1 | 36 | https://www.belstat.gov.by |
| Statistical Office of Montenegro | Office statistique national | gratuit (données publiques) | 1 | 36 | http://monstat.org |
| Uganda Bureau of Statistics | Office statistique national | gratuit (données publiques) | 1 | 36 | https://www.ubos.org/ |
| Bangladesh Bank | Banque centrale | gratuit (données publiques) | 1 | 35 | https://www.bb.org.bd/ |
| Central Bank of Trinidad and Tobago | Banque centrale | gratuit (données publiques) | 1 | 35 | https://www.central-bank.org.tt/ |
| INE, Cape Verde | Office statistique national | gratuit (données publiques) | 1 | 35 | http://ine.cv/ |
| International Monetary Fund | Organisation internationale | gratuit (données publiques) | 26 | 35 | https://www.imf.org/en/Home |
| Kenya National Bureau of Statistics | Office statistique national | gratuit (données publiques) | 1 | 35 | https://www.knbs.or.ke/ |
| National Bureau of Statistics of the Republic of Moldova | Office statistique national | gratuit (données publiques) | 1 | 34 | http://statbank.statistica.md |
| National Bureau of Statistics, Seychelles | Office statistique national | gratuit (données publiques) | 1 | 34 | https://www.nbs.gov.sc |
| National Institute of Statistics of Rwanda | Office statistique national | gratuit (données publiques) | 1 | 34 | http://statistics.gov.rw/ |
| South African Reserve Bank | Banque centrale | gratuit (données publiques) | 1 | 34 | http://www.resbank.co.za |
| Banco de Guatemala | Banque centrale | gratuit (données publiques) | 1 | 33 | https://banguat.gob.gt/ |
| Instituto Nacional de Estadística e Informática (INEI) | Office statistique national | gratuit (données publiques) | 1 | 33 | https://www.inei.gob.pe/ |
| Office National des Statistiques (ONS) | Office statistique national | gratuit (données publiques) | 1 | 33 | http://www.ons.dz/ |
| Statistics Korea | Office statistique national | gratuit (données publiques) | 1 | 33 | http://kostat.go.kr |
| Banco Central del Uruguay | Banque centrale | gratuit (données publiques) | 1 | 31 | https://www.bcu.gub.uy/ |
| Banco Central do Brasil | Banque centrale | gratuit (données publiques) | 1 | 31 | https://www.bcb.gov.br/ |
| Bank of Central African States | Organisation internationale | gratuit (données publiques) | 6 | 31 | https://www.beac.int/ |
| Bank of Japan | Banque centrale | gratuit (données publiques) | 1 | 30 | https://www.boj.or.jp |
| Cabinet Office, Japan | Ministère / agence publique | gratuit (données publiques) | 1 | 30 | https://www.cao.go.jp |
| Central Bank of Chile | Banque centrale | gratuit (données publiques) | 1 | 30 | https://www.bcentral.cl |
| International Labour Organization (ILO) | Organisation internationale | gratuit (données publiques) | 28 | 30 | https://www.ilo.org/global/lang--en/index.htm |
| Maldives Monetary Authority | Banque centrale | gratuit (données publiques) | 1 | 30 | http://www.mma.gov.mv |
| National Statistical Committee of Kyrgyz Republic | Office statistique national | gratuit (données publiques) | 1 | 30 | http://stat.kg |
| Saudi Central Bank | Banque centrale | gratuit (données publiques) | 1 | 30 | http://www.sama.gov.sa |
| Statistics Belgium | Office statistique national | gratuit (données publiques) | 1 | 30 | https://statbel.fgov.be |
| Instituto Nacional de Estatística, Angola | Office statistique national | gratuit (données publiques) | 1 | 29 | https://www.ine.gov.ao/ |
| Statistical Institute of Jamaica | Office statistique national | gratuit (données publiques) | 1 | 29 | http://statinja.gov.jm |
| Central Bank of Jordan | Banque centrale | gratuit (données publiques) | 1 | 28 | http://www.cbj.gov.jo |
| Central Bank of Nigeria | Banque centrale | gratuit (données publiques) | 1 | 28 | https://www.cbn.gov.ng/ |
| Central Bank of the Republic of Turkey | Banque centrale | gratuit (données publiques) | 1 | 28 | https://www.tcmb.gov.tr |
| Central Statistics Office, Botswana | Office statistique national | gratuit (données publiques) | 1 | 28 | https://www.statsbots.org.bw/ |
| National Institute of Statistics, Chile | Office statistique national | gratuit (données publiques) | 1 | 28 | https://www.ine.cl/ |
| National Statistics and Information Authority (NSIA) | Office statistique national | gratuit (données publiques) | 1 | 28 | https://nsia.gov.af/home |
| Banco Central De Venezuela | Banque centrale | gratuit (données publiques) | 1 | 27 | http://www.bcv.org.ve/ |
| Banco Central de Nicaragua | Banque centrale | gratuit (données publiques) | 1 | 27 | http://www.bcn.gob.ni |
| Bank Indonesia | Banque centrale | gratuit (données publiques) | 1 | 27 | https://www.bi.go.id |
| Croatian National Bank | Banque centrale | gratuit (données publiques) | 1 | 27 | http://www.hnb.hr |
| Department of Economic Planning and Development, Brunei | Ministère / agence publique | gratuit (données publiques) | 1 | 27 | https://deps.mofe.gov.bn/Theme/Home.aspx |
| Federal Competitiveness and Statistics Centre | Office statistique national | gratuit (données publiques) | 1 | 27 | https://fcsc.gov.ae/ |
| National Centre for Statistic & Information, Oman | Office statistique national | gratuit (données publiques) | 1 | 27 | https://www.ncsi.gov.om |
| Reserve Bank of India | Banque centrale | gratuit (données publiques) | 1 | 27 | https://www.rbi.org.in |
| Central Bank of Kuwait | Banque centrale | gratuit (données publiques) | 1 | 26 | http://www.cbk.gov.kw |
| Central Bank of Seychelles | Banque centrale | gratuit (données publiques) | 1 | 26 | http://www.cbs.sc |
| Central Statistical Office, Zambia | Office statistique national | gratuit (données publiques) | 1 | 26 | https://www.zamstats.gov.zm/ |
| Deutsche Bundesbank | Banque centrale | gratuit (données publiques) | 1 | 26 | https://www.bundesbank.de |
| Statistical Institute of Puerto Rico | Office statistique national | gratuit (données publiques) | 1 | 26 | https://www.estadisticas.pr.gov/ |
| Central Bank of Belize | Banque centrale | gratuit (données publiques) | 1 | 25 | https://www.centralbank.org.bz |
| Instituto Nacional de Estadística y Censo - Panamá | Office statistique national | gratuit (données publiques) | 1 | 25 | http://www.contraloria.gob.pa |
| State Bank of Pakistan | Banque centrale | gratuit (données publiques) | 1 | 25 | http://www.sbp.org.pk |
| State Committee on Statistics of Uzbekistan | Office statistique national | gratuit (données publiques) | 1 | 25 | http://stat.uz |
| Statistical Institute of Belize | Office statistique national | gratuit (données publiques) | 1 | 25 | http://www.sib.org.bz |
| Statistics Bureau of Japan | Office statistique national | gratuit (données publiques) | 1 | 25 | https://www.stat.go.jp |
| Bangko Sentral ng Pilipinas | Banque centrale | gratuit (données publiques) | 1 | 24 | http://www.bsp.gov.ph |
| Bank of England | Banque centrale | gratuit (données publiques) | 1 | 24 | https://www.bankofengland.co.uk/ |
| Central Bank of Bosnia and Herzegovina | Banque centrale | gratuit (données publiques) | 1 | 24 | https://www.cbbh.ba/?lang=en |
| Central Bank of The Gambia | Banque centrale | gratuit (données publiques) | 1 | 24 | https://www.cbg.gm/ |
| Czech National Bank | Banque centrale | gratuit (données publiques) | 1 | 24 | http://www.cnb.cz |
| Instituto Nacional de Estadística, Uruguay | Office statistique national | gratuit (données publiques) | 1 | 24 | https://www.gub.uy/instituto-nacional-estadistica/ |
| National Statistical Office of Malawi | Office statistique national | gratuit (données publiques) | 1 | 24 | http://www.nsomalawi.mw/ |
| People's Bank of China | Banque centrale | gratuit (données publiques) | 1 | 24 | http://www.pbc.gov.cn/ |
| Banco de México | Banque centrale | gratuit (données publiques) | 1 | 23 | https://www.banxico.org.mx/ |
| Bank of Burundi | Banque centrale | gratuit (données publiques) | 1 | 23 | http://www.brb.bi |
| Bank of the Lao PDR | Banque centrale | gratuit (données publiques) | 1 | 23 | http://www.bol.gov.la |
| Central Bank of Swaziland | Banque centrale | gratuit (données publiques) | 1 | 23 | https://www.centralbank.org.sz |
| INEC, Ecuador | Office statistique national | gratuit (données publiques) | 1 | 23 | https://www.ecuadorencifras.gob.ec |
| Institut National de la Statistique du Cameroun | Office statistique national | gratuit (données publiques) | 1 | 23 | http://www.statistics-cameroon.org |
| Lesotho Bureau of Statistics | Office statistique national | gratuit (données publiques) | 1 | 23 | http://www.bos.gov.ls/index.htm |
| National Bank of Poland | Banque centrale | gratuit (données publiques) | 1 | 23 | https://www.nbp.pl/ |
| The Department of Census and Statistics, Sri Lanka | Office statistique national | gratuit (données publiques) | 1 | 23 | http://www.statistics.gov.lk/ |
| Banco de Portugal | Banque centrale | gratuit (données publiques) | 1 | 22 | https://www.bportugal.pt |
| Bangladesh Bureau of Statistics | Office statistique national | gratuit (données publiques) | 1 | 22 | http://www.bbs.gov.bd/ |
| Bank of Botswana | Banque centrale | gratuit (données publiques) | 1 | 22 | http://www.bankofbotswana.bw |
| Bank of Spain | Banque centrale | gratuit (données publiques) | 1 | 22 | https://www.bde.es |
| Bank of Uganda | Banque centrale | gratuit (données publiques) | 1 | 22 | https://www.bou.or.ug |
| Central Bank of Egypt | Banque centrale | gratuit (données publiques) | 1 | 22 | https://www.cbe.org.eg |
| Central Bank of Kenya | Banque centrale | gratuit (données publiques) | 1 | 22 | https://www.centralbank.go.ke/ |
| Central Bank of Russia | Banque centrale | gratuit (données publiques) | 1 | 22 | http://www.cbr.ru/ |
| Federal Reserve Bank of New York | Banque centrale | gratuit (données publiques) | 2 | 22 | https://www.newyorkfed.org |
| Instituto Nacional de Estadística y Censos de Costa Rica | Office statistique national | gratuit (données publiques) | 1 | 22 | https://inec.cr/ |
| Inter-American Development Bank | Organisation internationale | gratuit (données publiques) | 22 | 22 | https://www.iadb.org/en |
| National Bank of Cambodia | Banque centrale | gratuit (données publiques) | 1 | 22 | https://www.nbc.gov.kh/english/ |
| National Office of Statistics, Republic of Cuba | Office statistique national | gratuit (données publiques) | 1 | 22 | http://www.onei.gob.cu/ |
| Bank of Albania | Banque centrale | gratuit (données publiques) | 1 | 21 | https://www.bankofalbania.org |
| Bank of Ghana | Banque centrale | gratuit (données publiques) | 1 | 21 | https://www.bog.gov.gh/ |
| Bank of Israel | Banque centrale | gratuit (données publiques) | 1 | 21 | http://www.boi.org.il |
| Central Bank of Bahrain | Banque centrale | gratuit (données publiques) | 1 | 21 | http://www.cbb.gov.bh |
| DGBAS, Taiwan | Office statistique national | gratuit (données publiques) | 1 | 21 | https://eng.dgbas.gov.tw/ |
| Federal Reserve | Banque centrale | gratuit (données publiques) | 1 | 21 | http://www.federalreserve.gov |
| Fiji Bureau of Statistics | Office statistique national | gratuit (données publiques) | 1 | 21 | https://www.statsfiji.gov.fj |
| Ghana Statistical Service | Office statistique national | gratuit (données publiques) | 1 | 21 | https://statsghana.gov.gh |
| ISEE, New Caledonia | Office statistique national | gratuit (données publiques) | 1 | 21 | http://www.isee.nc |
| Information & eGovernment Authority, Bahrain | Ministère / agence publique | gratuit (données publiques) | 1 | 21 | https://www.data.gov.bh/ |
| Ministry of Finance, Japan | Ministère / agence publique | gratuit (données publiques) | 1 | 21 | http://www.mof.go.jp |
| National Bank of Angola | Banque centrale | gratuit (données publiques) | 1 | 21 | https://www.bna.ao/ |
| National Bank of Hungary | Banque centrale | gratuit (données publiques) | 1 | 21 | https://www.mnb.hu |
| Statistical Center of Iran | Office statistique national | gratuit (données publiques) | 1 | 21 | https://www.amar.org.ir |
| Banco de Cabo Verde | Banque centrale | gratuit (données publiques) | 1 | 20 | http://www.bcv.cv |
| Bank of Greece | Banque centrale | gratuit (données publiques) | 1 | 20 | https://www.bankofgreece.gr |
| Bank of Tanzania | Banque centrale | gratuit (données publiques) | 1 | 20 | https://www.bot.go.tz |
| Central Bank of Armenia | Banque centrale | gratuit (données publiques) | 1 | 20 | https://www.cba.am |
| Central Bank of the Republic of Azerbaijan | Banque centrale | gratuit (données publiques) | 1 | 20 | https://www.cbar.az/ |
| Reserve Bank of Malawi | Banque centrale | gratuit (données publiques) | 1 | 20 | https://www.rbm.mw/ |
| Central Bank of Djibouti | Banque centrale | gratuit (données publiques) | 1 | 19 | http://www.banque-centrale.dj |
| Central Bank of Liberia | Banque centrale | gratuit (données publiques) | 1 | 19 | https://www.cbl.org.lr/ |
| Central Bank of Malaysia | Banque centrale | gratuit (données publiques) | 1 | 19 | http://www.bnm.gov.my |
| Department Of Statistics (Jordan) | Office statistique national | gratuit (données publiques) | 1 | 19 | http://www.dos.gov.jo |
| National Bank of Tajikistan | Banque centrale | gratuit (données publiques) | 1 | 19 | http://www.nbt.tj |
| National Bank of Ukraine | Banque centrale | gratuit (données publiques) | 1 | 19 | https://bank.gov.ua/ |
| National Bank of the Kyrgyz Republic | Banque centrale | gratuit (données publiques) | 1 | 19 | http://www.nbkr.kg |
| National Bank of the Republic of Belarus | Banque centrale | gratuit (données publiques) | 1 | 19 | http://www.nbrb.by |
| National Statistics Directorate, Timor | Office statistique national | gratuit (données publiques) | 1 | 19 | https://inetl-ip.gov.tl/ |
| Reserve Bank of Australia | Banque centrale | gratuit (données publiques) | 1 | 19 | http://www.rba.gov.au/ |
| Swiss National Bank | Banque centrale | gratuit (données publiques) | 1 | 19 | https://www.snb.ch |
| Banca d'Italia | Banque centrale | gratuit (données publiques) | 1 | 18 | https://www.bancaditalia.it/ |
| Bank of Jamaica | Banque centrale | gratuit (données publiques) | 1 | 18 | http://www.boj.org.jm |
| Bank of Lithuania | Banque centrale | gratuit (données publiques) | 1 | 18 | https://www.lb.lt/en/ |
| Bank of Namibia | Banque centrale | gratuit (données publiques) | 1 | 18 | https://www.bon.com.na/ |
| Banque Du Liban | Banque centrale | gratuit (données publiques) | 1 | 18 | http://www.bdl.gov.lb |
| Banque de France | Banque centrale | gratuit (données publiques) | 1 | 18 | https://www.banque-france.fr/ |
| Central Bank of Iran | Banque centrale | gratuit (données publiques) | 1 | 18 | http://www.cbi.ir |
| Central Bank of Iraq | Banque centrale | gratuit (données publiques) | 1 | 18 | http://www.cbi.iq |
| Central Bank of Tunisia | Banque centrale | gratuit (données publiques) | 1 | 18 | http://www.bct.gov.tn |
| Instituto Nacional de Estadística Guatemala | Office statistique national | gratuit (données publiques) | 1 | 18 | https://www.ine.gob.gt/ |
| National Bank of Romania | Banque centrale | gratuit (données publiques) | 1 | 18 | http://www.bnr.ro |
| National Bank of Serbia | Banque centrale | gratuit (données publiques) | 1 | 18 | http://www.nbs.rs |
| Nesdb, Thailand | Office statistique national | gratuit (données publiques) | 1 | 18 | https://www.nesdb.go.th |
| Qatar Central Bank | Banque centrale | gratuit (données publiques) | 1 | 18 | http://www.qcb.gov.qa |
| Statistics Faroe Islands | Office statistique national | gratuit (données publiques) | 1 | 18 | http://statbank.hagstova.fo |
| Banque Centrale Du Luxembourg | Banque centrale | gratuit (données publiques) | 1 | 17 | https://www.bcl.lu/ |
| Central Bank of Suriname | Banque centrale | gratuit (données publiques) | 1 | 17 | https://www.cbvs.sr |
| Central Bank of The Bahamas | Banque centrale | gratuit (données publiques) | 1 | 17 | http://www.centralbankbahamas.com |
| De Nederlandsche Bank | Banque centrale | gratuit (données publiques) | 1 | 17 | https://www.dnb.nl/ |
| Oesterreichische Nationalbank | Banque centrale | gratuit (données publiques) | 1 | 17 | https://www.oenb.at/en/ |
| Baker Hughes Company | Institut privé / association | chiffre-titre gratuit, série payante | 15 | 16 | https://www.bakerhughes.com/ |
| Banco de la República de Colombia | Banque centrale | gratuit (données publiques) | 1 | 16 | https://www.banrep.gov.co |
| Bank of Zambia | Banque centrale | gratuit (données publiques) | 1 | 16 | https://www.boz.zm/ |
| Banque Centrale des Comores | Banque centrale | gratuit (données publiques) | 1 | 16 | http://www.banque-comores.km |
| Bulgarian National Bank | Banque centrale | gratuit (données publiques) | 1 | 16 | https://www.bnb.bg/ |
| Central Bank of the Republic of China (Taiwan) | Banque centrale | gratuit (données publiques) | 1 | 16 | http://www.cbc.gov.tw |
| Danmarks Nationalbank | Banque centrale | gratuit (données publiques) | 1 | 16 | http://www.nationalbanken.dk |
| INIDE: Instituto Nacional de Información de Desarrollo | Office statistique national | gratuit (données publiques) | 1 | 16 | https://www.inide.gob.ni/ |
| National Bank of Moldova | Banque centrale | gratuit (données publiques) | 1 | 16 | http://www.bnm.md |
| National Bank of the Republic of Macedonia | Banque centrale | gratuit (données publiques) | 1 | 16 | http://www.nbrm.mk |
| Reserve Bank of New Zealand | Banque centrale | gratuit (données publiques) | 1 | 16 | https://www.rbnz.govt.nz |
| SESRIC | Organisation internationale | gratuit (données publiques) | 7 | 16 | https://www.sesric.org/index.php |
| Banco Central de Bolivia | Banque centrale | gratuit (données publiques) | 1 | 15 | https://www.bcb.gob.bo/ |
| Bank of Mauritius | Banque centrale | gratuit (données publiques) | 1 | 15 | https://www.bom.mu |
| Barbados Statistical Service | Office statistique national | gratuit (données publiques) | 1 | 15 | https://stats.gov.bb/ |
| Central Bank Of Barbados | Banque centrale | gratuit (données publiques) | 1 | 15 | http://www.centralbank.org.bb/ |
| Central Bank of Iceland | Banque centrale | gratuit (données publiques) | 1 | 15 | https://www.cb.is |
| Central Bank of Malta | Banque centrale | gratuit (données publiques) | 1 | 15 | http://www.centralbankmalta.org |
| Central Bank of Oman | Banque centrale | gratuit (données publiques) | 1 | 15 | https://cbo.gov.om/ |
| Department of Statistics (DOS), Jordan | Office statistique national | gratuit (données publiques) | 1 | 15 | http://dosweb.dos.gov.jo/ |
| General Direction of Taxes | Ministère / agence publique | gratuit (données publiques) | 7 | 15 | https://www.impots.gov.bf/ |
| Institute for Supply Management | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 15 | https://www.ismworld.org |
| Kuwait Central Statistical Bureau | Office statistique national | gratuit (données publiques) | 1 | 15 | https://www.csb.gov.kw/Default_en |
| Lao Statistical Bureau | Office statistique national | gratuit (données publiques) | 1 | 15 | https://www.lsb.gov.la/ |
| Ministry of Labor and Social Security | Ministère / agence publique | gratuit (données publiques) | 4 | 15 | https://www.ailevecalisma.gov.tr/ |
| Nepal Rastra Bank | Banque centrale | gratuit (données publiques) | 1 | 15 | https://nrb.org.np |
| Oficina Nacional de Estadística y Censos | Office statistique national | gratuit (données publiques) | 1 | 15 | https://onec.bcr.gob.sv/ |
| Royal Monetary Authority of Bhutan | Banque centrale | gratuit (données publiques) | 1 | 15 | https://www.rma.org.bt |
| ANSADE, Mauritania | Office statistique national | gratuit (données publiques) | 1 | 14 | https://ansade.mr/fr/ |
| African Development Bank Group | Organisation internationale | gratuit (données publiques) | 11 | 14 | https://www.afdb.org/en |
| Aruba Central Bureau of Statistics | Office statistique national | gratuit (données publiques) | 1 | 14 | http://www.cbs.aw |
| BCEAO | Organisation internationale | gratuit (données publiques) | 7 | 14 | https://www.bceao.int/fr |
| Cayman Islands - The Economics & Statistics Office | Office statistique national | gratuit (données publiques) | 1 | 14 | http://www.eso.ky |
| Central Bureau of Statistics, Nepal | Office statistique national | gratuit (données publiques) | 1 | 14 | http://www.dataforall.org |
| GBoS - Gambia Bureau of Statistics | Office statistique national | gratuit (données publiques) | 1 | 14 | https://www.gbosdata.org/ |
| General Administration of Customs | Ministère / agence publique | gratuit (données publiques) | 1 | 14 | http://www.customs.gov.cn |
| National Bureau of Statistics, South Sudan | Office statistique national | gratuit (données publiques) | 1 | 14 | https://nbs.gov.ss |
| State Secretariat for Economic Affairs | Ministère / agence publique | gratuit (données publiques) | 1 | 14 | https://www.seco.admin.ch |
| The National Bank of Kazakhstan | Banque centrale | gratuit (données publiques) | 1 | 14 | http://www.nationalbank.kz |
| Banco Central de Chile | Banque centrale | gratuit (données publiques) | 1 | 13 | https://www.bcentral.cl |
| Bank of Estonia | Banque centrale | gratuit (données publiques) | 1 | 13 | http://statistika.eestipank.ee |
| Bank of Guyana | Banque centrale | gratuit (données publiques) | 1 | 13 | http://www.bankofguyana.org.gy |
| Bank of Korea | Banque centrale | gratuit (données publiques) | 2 | 13 | https://www.bok.or.kr |
| Bank of Mongolia | Banque centrale | gratuit (données publiques) | 1 | 13 | https://www.mongolbank.mn |
| Bank of Papua New Guinea | Banque centrale | gratuit (données publiques) | 1 | 13 | https://www.bankpng.gov.pg |
| Bank of Sierra Leone | Banque centrale | gratuit (données publiques) | 1 | 13 | https://bsl.gov.sl/ |
| Bank of Slovenia | Banque centrale | gratuit (données publiques) | 1 | 13 | https://www.bsi.si/en |
| Central Bank of Lesotho | Banque centrale | gratuit (données publiques) | 1 | 13 | http://www.centralbank.org.ls |
| Central Bank of the United Arab Emirates | Banque centrale | gratuit (données publiques) | 1 | 13 | https://www.centralbank.ae |
| Directorate-General of Budget, Accounting and Statistics, Taiwan | Office statistique national | gratuit (données publiques) | 1 | 13 | https://eng.dgbas.gov.tw/Default.aspx |
| Europe's Energy Portal | Institut privé / association | chiffre-titre gratuit, série payante | 13 | 13 | https://www.energy.eu |
| Institut National de la Statistique du Niger | Office statistique national | gratuit (données publiques) | 1 | 13 | http://www.stat-niger.org |
| Ministry of Health, Labour and Welfare, Japan | Ministère / agence publique | gratuit (données publiques) | 1 | 13 | https://www.mhlw.go.jp |
| Ministry of Internal Affairs & Communications | Office statistique national | gratuit (données publiques) | 1 | 13 | http://www.stat.go.jp |
| Monetary Authority of Singapore | Banque centrale | gratuit (données publiques) | 1 | 13 | https://www.mas.gov.sg/ |
| National Bank of Rwanda | Banque centrale | gratuit (données publiques) | 1 | 13 | http://www.bnr.rw |
| National Statistics Bureau of Bhutan | Office statistique national | gratuit (données publiques) | 1 | 13 | https://www.nsb.gov.bt/ |
| Organization of the Petroleum Exporting Countries | Organisation internationale | gratuit (données publiques) | 13 | 13 | https://www.opec.org/ |
| Reserve Bank of Zimbabwe | Banque centrale | gratuit (données publiques) | 1 | 13 | https://www.rbz.co.zw/ |
| Central Bank of Sao Tome and Principe | Banque centrale | gratuit (données publiques) | 1 | 12 | http://www.bcstp.st |
| Central Bureau Of Statistics, Syrian Arab Republic | Office statistique national | gratuit (données publiques) | 1 | 12 | http://cbssyr.sy/index.htm |
| Institut National de la Statistique, Mali | Office statistique national | gratuit (données publiques) | 1 | 12 | http://www.instat-mali.org |
| Liberia Institute of Statistics and Geo-Information Services | Office statistique national | gratuit (données publiques) | 1 | 12 | https://lisgis.gov.lr/ |
| National Bank of Georgia | Banque centrale | gratuit (données publiques) | 1 | 12 | https://www.nbg.gov.ge |
| National Social Security Fund | Ministère / agence publique | gratuit (données publiques) | 4 | 12 |  |
| National Statistics Bureau Bhutan | Office statistique national | gratuit (données publiques) | 1 | 12 | https://www.nsb.gov.bt/ |
| Office of the Economic Advisor, India | Ministère / agence publique | gratuit (données publiques) | 1 | 12 | https://eaindustry.nic.in/ |
| U.S. Department of Labor | Ministère / agence publique | gratuit (données publiques) | 1 | 12 | https://www.bls.gov |
| Zimbabwe National Statistics Agency | Office statistique national | gratuit (données publiques) | 1 | 12 | https://zimstat.co.zw/ |
| Asian Development Bank (ADB) | Organisation internationale | gratuit (données publiques) | 2 | 11 | https://kidb.adb.org/economies/ |
| Banco de Moçambique | Banque centrale | gratuit (données publiques) | 1 | 11 | https://www.bancomoc.mz/ |
| Bermuda Government Department of Statistics | Office statistique national | gratuit (données publiques) | 1 | 11 | https://www.gov.bm/ |
| CAPMAS, Egypt | Office statistique national | gratuit (données publiques) | 1 | 11 | https://www.capmas.gov.eg |
| Central Bank of Libya | Banque centrale | gratuit (données publiques) | 1 | 11 | https://cbl.gov.ly/ |
| Central Bank of Uzbekistan | Banque centrale | gratuit (données publiques) | 1 | 11 | http://www.cbu.uz |
| Central Bank of the Republic of Kosovo | Banque centrale | gratuit (données publiques) | 1 | 11 | http://bqk-kos.org |
| National Bureau of Statistics Antigua and Barbuda | Office statistique national | gratuit (données publiques) | 1 | 11 | https://statistics.gov.ag/ |
| Sveriges Riksbank | Banque centrale | gratuit (données publiques) | 1 | 11 | https://www.riksbank.se/ |
| U.S. Department of the Treasury | Ministère / agence publique | gratuit (données publiques) | 1 | 11 | https://home.treasury.gov/ |
| Agency of the Republic of Kazakhstan on Statistics | Office statistique national | gratuit (données publiques) | 1 | 10 | http://www.stat.gov.kz |
| Bank of Algeria | Banque centrale | gratuit (données publiques) | 1 | 10 | http://www.bank-of-algeria.dz |
| Bank of Canada | Banque centrale | gratuit (données publiques) | 1 | 10 | https://www.bankofcanada.ca/ |
| Bank of Finland | Banque centrale | gratuit (données publiques) | 1 | 10 | https://www.suomenpankki.fi |
| Bank of Latvia | Banque centrale | gratuit (données publiques) | 1 | 10 | https://www.bank.lv/en/ |
| Central Bank of Ireland | Banque centrale | gratuit (données publiques) | 1 | 10 | https://www.centralbank.ie/ |
| Hong Kong Monetary Authority | Banque centrale | gratuit (données publiques) | 1 | 10 | https://www.hkma.gov.hk |
| Institut National de la Statistique de Madagascar | Office statistique national | gratuit (données publiques) | 1 | 10 | http://www.instat.mg |
| Maldives Bureau of Statistics | Office statistique national | gratuit (données publiques) | 1 | 10 | https://statisticsmaldives.gov.mv/ |
| Ministry of Economy Trade & Industry (METI) | Ministère / agence publique | gratuit (données publiques) | 1 | 10 | https://www.meti.go.jp |
| Ministry of Finance, Government of India | Ministère / agence publique | gratuit (données publiques) | 1 | 10 | https://finmin.nic.in/ |
| Mongolbank | Banque centrale | gratuit (données publiques) | 1 | 10 | http://www.mongolbank.mn |
| National Bank of Ethiopia | Banque centrale | gratuit (données publiques) | 1 | 10 | http://www.nbe.gov.et |
| Reserve Bank of Fiji | Banque centrale | gratuit (données publiques) | 1 | 10 | https://www.rbf.gov.fj/reserve-bank-of-fiji/ |
| State statistical committee of the Republic of Tajikistan | Office statistique national | gratuit (données publiques) | 1 | 10 | https://www.stat.tj/tj |
| Syrian Center for Policy Research | Ministère / agence publique | gratuit (données publiques) | 1 | 10 | https://scpr-syria.org/ |
| Bundeszentralamt für Steuern | Ministère / agence publique | gratuit (données publiques) | 1 | 9 | https://www.bzst.de |
| Central Bank of Argentina | Banque centrale | gratuit (données publiques) | 1 | 9 | http://www.bcra.gob.ar |
| Central Bank of Cyprus | Banque centrale | gratuit (données publiques) | 1 | 9 | https://www.centralbank.cy/en/home |
| Central Bank of Lybia | Banque centrale | gratuit (données publiques) | 1 | 9 | http://cbl.gov.ly |
| Danish Central Tax Administration | Ministère / agence publique | gratuit (données publiques) | 1 | 9 | http://ec.europa.eu |
| Directorate-General of Budget, Accounting and Statistics, Executive Yuan, Taiwan | Office statistique national | gratuit (données publiques) | 1 | 9 | https://eng.dgbas.gov.tw/ |
| Federal Ministry of Finance | Ministère / agence publique | gratuit (données publiques) | 1 | 9 | https://www.vatlive.com |
| Federal Reserve Bank of Dallas | Banque centrale | gratuit (données publiques) | 1 | 9 | https://www.dallasfed.org |
| HM Revenue & Customs | Ministère / agence publique | gratuit (données publiques) | 1 | 9 | https://www.gov.uk |
| Institut National de la Statistique de Guinée | Office statistique national | gratuit (données publiques) | 1 | 9 | http://www.stat-guinee.org |
| Institut National de la Statistique et de la Demographie, Burkina Faso | Office statistique national | gratuit (données publiques) | 1 | 9 | http://www.insd.bf |
| Ministry of Finance, Bangladesh | Ministère / agence publique | gratuit (données publiques) | 1 | 9 | https://mof.gov.bd/ |
| Ministry of Finance, R.O.C | Ministère / agence publique | gratuit (données publiques) | 1 | 9 | https://www.mof.gov.tw |
| Ministry of Finance, Republic of Serbia | Ministère / agence publique | gratuit (données publiques) | 1 | 9 | https://www.ncbi.nlm.nih.gov |
| Ministry of Planning and Economic Development | Ministère / agence publique | gratuit (données publiques) | 1 | 9 | https://mped.gov.eg/ |
| National Bank of Slovakia | Banque centrale | gratuit (données publiques) | 1 | 9 | http://www.nbs.sk |
| National Center for Statistics & Information - Oman | Office statistique national | gratuit (données publiques) | 1 | 9 | https://www.ncsi.gov.om |
| National Tax and Customs Administration of Hungary | Ministère / agence publique | gratuit (données publiques) | 1 | 9 | https://nav.gov.hu/ |
| State Administration of Taxation | Ministère / agence publique | gratuit (données publiques) | 1 | 9 | http://www.chinatax.gov.cn |
| Tax Administration of the Republic of Slovenia | Ministère / agence publique | gratuit (données publiques) | 1 | 9 | https://e-uprava.gov.si |
| The Office of the Revenue Commissioners | Ministère / agence publique | gratuit (données publiques) | 1 | 9 | https://revenue.ie/ |
| American Petroleum Institute (API) | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 8 | http://www.api.org |
| Asian Development Bank | Organisation internationale | gratuit (données publiques) | 3 | 8 | http://www.adb.org |
| Australian Taxation Office | Ministère / agence publique | gratuit (données publiques) | 1 | 8 | http://www.humanservices.gov.au |
| Bank Al-Maghrib | Banque centrale | gratuit (données publiques) | 1 | 8 | http://www.bkam.ma/ |
| Central Bank of Montenegro | Banque centrale | gratuit (données publiques) | 1 | 8 | https://www.cbcg.me/en |
| Central Bank of Republic of Guinea | Banque centrale | gratuit (données publiques) | 1 | 8 | http://bcrg-guinee.org |
| Central Statistics Organization, Myanmar | Office statistique national | gratuit (données publiques) | 1 | 8 | https://www.csostat.gov.mm/ |
| Chilean Tax Administration | Ministère / agence publique | gratuit (données publiques) | 1 | 8 |  |
| Consejo Monetario Centroamericano | Organisation internationale | gratuit (données publiques) | 5 | 8 | http://www.secmca.org |
| Consejo Monetario Centroamericanono | Organisation internationale | gratuit (données publiques) | 5 | 8 | http://www.secmca.org |
| Departamento del Trabajo y Recursos Humanos, Puerto Rico | Ministère / agence publique | gratuit (données publiques) | 1 | 8 | http://www.mercadolaboral.pr.gov |
| Direktorat Jenderal Pajak | Ministère / agence publique | gratuit (données publiques) | 1 | 8 | https://www.pajak.go.id |
| Estonian Tax and Customs Board | Ministère / agence publique | gratuit (données publiques) | 1 | 8 | http://www.sotsiaalkindlustusamet.ee |
| INSD, Burkina Faso | Office statistique national | gratuit (données publiques) | 1 | 8 | http://www.insd.bf |
| Inland Revenue Authority of Singapore | Ministère / agence publique | gratuit (données publiques) | 1 | 8 | https://www.iras.gov.sg |
| Institut National de la Statistique et de la Démographie (INStaD) | Office statistique national | gratuit (données publiques) | 1 | 8 | https://instad.bj/ |
| International Labor Organization (ILO) | Organisation internationale | gratuit (données publiques) | 8 | 8 | https://ilostat.ilo.org/data/ |
| MNI Deutsche Börse Group | Institut privé / association | chiffre-titre gratuit, série payante | 5 | 8 |  |
| National Administration of the Principality of Liechtenstein | Ministère / agence publique | gratuit (données publiques) | 1 | 8 | http://www.llv.li/ |
| New Zealand Inland Revenue Department | Ministère / agence publique | gratuit (données publiques) | 1 | 8 | https://www.newzealandnow.govt.nz |
| Norwegian Tax Administration | Ministère / agence publique | gratuit (données publiques) | 1 | 8 | https://www.nav.no |
| SESRIC - The Statistical, Economic & Social Res. & Training Centre for Islamic Countries | Organisation internationale | gratuit (données publiques) | 1 | 8 | https://www.sesric.org/ |
| Somali National Bureau Of Statistics | Office statistique national | gratuit (données publiques) | 1 | 8 | https://nbs.gov.so/ |
| Statistics Sierra Leone | Office statistique national | gratuit (données publiques) | 1 | 8 | https://www.statistics.sl |
| U.S. Bureau of Economic Analysis (BEA) | Office statistique national | gratuit (données publiques) | 1 | 8 | https://www.bea.gov/ |
| Vanuatu Bureau of Statistics | Office statistique national | gratuit (données publiques) | 1 | 8 | https://vnso.gov.vu/index.php/en/ |
| ACEA - European Automobile Manufacturers' Association | Institut privé / association | chiffre-titre gratuit, série payante | 7 | 7 | https://www.acea.auto/ |
| Albanian Taxation Office | Ministère / agence publique | gratuit (données publiques) | 1 | 7 | https://www.tatime.gov.al/eng/ |
| Bureau of Trade and Economic Indices, Ministry of Commerce, Thailand | Ministère / agence publique | gratuit (données publiques) | 1 | 7 | http://www.price.moc.go.th |
| Canada Revenue Agency | Ministère / agence publique | gratuit (données publiques) | 1 | 7 | https://www.canada.ca |
| Central Administration of Statistics, Republic of Lebanon | Office statistique national | gratuit (données publiques) | 1 | 7 | http://www.cas.gov.lb |
| Central Bank of Honduras | Banque centrale | gratuit (données publiques) | 1 | 7 | http://www.bch.hn/ |
| Central Bank of Madagascar | Banque centrale | gratuit (données publiques) | 1 | 7 | https://www.banky-foibe.mg/ |
| Egyptian Tax Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 7 | https://www.vatlive.com |
| Federal Reserve Bank of Chicago | Banque centrale | gratuit (données publiques) | 1 | 7 | https://www.chicagofed.org/ |
| Federal Reserve Bank of Philadelphia | Banque centrale | gratuit (données publiques) | 1 | 7 | https://www.philadelphiafed.org/ |
| Finnish Tax Administration | Ministère / agence publique | gratuit (données publiques) | 1 | 7 | https://www.vero.fi |
| General Authority of Zakat and Tax (GAZT) | Ministère / agence publique | gratuit (données publiques) | 1 | 7 | https://gazt.gov.sa |
| Institut National de la Statistique et de l’Analyse Economique, Benin | Office statistique national | gratuit (données publiques) | 1 | 7 | http://www.insae-bj.org |
| Institut National de la Statistique, Guinea | Office statistique national | gratuit (données publiques) | 1 | 7 | https://www.stat-guinee.org/ |
| Ministerio de Empleo y Seguridad Social | Ministère / agence publique | gratuit (données publiques) | 1 | 7 | http://www.seg-social.es |
| Ministry of Finance - Kingdom of Bahrain | Ministère / agence publique | gratuit (données publiques) | 1 | 7 | https://www.data.gov.bh/ |
| Ministry of Finance of the Republic of Albania | Ministère / agence publique | gratuit (données publiques) | 1 | 7 | http://financa.gov.al |
| Ministry of Finance, Poland | Ministère / agence publique | gratuit (données publiques) | 1 | 7 | https://www.gov.pl/ |
| Ministry of Labor and Social Affairs | Ministère / agence publique | gratuit (données publiques) | 2 | 7 | http://www.mlsa.am |
| Monetary Authority of Macao | Banque centrale | gratuit (données publiques) | 1 | 7 | https://www.amcm.gov.mo/ |
| National Revenue Agency, Bulgaria | Ministère / agence publique | gratuit (données publiques) | 1 | 7 | http://www.ey.com |
| National Tax Agency | Ministère / agence publique | gratuit (données publiques) | 1 | 7 | https://www.nta.go.jp |
| Norges Bank | Banque centrale | gratuit (données publiques) | 1 | 7 | https://www.norges-bank.no |
| Národná banka Slovenska | Banque centrale | gratuit (données publiques) | 1 | 7 | https://nbs.sk/ |
| Skatteverket | Ministère / agence publique | gratuit (données publiques) | 1 | 7 | https://www.skatteverket.se/ |
| State Administration of Foreign Exchange, China | Ministère / agence publique | gratuit (données publiques) | 1 | 7 | http://www.safe.gov.cn/ |
| Swiss Federal Finance Administration | Ministère / agence publique | gratuit (données publiques) | 1 | 7 | https://www.efv.admin.ch/de |
| US Department of Agriculture | Ministère / agence publique | gratuit (données publiques) | 1 | 7 | https://www.usda.gov/ |
| Undersecretariat of Treasury, Turkey | Ministère / agence publique | gratuit (données publiques) | 1 | 7 | https://en.hmb.gov.tr/ |
| Administration des Contributions Directes | Ministère / agence publique | gratuit (données publiques) | 1 | 6 | https://www.vatlive.com |
| Automatic Data Processing, Inc | Institut privé / association | chiffre-titre gratuit, série payante | 2 | 6 | https://adpemploymentreport.com/ |
| Banque de la République d'Haiti | Banque centrale | gratuit (données publiques) | 1 | 6 | https://www.brh.ht |
| Bureau of Internal Revenue | Ministère / agence publique | gratuit (données publiques) | 1 | 6 |  |
| Bureau of Statistics and Census Libya | Office statistique national | gratuit (données publiques) | 1 | 6 | http://bsc.ly |
| Central Bank of Sudan | Banque centrale | gratuit (données publiques) | 1 | 6 | https://cbos.gov.sd/en |
| Central Statistical Organization Iraq (COSIT) | Office statistique national | gratuit (données publiques) | 1 | 6 | http://www.cosit.gov.iq/en |
| Centre for European Economic Research (ZEW) | Institut privé / association | chiffre-titre gratuit, série payante | 5 | 6 | http://www.zew.de |
| Direction Générale des Impôts, Algeria | Ministère / agence publique | gratuit (données publiques) | 1 | 6 | https://www.ey.com |
| Direcção Nacional dos Impostos, Ministério das Finanças | Office statistique national | gratuit (données publiques) | 1 | 6 | https://www.minfin.gov.ao/ |
| European Automobile Manufacturers Association | Institut privé / association | chiffre-titre gratuit, série payante | 6 | 6 | https://www.acea.auto/ |
| Federal Reserve Bank of Kansas City | Banque centrale | gratuit (données publiques) | 1 | 6 | https://kansascityfed.org |
| Federal Tax Service of Russia | Ministère / agence publique | gratuit (données publiques) | 1 | 6 | https://www.nalog.ru |
| Food and Agriculture Organization of the United Nations | Organisation internationale | gratuit (données publiques) | 1 | 6 | https://www.fao.org/worldfoodsituation/foodpricesindex/en |
| General Inspectorate of Finance | Ministère / agence publique | gratuit (données publiques) | 1 | 6 | https://www.vatlive.com |
| Government of Malta - Inland Revenue | Ministère / agence publique | gratuit (données publiques) | 1 | 6 | https://socialsecurity.gov.mt |
| Inland Revenue Department of the Republic of Cyprus | Ministère / agence publique | gratuit (données publiques) | 1 | 6 | http://ec.europa.eu |
| International Trade Centre | Organisation internationale | gratuit (données publiques) | 2 | 6 | https://intracen.org/ |
| Ministerio de Hacienda, Argentina | Ministère / agence publique | gratuit (données publiques) | 1 | 6 | https://argentina.gob.ar |
| Ministry Of Labour and Social Welfare | Ministère / agence publique | gratuit (données publiques) | 2 | 6 | https://www.rks-gov.net |
| Ministry of Labour and Social Security | Ministère / agence publique | gratuit (données publiques) | 2 | 6 |  |
| Ministry of Revenue and Duties of Ukraine | Ministère / agence publique | gratuit (données publiques) | 1 | 6 | https://www.avalara.com/eu/en/index.html |
| Monaco Statistics | Office statistique national | gratuit (données publiques) | 1 | 6 | http://www.monacostatistics.mc |
| Mortgage Bankers Association of America | Institut privé / association | payant | 1 | 6 | http://www.mortgagebankers.org/ |
| National Association of Realtors | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 6 | https://www.nar.realtor |
| New Zealand Treasury | Ministère / agence publique | gratuit (données publiques) | 1 | 6 | https://treasury.govt.nz/ |
| Nielsen | Institut privé / association | chiffre-titre gratuit, série payante | 6 | 6 | https://www.nielsen.com |
| Qatar Statistics Authority | Office statistique national | gratuit (données publiques) | 1 | 6 | https://www.psa.gov.qa/en/Pages/default.aspx |
| Secretaria da Receita Federal do Brasil | Ministère / agence publique | gratuit (données publiques) | 1 | 6 | http://receita.economia.gov.br/ |
| Social Insurance Institute | Ministère / agence publique | gratuit (données publiques) | 2 | 6 | http://www.kpmg.com |
| South African Revenue Service | Ministère / agence publique | gratuit (données publiques) | 1 | 6 | http://www.sars.gov.za |
| Tax Directorate, Slovakia | Ministère / agence publique | gratuit (données publiques) | 1 | 6 | https://www.vatlive.com |
| AFDB | Organisation internationale | gratuit (données publiques) | 2 | 5 | http://www.afdb.org |
| Agency of Statistics of Tajikistan | Office statistique national | gratuit (données publiques) | 1 | 5 |  |
| Australian Industry Group | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 5 | https://www.aigroup.com.au |
| Autoridad de Impugnación Tributaria (AIT), Bolivia | Ministère / agence publique | gratuit (données publiques) | 1 | 5 | https://www.ait.gob.bo/ |
| Banque Nationale de Belgique | Banque centrale | gratuit (données publiques) | 1 | 5 | http://stat.nbb.be |
| Central Bank of Mauritania | Banque centrale | gratuit (données publiques) | 1 | 5 | http://www.bcm.mr |
| Central Bank of Solomon Islands (CBSI) | Banque centrale | gratuit (données publiques) | 1 | 5 | https://www.cbsi.com.sb/ |
| Central Statistical Office of Zambia | Office statistique national | gratuit (données publiques) | 1 | 5 | http://www.zamstats.gov.zm/ |
| Central Statistical Office, Trinidad and Tobago | Office statistique national | gratuit (données publiques) | 1 | 5 | https://cso.gov.tt/ |
| Czech Social Security Administration | Ministère / agence publique | gratuit (données publiques) | 1 | 5 | https://www.duchodovareforma.cz |
| Department Of Finance Canada | Ministère / agence publique | gratuit (données publiques) | 1 | 5 | https://www.fin.gc.ca |
| Directorate of Internal Revenue | Ministère / agence publique | gratuit (données publiques) | 1 | 5 | https://www.rsk.is |
| Ethiopian Statistical Service (ESS) | Office statistique national | gratuit (données publiques) | 1 | 5 | https://ess.gov.et/ |
| FSM Social Security Administration | Ministère / agence publique | gratuit (données publiques) | 1 | 5 | http://www.fsmssa.fm/ |
| Federal Department of Home Affairs | Ministère / agence publique | gratuit (données publiques) | 1 | 5 | https://www.edi.admin.ch |
| General Department of Taxation | Ministère / agence publique | gratuit (données publiques) | 2 | 5 | https://www.tax.gov.kh |
| Hellenic Ministry of Labour, Social Insurance and Social Solidarity | Ministère / agence publique | gratuit (données publiques) | 1 | 5 |  |
| Inland Revenue Board of Malaysia | Ministère / agence publique | gratuit (données publiques) | 1 | 5 | http://www.hasil.gov.my |
| Instituto Nacional de Estadistica y Censos (INEC) | Office statistique national | gratuit (données publiques) | 1 | 5 | https://www.ecuadorencifras.gob.ec |
| Instituto da Segurança Social | Ministère / agence publique | gratuit (données publiques) | 1 | 5 | http://www.seg-social.pt |
| Korean Worker's Compensation & Welfare Service | Ministère / agence publique | gratuit (données publiques) | 1 | 5 | https://www.kcomwel.or.kr |
| L'Institut Haïtien de Statistique et d'Informatique (IHSI) | Office statistique national | gratuit (données publiques) | 1 | 5 | https://ihsi.gouv.ht/ |
| L'Institut de Statistiques et d’Etudes Economiques du Burundi (ISTEEBU) | Office statistique national | gratuit (données publiques) | 1 | 5 | http://isteebu.bi |
| Logistics Managers' Index | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 5 | http://www.the-lmi.com/ |
| Mexican Social Security Institute | Ministère / agence publique | gratuit (données publiques) | 1 | 5 | http://www.imss.gob.mx/ |
| Ministerio de Hacienda y Crédito Público | Ministère / agence publique | gratuit (données publiques) | 1 | 5 | http://www.hacienda.gob.ni/ |
| Ministero del Lavoro e delle Politiche Sociali | Ministère / agence publique | gratuit (données publiques) | 1 | 5 | https://www.lavoro.gov.it |
| Ministry Of Public Finance, Romania | Ministère / agence publique | gratuit (données publiques) | 1 | 5 |  |
| Ministry of Commerce, Thailand | Ministère / agence publique | gratuit (données publiques) | 1 | 5 | https://www.moc.go.th |
| Ministry of Economic Affairs, R.O.C | Ministère / agence publique | gratuit (données publiques) | 1 | 5 | http://www.moea.gov.tw |
| Ministry of Finance of the Republic of Bulgaria | Ministère / agence publique | gratuit (données publiques) | 1 | 5 | http://www.minfin.bg |
| Ministry of Finance, Republic of Kosovo | Ministère / agence publique | gratuit (données publiques) | 1 | 5 | https://www.rks-gov.net |
| Ministry of Planning, Egypt | Ministère / agence publique | gratuit (données publiques) | 1 | 5 | http://mpmar.gov.eg/ |
| Ministry of Social Affairs and Social Services | Ministère / agence publique | gratuit (données publiques) | 1 | 5 | http://www.btl.gov.il |
| Ministry of Trade, Industry & Energy (MOTIE) | Ministère / agence publique | gratuit (données publiques) | 1 | 5 | http://english.motie.go.kr |
| Ministère des Solidarités et de la Santé | Ministère / agence publique | gratuit (données publiques) | 1 | 5 | https://solidarites-sante.gouv.fr/ |
| National Bank of Kazakhstan | Banque centrale | gratuit (données publiques) | 1 | 5 | http://www.nationalbank.kz |
| National Bureau of Statistics, UAE | Office statistique national | gratuit (données publiques) | 1 | 5 | https://www.moca.gov.ae/ |
| National Institute of Statistics of Cambodia | Office statistique national | gratuit (données publiques) | 1 | 5 | http://www.nis.gov.kh/index.php/en/ |
| National Institute of Statistics of Djibouti (INSTAD) | Office statistique national | gratuit (données publiques) | 1 | 5 | https://instad.dj/ |
| National Institute of Statistics, Venezuela | Office statistique national | gratuit (données publiques) | 1 | 5 | http://www.ine.gov.ve/ |
| Republic of Azerbaijan Ministry of Labour and Social Protection of Population | Ministère / agence publique | gratuit (données publiques) | 1 | 5 | https://www.sosial.gov.az/ |
| SEMCA - Consejo Monetario Centroamericano | Organisation internationale | gratuit (données publiques) | 4 | 5 | http://www.secmca.org |
| Social Security Administration | Ministère / agence publique | gratuit (données publiques) | 1 | 5 | https://www.ssa.gov/ |
| Statistics Guyana | Office statistique national | gratuit (données publiques) | 1 | 5 | http://www.bankofguyana.org.gy |
| Tax and Customs Administration | Ministère / agence publique | gratuit (données publiques) | 1 | 5 | https://www.iamexpat.nl |
| The Society of Motor Manufacturers and Traders Limited (SMMT) | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 5 | https://www.smmt.co.uk |
| The State Bank of Vietnam | Banque centrale | gratuit (données publiques) | 1 | 5 | https://www.sbv.gov.vn |
| University of Michigan | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 5 | http://www.sca.isr.umich.edu/ |
| Agencia Tributaria, Spain | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://www.agenciatributaria.es/ |
| Agency for Statistics of Bosnia and Herzegovina | Office statistique national | gratuit (données publiques) | 1 | 4 | http://bhas.gov.ba |
| Agenzia delle Entrate, Ministero dell'Economia e delle finanze | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://www.agenziaentrate.gov.it |
| Autoriti Monetari Brunei Darussalam | Banque centrale | gratuit (données publiques) | 1 | 4 | https://www.bdcb.gov.bn/home.aspx |
| Bangladesh Bureau of Statistics (BBS) | Office statistique national | gratuit (données publiques) | 1 | 4 | https://bbs.gov.bd/ |
| Botswana Unified Revenue Service | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://www.burs.org.bw/ |
| Brunei Darussalam Central Bank | Banque centrale | gratuit (données publiques) | 1 | 4 | https://www.bdcb.gov.bn/ |
| Bundesagentur für Arbeit | Office statistique national | gratuit (données publiques) | 1 | 4 | https://statistik.arbeitsagentur.de |
| Bureau for Economic Research (BER) | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 4 | https://www.ber.ac.za |
| Bureau of Trade and Economic Indices, Thailand | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | http://www.price.moc.go.th |
| Bureau of the Treasury, Philippines | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://www.treasury.gov.ph |
| CNSEE - Centre Nationale de la Statistique et des Etudes Economiques, Congo | Office statistique national | gratuit (données publiques) | 1 | 4 | https://ins-congo.cg/ |
| Central Administration for Statistics, Lebanon | Office statistique national | gratuit (données publiques) | 1 | 4 |  |
| Central Bank of West African States | Organisation internationale | gratuit (données publiques) | 4 | 4 | https://www.bceao.int |
| Central Informatics Organisation - Kingdom of Bahrain | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | http://www.data.gov.bh |
| Confederation of British Industry | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 4 | https://www.cbi.org.uk |
| DARES, France | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://dares.travail-emploi.gouv.fr |
| DGSCN, Togo | Office statistique national | gratuit (données publiques) | 1 | 4 | http://www.stat-togo.org/ |
| DRC Tax Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | http://portal.drc.gov.bt/drc/ |
| Direction Générale de la Statistique, Gabon | Office statistique national | gratuit (données publiques) | 1 | 4 | https://instatgabon.org/ |
| Direction Générale des Finances Publiques | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://www.impots.gouv.fr |
| Direction Générale des Impôts | Ministère / agence publique | gratuit (données publiques) | 2 | 4 | https://www.impots.bj/ |
| Directorate General of Taxation of Costa Rica | Ministère / agence publique | gratuit (données publiques) | 1 | 4 |  |
| Eiendom Norge/Real Estate Norway | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 4 | https://eiendomnorge.no |
| Federal Inland Revenue Service, Nigeria | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://www.firs.gov.ng/ |
| Federal Treasury Of Russia | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://minfin.gov.ru |
| Financial Administration of the Czech Republic | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://taxsummaries.pwc.com/ |
| GSIS, Greece | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://www.gsis.gr/ |
| Halifax and Bank of Scotland | Institut privé / association | payant | 1 | 4 | https://www.halifax.co.uk/media-centre/house-price-index.html |
| Indirect Taxation Authority of Bosnia and Herzegovina | Ministère / agence publique | gratuit (données publiques) | 1 | 4 |  |
| Inland Revenue Department | Ministère / agence publique | gratuit (données publiques) | 4 | 4 | https://ird.gov.ag/ |
| Institut National de la Statistique du Tchad (INSEED) | Office statistique national | gratuit (données publiques) | 1 | 4 | https://www.inseed.td/index.php |
| Internal Revenue Commission, Papua New Guinea | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | http://irc.gov.pg |
| Internal Revenue Service | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://www.taxpolicycenter.org/ |
| Kenya Revenue Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://www.kra.go.ke/ |
| Mauritius Revenue Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://www.mra.mu/ |
| Ministere de l'Économie, des Finances, de l'Action et des Comptes publics, France | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://www.economie.gouv.fr/ |
| Ministerio de Economía y Finanzas Públicas | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://www.economiayfinanzas.gob.bo/ |
| Ministry Of Finance, Republic of Lebanon | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://www.bdl.gov.lb/index.php |
| Ministry of Economic Affairs (MOEA), Taiwan | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | http://www.moea.gov.tw |
| Ministry of Economy and Finance | Ministère / agence publique | gratuit (données publiques) | 4 | 4 | https://finances.ml/ |
| Ministry of Economy and Finance of the Republic of Uzbekistan | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://www.imv.uz/ |
| Ministry of Finance of Republic of Indonesia | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://www.kemenkeu.go.id |
| Ministry of Finance of Ukraine | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://mof.gov.ua/uk |
| Ministry of Finance of the People's Republic of China | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | http://www.mof.gov.cn |
| Ministry of Finance, Bahamas | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://www.bahamas.gov.bs |
| Ministry of Finance, Barbados | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://www.gov.bb/ |
| Ministry of Finance, Bhutan | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | http://www.mof.gov.bt |
| Ministry of Finance, Egypt | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | http://www.mof.gov.eg |
| Ministry of Finance, Jordan | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://mof.gov.jo |
| Ministry of Finance, Qatar | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://www.mof.gov.qa/en/pages/default.aspx |
| Ministry of Finance, Republic of Slovenia | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://www.gov.si/en/ |
| Ministry of Human Resources and Social Security, China | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | http://www.mohrss.gov.cn/ |
| Ministry of Labor and Social Welfare | Ministère / agence publique | gratuit (données publiques) | 2 | 4 | https://www.dt.gob.cl |
| Ministry of Labor, Employment, and Social Security | Ministère / agence publique | gratuit (données publiques) | 2 | 4 |  |
| Ministry of Land, Infrastructure, Transport and Tourism, Japan | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | http://www.mlit.go.jp |
| Ministry of Manpower Singapore | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://www.mom.gov.sg/ |
| Ministry of Social Policy of Ukraine | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://services.dtkt.ua |
| Namibia Revenue Agency | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://www.namra.org.na/ |
| National Bureau of Statistics, Tanzania | Office statistique national | gratuit (données publiques) | 1 | 4 | http://www.nbs.go.tz |
| National Center of Legislation under the President of the Republic of Tajikistan | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | http://mmk.tj |
| National Institute of Statistics of Rwanda (NISR) | Office statistique national | gratuit (données publiques) | 1 | 4 | http://statistics.gov.rw |
| National Revenue Agency | Ministère / agence publique | gratuit (données publiques) | 2 | 4 | http://www.noi.bg |
| National Treasury, South Africa | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | http://www.treasury.gov.za |
| Palestine Monetary Authority | Banque centrale | gratuit (données publiques) | 1 | 4 | https://www.pma.ps/en/ |
| Petrol | Institut privé / association | chiffre-titre gratuit, série payante | 4 | 4 | http://www.petrol.eu/ |
| Public Revenue Office | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://www.vatlive.com |
| Quotable Value | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 4 | https://www.qv.co.nz/ |
| Service Public Fédéral Finances | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://finances.belgium.be/fr |
| Standard & Poor's | Institut privé / association | payant | 1 | 4 | https://us.spindices.com |
| State Treasury, Finland | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://www.treasuryfinland.fi/ |
| State tax Service | Ministère / agence publique | gratuit (données publiques) | 2 | 4 |  |
| Statistics Suriname | Office statistique national | gratuit (données publiques) | 1 | 4 | https://statistics-suriname.org |
| Sudan Central Bureau of Statistics | Office statistique national | gratuit (données publiques) | 1 | 4 | http://cbs.gov.sd |
| Superintendencia Nacional de Aduanas y de Administración Tributaria | Ministère / agence publique | gratuit (données publiques) | 1 | 4 |  |
| Swiss Federal Tax Administration | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://www.estv.admin.ch |
| Tax and Customs Administration - Belastingdienst | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://www.vatlive.com |
| The Federation of Thai Industries | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 4 | https://fti.or.th/ |
| Turkish Revenue Administration | Ministère / agence publique | gratuit (données publiques) | 1 | 4 | https://www.gib.gov.tr/ |
| Afghanistan Revenue Department | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Autoridade Tributária de Moçambique | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Autoridade Tributária e Aduaneira | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | http://info.portaldasfinancas.gov.pt |
| Banco Central de Costa Rica | Banque centrale | gratuit (données publiques) | 1 | 3 | https://www.bccr.fi.cr |
| Banco de Mexico | Banque centrale | gratuit (données publiques) | 1 | 3 | https://www.banxico.org.mx/ |
| Banco de la Republica de Colombia | Banque centrale | gratuit (données publiques) | 1 | 3 | https://www.banrep.gov.co |
| Banco de la Republica, Colombia | Banque centrale | gratuit (données publiques) | 1 | 3 | https://www.banrep.gov.co |
| Bureau of Labor Statistics | Office statistique national | gratuit (données publiques) | 1 | 3 | https://www.bls.gov/home.htm |
| Business New Zealand | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 3 | https://www.businessnz.org.nz |
| Central Agency for Public Mobilization and Statistics | Office statistique national | gratuit (données publiques) | 1 | 3 | https://www.capmas.gov.eg |
| Central American Monetary Council | Organisation internationale | gratuit (données publiques) | 1 | 3 | http://www.secmca.org |
| Central Bank of Myanmar | Banque centrale | gratuit (données publiques) | 1 | 3 | http://www.cbm.gov.mm |
| Central Bank of Pakistan | Banque centrale | gratuit (données publiques) | 1 | 3 | http://www.sbp.org.pk |
| Central Bank of Swaiziland | Banque centrale | gratuit (données publiques) | 1 | 3 | https://www.centralbank.org.sz |
| Central Bank of Syria | Banque centrale | gratuit (données publiques) | 1 | 3 |  |
| Central Bank of United Arab Emirates | Banque centrale | gratuit (données publiques) | 1 | 3 | https://www.centralbank.ae |
| Central Bank of Yemen | Banque centrale | gratuit (données publiques) | 1 | 3 | http://www.centralbank.gov.ye |
| Central Statistics Office, Yemen | Office statistique national | gratuit (données publiques) | 1 | 3 | http://www.cso-yemen.org |
| Central Statistics Organization of Afghanistan | Office statistique national | gratuit (données publiques) | 1 | 3 | http://cso.gov.af |
| Chilean Chamber of Construction | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 3 | https://www.cchc.cl/ |
| China Association of Automobile Manufacturers | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 3 | http://www.caam.org.cn |
| Commisioner of Inland Revenue | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Confederacao Nacional da Industria (CNI) | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 3 | http://www.portaldaindustria.com.br |
| Controller General Of Accounts, India | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://cga.nic.in/ |
| DGO - Direccao Geral do Orcamento, Portugal | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.dgo.gov.pt/Paginas/default.aspx |
| Department of Income Tax | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Department of Public Revenues, Montenegro | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.vatlive.com |
| Department of Statistics of The Bahamas | Office statistique national | gratuit (données publiques) | 1 | 3 | http://www.bahamas.gov.bs/ |
| Direccion General de Impuestos Internos | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Dirección Ejecutiva de Ingresos | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Dirección General Impositiva | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Dirección General de Administración y Finanzas | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Directorate of National Statistics, Somalia | Office statistique national | gratuit (données publiques) | 1 | 3 | https://nbs.gov.so/ |
| Directorate of National Taxes and Customs (DIAN) | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Dubai Data and Statistics Establishment - Government of Dubai | Office statistique national | gratuit (données publiques) | 1 | 3 | https://www.dsc.gov.ae/en-us/Pages/default.aspx |
| Employment Service of Slovenia | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.ess.gov.si |
| Ethiopian Revenues and Customs Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| FGV - Fundacao Getulio Vargas | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 3 | https://portal.fgv.br/ |
| FUNCEX, Brazil | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 3 | https://www.funcex.org.br |
| Federal Administration of Public Revenue | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Federal Administration of Public Revenue (AFIP) | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Federal Board of Revenue, Government of Pakistan | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Federal Chamber of Automotive Industries (FCAI) | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 3 | https://www.fcai.com.au/ |
| Federal Customs Administration | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.ezv.admin.ch |
| Federal Housing Finance Agency | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.fhfa.gov |
| Federal Office of Statistics, Federation of Bosnia and Herzegovina | Office statistique national | gratuit (données publiques) | 1 | 3 | https://bhas.gov.ba/?lang=en |
| Federal Reserve Bank of Richmond | Banque centrale | gratuit (données publiques) | 1 | 3 | https://www.richmondfed.org |
| Fiji National Provident Fund | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Fiji Revenue & Customs Service | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.frcs.org.fj |
| Gambia Revenue Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.gra.gm/ |
| General Department Of Taxation - Ministry Of Finance | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.tax.gov.kh |
| General Department of National Taxation | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| General Directorate of Revenues | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| General Directorate of Revenues, Panama | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| General Pension and Social Security Authority (GPSSA) | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.kpmg.com |
| Ghana Revenue Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://gra.gov.gh/ |
| Ghana Statistical Services | Office statistique national | gratuit (données publiques) | 1 | 3 | https://statsghana.gov.gh/ |
| Hellenic Republic Ministry of Economy and Finance | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.minfin.gr/ |
| ILO | Organisation internationale | gratuit (données publiques) | 3 | 3 | https://www.ilo.org/global/lang--en/index.htm |
| INSAE, Benin | Office statistique national | gratuit (données publiques) | 1 | 3 | http://www.insae-bj.org |
| Icelandic Ministry of Finance | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | http://eng.fjarmalaraduneyti.is |
| Ifo Institute | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 3 | https://www.ifo.de |
| Inland Revenue Division | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Institut National de la Statistique | Office statistique national | gratuit (données publiques) | 1 | 3 | http://www.ins.ci/ |
| Institut National de la Statistique de Côte d'Ivoire | Office statistique national | gratuit (données publiques) | 1 | 3 | http://www.ins.ci |
| Institut de Statistiques et d’Etudes Economiques du Burundi | Office statistique national | gratuit (données publiques) | 1 | 3 | http://isteebu.bi |
| Institute for Social Insurance | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Instituto Ecuatoriano de Seguridad Social | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | http://www.igpinfo.com |
| Instituto Guatemalteco de Seguridad Social | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Instituto Nacional de Estadistica, Geografia e Informatica, Mexico | Office statistique national | gratuit (données publiques) | 1 | 3 | https://www.inegi.org.mx/ |
| Instituto Nacional de Estatistica | Office statistique national | gratuit (données publiques) | 1 | 3 | https://www.ine.st/index.php |
| Instituto Venezolano de los Seguros Sociales | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Instituto de Seguros Sociales | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Internal Revenue Commission | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Internal Revenue Department | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| International Monetary Fund (IMF) | Organisation internationale | gratuit (données publiques) | 3 | 3 | http://www.imf.org |
| Intitut National de la Statistique | Office statistique national | gratuit (données publiques) | 1 | 3 | http://www.ins.ci/ |
| Ipsos | Institut privé / association | chiffre-titre gratuit, série payante | 3 | 3 | https://www.ipsos.com |
| Israel Ministry Of Finance | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.gov.il/en/departments/ministry_of_finance/govil-landing-page |
| Israel Ministry of Finance Tax Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| La Direction Générale des Impôts, Ministère des Finances | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Labor Office of the Czech Republic | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.uradprace.cz/uvod |
| Latvijas Banka | Banque centrale | gratuit (données publiques) | 1 | 3 | https://statdb.bank.lv |
| Lesotho Revenue Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | http://lra.org.ls/ |
| Melbourne Institute | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 3 | http://melbourneinstitute.unimelb.edu.au |
| Ministerio de Economía y Finanzas, Ecuador | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.finanzas.gob.ec/ |
| Ministerio de Economía y Finanzas, Uruguay | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | http://deuda.mef.gub.uy |
| Ministerio de Economía, Comercio y Empresa | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | http://www.comercio.gob.es |
| Ministerio de Trabajo y Seguridad Social | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Ministerio de Trabajo y Seguridad Social de Costa Rica | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Ministerio del Poder Popular de Finanzas, SENIAT | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Ministerio di Finansas, Comunicacion, Utilidad y Energia | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Ministry Of Finance Of Slovak Republic | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.mfsr.sk/en/ |
| Ministry Of Finance, Montenegro | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Ministry for Taxes and Levies of the Republic of Belarus | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Ministry of Commerce | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Ministry of Commerce and Industry, India | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://commerce.gov.in/ |
| Ministry of Customs and Revenue, Samoa | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Ministry of Employment and Labor | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Ministry of Employment, Social Affairs and Professional Formation | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Ministry of Finance and Economic Development | Ministère / agence publique | gratuit (données publiques) | 2 | 3 | http://www.nbe.gov.et |
| Ministry of Finance and National Economy | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.mofne.gov.bh |
| Ministry of Finance of Angola | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.minfin.gov.ao |
| Ministry of Finance of Georgia | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.mof.ge/ |
| Ministry of Finance of the Republic of Belarus | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | http://minfin.gov.by/ |
| Ministry of Finance of the Republic of Kazakhstan | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Ministry of Finance of the Republic of Macedonia | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Ministry of Finance, Hungary | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://ngmszakmaiteruletek.kormany.hu/index |
| Ministry of Finance, Income Tax Department | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.pit.pl |
| Ministry of Finance, Oman | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://gov.om/en/ministry-of-finance |
| Ministry of Finance, Republic of Montenegro | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.gov.me/mif |
| Ministry of Finance, Sierra Leone | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://mof.gov.sl/ |
| Ministry of Finance, Swaziland | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.gov.sz |
| Ministry of Finance, UAE | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://mof.gov.ae/en/home |
| Ministry of Finance, Ukraine | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://index.minfin.com.ua |
| Ministry of Labor | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Ministry of Labor and Social Insurance | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Ministry of Labor and Social Protection | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Ministry of Labor, Family, and Social Protection | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://home.kpmg.com |
| Ministry of Labour and Employment Relations | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Ministry of Labour and Immigration | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Ministry of Labour and Social Protection | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://mlsp.gov.mn/eng/ |
| Ministry of National Economy, Oman | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.ncsi.gov.om |
| Ministry of Social Security, National Solidarity, and Reform Institutions | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://socialsecurity.govmu.org/socialsecurity/ |
| Ministère de l'Économie et des Finances | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | http://lekiosque.finances.gouv.fr |
| Ministère de la Transition écologique et solidaire | Office statistique national | gratuit (données publiques) | 1 | 3 | https://www.statistiques.developpement-durable.gouv.fr/ |
| Ministério da Previdência Social | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | http://www.previdencia.gov.br/ |
| Ministério do Desenvolvimento, Indústria e Comércio Exterior | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.gov.br/mdic/pt-br |
| Monetary Authority Macao | Banque centrale | gratuit (données publiques) | 1 | 3 | https://www.amcm.gov.mo/ |
| National Agency for Fiscal Administration (ANAF) | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.vatlive.com |
| National Australia Bank | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 3 | https://business.nab.com.au |
| National Board of Revenue (NBR), Bangladesh | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| National Debt Management Center, Ministry of Finance | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.ndmc.gov.sa/en/Pages/default.aspx |
| National Institute of Economic Research, Sweden | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.konj.se/ |
| National Institute of Social Security | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| National Institute of State Statistics and Information, Turkmenistan | Office statistique national | gratuit (données publiques) | 1 | 3 |  |
| National Insurance Corporation | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| National Office of Social Insurance | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| National Pension Scheme Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.napsa.co.zm/ |
| National Social Insurance Fund | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| National Social Security Fund, Kenya | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.nssf.or.ke/ |
| National Social Security Fund, Uganda | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.nssfug.org/ |
| National Social Security Office | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| National Statistical Office of Thailand | Office statistique national | gratuit (données publiques) | 1 | 3 | https://www.nso.go.th/nsoweb/index?set_lang=en |
| National Statistics, Republic of China | Office statistique national | gratuit (données publiques) | 1 | 3 | http://eng.stat.gov.tw |
| National Statistics, Republic of China (Taiwan) | Office statistique national | gratuit (données publiques) | 1 | 3 | http://eng.stat.gov.tw |
| National Tax Administration, Ministry of Finance | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| National Tax Service, South Korea | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.nts.go.kr |
| Nationwide Building Society, United Kingdom | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 3 | https://www.nationwide.co.uk/ |
| Office des Changes | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | http://www.oc.gov.ma |
| Office des Changes, Marocco | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | http://www.oc.gov.ma |
| Office for Budget Responsibility, UK | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://obr.uk/ |
| Office of the Tax Commissioner, Government of Bermuda | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.gov.bm/ |
| Public Revenues and Taxes Department, Ministry of Finance | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Rwanda Revenue Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.rra.gov.rw/en |
| Rwanda Social Security Board | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.rssb.rw/ |
| SIAM - Society of Indian Automobile Manufacturers | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 3 | https://www.siam.in/ |
| Secretaría de Hacienda Y Crédito Público | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.gob.mx/shcp |
| Servicio de Administración Tributaria, SHCP | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Servicio de Rentas Internas del Ecuador | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Seychelles Revenue Commission | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://src.gov.sc/ |
| Social Insurance General Office | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Social Security Corporation | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Social Security Fund | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Social Security Organisation (SOCSO) | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.perkeso.gov.my |
| South African Social Security Agency (SASSA) | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Sri Lanka Inland Revenue Department | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | http://www.ird.gov.lk |
| State Revenue Service - Ministry of Finance | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| State Revenue Service, Latvia | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.vatlive.com |
| State Tax Committee | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| State Tax Inspectorate | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | http://ec.europa.eu |
| State Tax Inspectorate (STI) | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Statistics Netherlands - CBS | Office statistique national | gratuit (données publiques) | 1 | 3 | https://www.cbs.nl/en-gb |
| Statistik Austria | Office statistique national | gratuit (données publiques) | 1 | 3 | http://www.statistik.at |
| Sudan Chamber of Taxation | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Superintendence of the Tax Administration | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Swedish National Financial Management Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.esv.se/ |
| Tanzania Revenue Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.tra.go.tz/ |
| Tax Administration - Ministry of Finance | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Tax Administration of the Federation of B&H | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | http://www.fipa.gov.ba |
| Tax Admministration of Jamaica | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Tax Committee of the Ministry of Finance | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Tax Committee of the Ministry of Finance of the Republic of Kazakhstan | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Tax Service of Republic of Armenia | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Taxation Administration, Ministry of Finance, R.O.C | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Thailand Revenue Department | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| The Hashemite Kingdom of Jordan Department of Statistics (DOS) | Office statistique national | gratuit (données publiques) | 1 | 3 | http://dosweb.dos.gov.jo |
| The Revenue Department, Government of Thailand | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Trade Policy and Strategy Office | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://tpso.go.th/home |
| Treasury Department, Ministry of Finance and Economy | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.mofe.gov.bn/ |
| Treasury of the Republic of Latvia | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.kase.gov.lv/en |
| Uganda Revenue Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.ura.go.ug/ |
| Urban Redevelopment Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.ura.gov.sg |
| Vanuatu National Statistics Office | Office statistique national | gratuit (données publiques) | 1 | 3 | https://vnso.gov.vu/ |
| Yemen Tax Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 3 |  |
| Zambia Revenue Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.zra.org.zm/ |
| Zimbabwe Revenue Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 3 | https://www.zimra.co.zw/ |
| ANFAC - Asociacion Espanola de Fabricantes de Automoviles y Camiones | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 2 | https://anfac.com/ |
| ANFAVEA, Brazil | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 2 | http://www.anfavea.com.br |
| ANFIA - Italian Association of the Automotive Industry | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 2 | https://www.anfia.it/ |
| ANZ Bank New Zealand | Banque centrale | gratuit (données publiques) | 1 | 2 | https://www.anz.co.nz/personal/ |
| African Development Bank | Organisation internationale | gratuit (données publiques) | 1 | 2 |  |
| Arbeitsmarktservice Oesterreich | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.ams.at |
| Association of European Businesses | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 2 | https://aebrus.ru |
| Australia and New Zealand Banking Group | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 2 | https://www.anz.com.au/personal/ |
| Australian Government | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | http://www.budget.gov.au |
| BRC - British Retail Consortium | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 2 | https://www.brc.org.uk/ |
| Banco de la República, Colombia | Banque centrale | gratuit (données publiques) | 1 | 2 | https://www.banrep.gov.co |
| Bank of Mexico | Banque centrale | gratuit (données publiques) | 1 | 2 | https://www.banxico.org.mx/ |
| BelgoStat | Office statistique national | gratuit (données publiques) | 1 | 2 | https://statbel.fgov.be |
| Belize Tax Services | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://bts.gov.bz/ |
| CAPMAS - Central Agency for Public Mobilization And Statistics, Egypt | Office statistique national | gratuit (données publiques) | 1 | 2 | https://www.capmas.gov.eg |
| CCFA - Comite des Constructeurs Francais d'Automobiles | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 2 | https://ccfa.fr/ |
| CNSEE, Congo | Office statistique national | gratuit (données publiques) | 1 | 2 | https://www.cnsee.org/ |
| Caisse Nationale d’Assurance Pension | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | http://www.cnap.lu |
| Canadian Real Estate Association (CREA) | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.crea.ca/ |
| Central Bank of Angola | Banque centrale | gratuit (données publiques) | 1 | 2 | https://www.bna.ao/ |
| Central Bank of Morocco | Banque centrale | gratuit (données publiques) | 1 | 2 | http://www.bkam.ma/ |
| Central Bank of Nigeria (CBN) | Banque centrale | gratuit (données publiques) | 1 | 2 | https://www.cbn.gov.ng/ |
| Central Bank of Timor-Leste | Banque centrale | gratuit (données publiques) | 1 | 2 | http://www.bancocentral.tl |
| Central Bank of the Republic Azerbaijan | Banque centrale | gratuit (données publiques) | 1 | 2 | https://www.cbar.az/ |
| Central Statistical Agency of Ethiopia | Office statistique national | gratuit (données publiques) | 1 | 2 | https://ess.gov.et/ |
| Central Statistical Office | Office statistique national | gratuit (données publiques) | 1 | 2 | https://cso.gov.tt/ |
| Central Statistical Organization Iraq | Office statistique national | gratuit (données publiques) | 1 | 2 | https://cosit.gov.iq/ar/ |
| Central Statistics Office | Office statistique national | gratuit (données publiques) | 1 | 2 | https://www.statsbots.org.bw/ |
| Centrale Bank van Aruba | Banque centrale | gratuit (données publiques) | 1 | 2 | http://www.cbaruba.org |
| Challenger, Gray and Christmas, Inc | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 2 | https://www.challengergray.com |
| Civil Service Bureau | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.csb.gov.hk |
| Commission Bancaire, Financière et des Assurances | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.expatica.com |
| Customs Finland | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://tulli.fi/en/frontpage |
| Customs and Inland Revenue Department, Vanuatu | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://customsinlandrevenue.gov.vu |
| Czech Statistical Office (CSU) | Office statistique national | gratuit (données publiques) | 1 | 2 | https://www.czso.cz/csu/czso/home |
| DIPRES - Direccion de Presupuestos, Chile | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.dipres.gob.cl |
| Daily NK | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 2 |  |
| Department of Finance and Administration, Australia | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | http://www.financeminister.gov.au |
| Department of Tax Services, Monaco | Ministère / agence publique | gratuit (données publiques) | 1 | 2 |  |
| Department of Taxes | Ministère / agence publique | gratuit (données publiques) | 1 | 2 |  |
| Dirección General de Crédito Público, Ministerio de Hacienda | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.creditopublico.gob.do/ |
| Employment Agency of Montenegro | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.zzzcg.me/me/ |
| Europace AG, Germany | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 2 | https://neu.europace.de |
| European Banking Federation | Institut privé / association | chiffre-titre gratuit, série payante | 2 | 2 | https://www.suomenpankki.fi/ |
| Federal Reserve Bank of Cleveland | Banque centrale | gratuit (données publiques) | 1 | 2 | https://www.clevelandfed.org/ |
| Fedesarrollo | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 2 | https://www.fedesarrollo.org.co/ |
| Financial Management Service, United States | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.fiscal.treasury.gov |
| Freddie Mac | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 2 | https://www.freddiemac.com/ |
| GKI Economic Research Co | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 2 |  |
| GfK Group | Institut privé / association | chiffre-titre gratuit, série payante | 2 | 2 | https://www.gfk.com/de/ |
| Government of the People's Republic of Bangladesh | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.bangladesh.gov.bd |
| Guam Department of Revenue and Taxation | Ministère / agence publique | gratuit (données publiques) | 1 | 2 |  |
| IEFP - Institute of Employment and Professional Formation, Portugal | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.iefp.pt |
| INEC, Panama | Office statistique national | gratuit (données publiques) | 1 | 2 | http://www.inec.gob.pa/ |
| ISSH, Albania | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 2 | http://www.issh.gov.al |
| Income Tax Services, Isle of Man Government | Ministère / agence publique | gratuit (données publiques) | 1 | 2 |  |
| Inland Revenue Department, Grenada | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.gov.gd/index.php |
| Inland Revenue Department, Hong Kong | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.ird.gov.hk |
| Inland Revenue Division, Dominica | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://ird.gov.dm/ |
| Inst Natl De La Statistique, Ivory Coast | Office statistique national | gratuit (données publiques) | 1 | 2 | http://www.ins.ci |
| Institut National de la Statistique, des Etudes Economiques et Démographiques (INSEED), Tchad | Office statistique national | gratuit (données publiques) | 1 | 2 | https://www.inseed.td/index.php# |
| Institute of Overseas Issuance | Organisation internationale | gratuit (données publiques) | 1 | 2 | https://www.ieom.fr |
| Instituto Nacional de Estadística Paraguay | Office statistique national | gratuit (données publiques) | 1 | 2 | https://www.ine.gov.py/ |
| Investment Coordinating Board of the Republic of Indonesia (BKPM) | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.bkpm.go.id/ |
| Jordan Tax Service | Ministère / agence publique | gratuit (données publiques) | 1 | 2 |  |
| Junta de Planificación de Puerto Rico | Ministère / agence publique | gratuit (données publiques) | 1 | 2 |  |
| KBA - Federal Motor Transport Authority, Germany | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.kba.de/DE/Home/home_node.html |
| Korea Automobile Manufacturers Association (KAMA) | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 2 | https://www.kama.or.kr/MainController |
| Korea Productivity Center | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 2 | https://www.kpc.or.kr |
| Korea Tourism Organization | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://kto.visitkorea.or.kr |
| Lao Statistics Bureau | Office statistique national | gratuit (données publiques) | 1 | 2 |  |
| Malaysian Automotive Association | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 2 | https://www.maa.org.my/index.html |
| Manheim Consulting Inc | Institut privé / association | payant | 1 | 2 | https://site.manheim.com/en/services/consulting/used-vehicle-value-index.html |
| Ministerio de Economia y Finanzas, Uruguay | Ministère / agence publique | gratuit (données publiques) | 1 | 2 |  |
| Ministerio de Hacienda Y Administraciones Publicas | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.hacienda.gob.es/ |
| Ministerio de Hacienda, Paraguay | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | http://www.economia.gov.py |
| Ministerul Finantelor Publice | Ministère / agence publique | gratuit (données publiques) | 1 | 2 |  |
| Ministry of Commerce of the People's Republic of China | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | http://www.mofcom.gov.cn |
| Ministry of Development Planning and Statistics | Office statistique national | gratuit (données publiques) | 1 | 2 |  |
| Ministry of Economic Development | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | http://economy.gov.ru |
| Ministry of Economy and Finance of Peru | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.mef.gob.pe |
| Ministry of Employment and Labor, South Korea | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.moel.go.kr |
| Ministry of Finance Malaysia | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.mof.gov.my/portal/en |
| Ministry of Finance of Moldova | Ministère / agence publique | gratuit (données publiques) | 1 | 2 |  |
| Ministry of Finance of Republic of Moldova | Ministère / agence publique | gratuit (données publiques) | 1 | 2 |  |
| Ministry of Finance of the Republic of Belarus Belarus | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.minfin.gov.by/en/ |
| Ministry of Finance of the Republic of Uzbekistan | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.mf.uz/uz/ |
| Ministry of Finance of the Russian Federation | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.minfin.ru |
| Ministry of Finance, Andorra | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.impostos.ad/ |
| Ministry of Finance, Cyprus | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | http://www.mof.gov.cy |
| Ministry of Finance, Government of Chile | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://hacienda.cl/ |
| Ministry of Finance, Iceland | Ministère / agence publique | gratuit (données publiques) | 1 | 2 |  |
| Ministry of Finance, Kuwait | Ministère / agence publique | gratuit (données publiques) | 1 | 2 |  |
| Ministry of Finance, Panama | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | http://www.mef.gob.pa |
| Ministry of Finance, Republic of Macedonia | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | http://www.finance.gov.mk |
| Ministry of Finance, Revenue Division, Brunei | Ministère / agence publique | gratuit (données publiques) | 1 | 2 |  |
| Ministry of Finance, Syria | Ministère / agence publique | gratuit (données publiques) | 1 | 2 |  |
| Ministry of Finance, Thailand | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | http://www2.mof.go.th/ |
| Ministry of Finance, Tourism and Development | Ministère / agence publique | gratuit (données publiques) | 1 | 2 |  |
| Ministry of Finance, United Arab Emirates | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.mof.gov.ae |
| Ministry of Health and Social Affairs, Sweden | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.government.se |
| Ministry of Labor and Social Protection of Population of the Republic of Kazakhstan | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.gov.kz/ |
| Ministry of Labour | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://mlsd.gov.sa |
| Ministry of Labour and Social Protection of Population of Turkmenistan | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | http://mlsp.gov.tm |
| Ministry of Labour, Human Resource Development and Training | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://labour.govmu.org/SitePages/Index.aspx |
| Ministry of Labour, Social Affairs and Family of the Slovak Republic | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | http://www.employment.gov.sk/ |
| Ministry of Labour, War Invalids, and Social Affairs | Ministère / agence publique | gratuit (données publiques) | 1 | 2 |  |
| Ministry of Land, Infrastructure and Transport | Office statistique national | gratuit (données publiques) | 1 | 2 | https://stat.molit.go.kr/portal/main/portalMain.do |
| Ministry of Social Affairs and Health | Ministère / agence publique | gratuit (données publiques) | 1 | 2 |  |
| Ministry of Social Affairs, Liechtenstein | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.ahv.li |
| Ministry of Taxes | Ministère / agence publique | gratuit (données publiques) | 1 | 2 |  |
| Ministry of Trade, Industry and Energy | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | http://english.motie.go.kr |
| Ministère du Budget | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://mbudget.gov.gn/ |
| Ministère du Travail, de L'emploi et de la Sécurité Sociale | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.pwc.com |
| Monentary Authority of Macao | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | http://www.amcm.gov.mo |
| Myanmar Statistical Information Service - MMSIS | Office statistique national | gratuit (données publiques) | 1 | 2 | http://mmsis.gov.mm |
| NAAMSA, South Africa | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 2 | https://www.naamsa.co.za |
| National Bank of Austria | Banque centrale | gratuit (données publiques) | 1 | 2 | https://www.oenb.at/en/ |
| National Bureau of Statistics of Tanzania | Office statistique national | gratuit (données publiques) | 1 | 2 | https://www.nbs.go.tz |
| National Bureau of Statistics, Maldives | Office statistique national | gratuit (données publiques) | 1 | 2 | http://statisticsmaldives.gov.mv/ |
| National Development Council (NDC), Taiwan | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | http://www.ndc.gov.tw |
| National Housing Bank, India | Banque centrale | gratuit (données publiques) | 1 | 2 | https://residex.nhbonline.org.in/ |
| National Institute of Statistics | Office statistique national | gratuit (données publiques) | 1 | 2 |  |
| National Institute of Statistics (INE) | Office statistique national | gratuit (données publiques) | 2 | 2 | https://www.ine.cl/ |
| National Property Information Center, Malaysia | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://napic.jpph.gov.my/en |
| National Statistical Institute of Bulgaria | Office statistique national | gratuit (données publiques) | 1 | 2 | http://www.nsi.bg |
| Nepal Central Bureau of Statistics | Office statistique national | gratuit (données publiques) | 1 | 2 |  |
| OICA - International Organization of Motor Vehicle Manufacturers | Institut privé / association | chiffre-titre gratuit, série payante | 2 | 2 | http://www.oica.net/ |
| Office of Industrial Economics, Thailand | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | http://www.oie.go.th |
| Office of Travel and Tourism Industries | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://travel.trade.gov |
| PT Astra International TBK | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.astra.co.id/ |
| Pension Fund of the Russian Federation | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | http://www.pfrf.ru/ |
| Philippine Central Bank | Banque centrale | gratuit (données publiques) | 1 | 2 | http://www.bsp.gov.ph |
| Philippine National Statistical Coordination Board | Office statistique national | gratuit (données publiques) | 1 | 2 |  |
| Public Revenues and Tax Department, Qatar | Ministère / agence publique | gratuit (données publiques) | 1 | 2 |  |
| Republic of Poland | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.gov.pl/ |
| Road Transport Department Malaysia | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | http://www.mot.gov.my |
| SECMCA - Consejo Monetario Centroamericano | Organisation internationale | gratuit (données publiques) | 1 | 2 | http://www.secmca.org |
| Secretaria de Hacienda y Credito Publico | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.gob.mx/shcp |
| Service Canada | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.canada.ca |
| Singapore Economic Development Board | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.edb.gov.sg/ |
| Social Insurance Agency, Slovak Republic | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | http://www.socpoist.sk |
| Social Protection Fund of the Ministry of Labor and Social Protection of the Republic of Belarus | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://ssf.gov.by/ru |
| Social Service Agency | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | http://ssa.gov.ge |
| Solomon Islands National Statistics Office | Office statistique national | gratuit (données publiques) | 1 | 2 | https://www.cbsi.com.sb/ |
| South Africa Government Services | Ministère / agence publique | gratuit (données publiques) | 1 | 2 |  |
| Stat Togo | Office statistique national | gratuit (données publiques) | 1 | 2 | http://www.stat-togo.org |
| State Social Insurance Agency, Latvia | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | http://www.vsaa.gov.lv |
| State Social Insurance Fund Board | Ministère / agence publique | gratuit (données publiques) | 1 | 2 |  |
| State Social Security Service | Ministère / agence publique | gratuit (données publiques) | 1 | 2 |  |
| Supreme Council of the Republic of Tajikistan | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | http://www.mehnat.tj |
| Suriname Debt Management Office | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | http://www.sdmo.org |
| Swedbank | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 2 | https://www.swedbank.com/ |
| Swedish National Debt Office | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.riksgalden.se/ |
| Swiss Economic Institute (KOF) | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://kof.ethz.ch/ |
| Tax Administration (Porezna Uprava) | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://porezna-uprava.gov.hr/en |
| The Central Reserve Bank of El Salvador | Banque centrale | gratuit (données publiques) | 1 | 2 |  |
| The Conference Board | Institut privé / association | chiffre-titre gratuit, série payante | 2 | 2 | https://www.conference-board.org/us/ |
| The ISET Policy Institute | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 2 | http://iset-pi.ge |
| The Inland Revenue Department of the Republic of Cyprus | Ministère / agence publique | gratuit (données publiques) | 1 | 2 |  |
| The Republic of Croatia, Tax Administration | Organisation internationale | gratuit (données publiques) | 1 | 2 | http://siteresources.worldbank.org |
| The national database of legislation of the Republic of Uzbekistan | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | http://lex.uz |
| Tourism Malaysia | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://www.tourism.gov.my |
| Truckstop | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 2 | https://truckstop.com/ |
| Turkish Automotive Manufacturers Association (OSD) | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 2 | http://www.osd.org.tr |
| U.S. Treasury | Ministère / agence publique | gratuit (données publiques) | 1 | 2 | https://home.treasury.gov/ |
| United Nations Refugee Agency | Organisation internationale | gratuit (données publiques) | 2 | 2 | http://popstats.unhcr.org |
| Universidad Torcuato di Tella | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 2 | https://www.utdt.edu |
| VDA - German Association of the Automotive Industry | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 2 | https://www.vda.de |
| Venezuela Central Bank | Banque centrale | gratuit (données publiques) | 1 | 2 | http://www.bcv.org.ve/ |
| Westpac Banking Corporation, Melbourne Institute | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 2 | http://www.westpac.com.au |
| ZIMSTAT, Zimbabwe | Office statistique national | gratuit (données publiques) | 1 | 2 | http://www.zimstat.co.zw |
| AA Ireland | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.theaa.ie |
| ACAP | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.acap.pt |
| ADB - Asian Development Bank | Organisation internationale | gratuit (données publiques) | 1 | 1 |  |
| ADEFA - The Association of Automotive Makers, Argentina | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | http://www.adefa.org.ar |
| ANOFM, Romania | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| ANZ - Indeed Australian Job Ads | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://media.anz.com/ |
| Adimark GfK | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.gfk.com |
| Administración Nacional de Combustibles Alcohol y Pórtland | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.ancap.com.uy |
| Administrative Office of the U.S. Courts | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.uscourts.gov |
| Afghanistan Bank | Banque centrale | gratuit (données publiques) | 1 | 1 | http://www.centralbank.gov.af/ |
| Agence pour le développement de l'emploi (ADEM) | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://adem.public.lu/en.html |
| Agencia Nacional de Hidrocarburos | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.anh.gob.bo |
| Agencija za statistiku Bosne i Hercegovine | Office statistique national | gratuit (données publiques) | 1 | 1 | https://bhas.gov.ba/Home/ |
| Agency for Natural Resources and Energy | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.enecho.meti.go.jp |
| Agency on Statistics under President of the Republic of Tajikistan | Office statistique national | gratuit (données publiques) | 1 | 1 | http://stat.tj |
| Agency on Statistics, Tajikistan | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.stat.tj |
| Agência Nacional do Petróleo, Gás Natural e Biocombustíveis | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.gov.br/anp/pt-br |
| Amt für Statistik Berlin-Brandenburg | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.statistik-berlin-brandenburg.de/ |
| Asociacion Nacional de Empresarios de Colombia (ANDI) | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | http://www.andi.com.co |
| Asociación National Automotriz de Chile, ANAC | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.anac.cl/ |
| Asociación de Fábricas de Automotores (ADEFA) | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | http://www.adefa.org.ar |
| Associates for Development Services Limited | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Association of Job Information of Japan | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 |  |
| Australia Chamber of Commerce and Industry | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://acci.com.au/ |
| Australian Bureau of Statistic | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.abs.gov.au |
| Australian Government Department of Finance | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.finance.gov.au |
| Australian Institute of Petroleum | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.aip.com.au |
| Australian Office of Financial Management (AOFM) | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.aofm.gov.au |
| Australian Securities and Investments Commission | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://asic.gov.au/ |
| Autoridade Tributária | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Bahamas Department of Statistics | Office statistique national | gratuit (données publiques) | 1 | 1 | http://www.bahamas.gov.bs/ |
| Banco Central de Reserva | Banque centrale | gratuit (données publiques) | 1 | 1 | https://www.bcr.gob.sv/documental/Inicio/busqueda/135 |
| Banco Central de Reserva de El Salvador | Banque centrale | gratuit (données publiques) | 1 | 1 | http://www.bcr.gob.sv |
| Banco Central de São Tomé e Príncipe | Banque centrale | gratuit (données publiques) | 1 | 1 |  |
| Banco Central de la Republica Argentina | Banque centrale | gratuit (données publiques) | 1 | 1 |  |
| Banco Central de la República Argentina | Banque centrale | gratuit (données publiques) | 1 | 1 | http://www.bcra.gob.ar |
| Banco de São Tomé e Príncipe | Banque centrale | gratuit (données publiques) | 1 | 1 | http://www.bcstp.st/ |
| Banco de la Republica | Banque centrale | gratuit (données publiques) | 1 | 1 |  |
| Bangchak Corporation Public Company Limited | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.bangchak.co.th/en/home |
| Bangladesh Petroleum Corporation | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://bpc.gov.bd/ |
| Bank Hapoalim/ILPMA | Banque centrale | gratuit (données publiques) | 1 | 1 | https://www.iplma.org.il/ |
| Bank of Central African States (BEAC) | Organisation internationale | gratuit (données publiques) | 1 | 1 | https://www.beac.int/ |
| Bank of Guatemala | Banque centrale | gratuit (données publiques) | 1 | 1 | https://www.banguat.gob.gt |
| Bank of South Sudan | Banque centrale | gratuit (données publiques) | 1 | 1 | https://boss.gov.ss/ |
| Banks Association of Turkey | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 |  |
| Banque Centrale de Mauritanie | Banque centrale | gratuit (données publiques) | 1 | 1 | http://www.bcm.mr |
| Banque Centrale des Etats de l’Afrique de l’Ouest (BCEAO) | Organisation internationale | gratuit (données publiques) | 1 | 1 | https://www.bceao.int/index.php/fr |
| Bayerisches Landesamt für Statistik | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.statistik.bayern.de/ |
| Brazilian Institute of Geography and Statistics (IBGE) | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.ibge.gov.br |
| Bureau of Economic Analysis (BEA) | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.bea.gov |
| Bureau of National Statistics of the Republic of Kazakhstan | Office statistique national | gratuit (données publiques) | 1 | 1 | http://stat.gov.kz |
| CAPMAS | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.capmas.gov.eg |
| CFIB - Canadian Federation of Independent Business | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.cfib-fcei.ca/en |
| CKGSB Cheung Kong Graduate School of Business | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://english.ckgsb.edu.cn/ |
| CKGSB Knowledge | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://english.ckgsb.edu.cn/knowledge/ |
| CNBC/National Retail Federation | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://nrffoundation.org/ |
| CNI - Brazilian National Confederation of Industry | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.portaldaindustria.com.br |
| CPB Netherlands Bureau for Economic Policy Analysis | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.cpb.nl |
| CSU - Czech Statistical Office | Office statistique national | gratuit (données publiques) | 1 | 1 | http://www.czso.cz |
| Caisse Nationale de Sécurité Sociale, Benin | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://cnss.bj/ |
| Canada Mortgage And Housing Corporation | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Centaline Property Agency Ltd., Hong Kong | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://hk.centanet.com |
| Center for Economic Research and Reforms (CERR) | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.cer.uz/uzc/ |
| Central Agency for Public Mobilization and Statistics (CAPMAS) | Office statistique national | gratuit (données publiques) | 1 | 1 |  |
| Central Bank Liberia | Banque centrale | gratuit (données publiques) | 1 | 1 | https://www.cbl.org.lr/ |
| Central Bank of Brazil | Banque centrale | gratuit (données publiques) | 1 | 1 | https://www.bcb.gov.br/ |
| Central Bank of Cuba | Banque centrale | gratuit (données publiques) | 1 | 1 | http://www.bc.gob.cu |
| Central Bank of Hungary | Banque centrale | gratuit (données publiques) | 1 | 1 | http://www.mnb.hu |
| Central Bank of Mauritanita | Banque centrale | gratuit (données publiques) | 1 | 1 | http://www.bcm.mr |
| Central Bank of South Sudan | Banque centrale | gratuit (données publiques) | 1 | 1 |  |
| Central Bank of the UAE | Banque centrale | gratuit (données publiques) | 1 | 1 |  |
| Central Department of Statistics and Information | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.stats.gov.sa |
| Central Electricity Authority, Ministry of Power, Government of India | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://npp.gov.in/ |
| Central Statistical Bureau (Kuwait) | Office statistique national | gratuit (données publiques) | 1 | 1 | http://www.csb.gov.kw |
| Centro de Investigaciones Sociológicas | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.cis.es |
| Ceylon Tourist Board | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.sltda.gov.lk/en |
| Chamber of Automotive Manufacturers of the Philippines (CAMPI) | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.campiauto.org |
| Chamber of Commerce | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://portaldocomercio.org.br/ |
| Chile Central Bank | Banque centrale | gratuit (données publiques) | 1 | 1 | https://www.bcentral.cl/en |
| Chilean Benchmark Facility | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://cbf.cl/about-cbf/ |
| China Real Estate Information Corp | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.cric.com/ |
| Circle K, Lithuania | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.circlek.lt |
| City University of Hong Kong | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.cb.cityu.edu.hk/dao/scu/?page=introduction |
| Comisión Nacional de Energía, Chile | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.cne.cl/ |
| Confederation of Finnish Industries (EK) | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://ek.fi/ |
| Consumer Council, Hong Kong | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.consumer.org.hk |
| Cotality, Australia | Institut privé / association | payant | 1 | 1 | https://www.corelogic.com.au/ |
| Council of Labour Affairs | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://english.mol.gov.tw |
| Croatian Tax Administration | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.porezna-uprava.hr/Stranice/Naslovnica.aspx |
| Czech Financial Benchmark Facility | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://cfbf.cz/ |
| DG ECFIN - Directorate General for Economic and Financial Affairs | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://ec.europa.eu |
| DGEEC | Office statistique national | gratuit (données publiques) | 1 | 1 | http://www.dgeec.gov.py |
| DILF - Danish Purchasing and Logistics Forum | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.dilf.dk |
| Da Afghanistan Bank | Banque centrale | gratuit (données publiques) | 1 | 1 | https://dab.gov.af/ |
| Danish Financial Benchmark Facility | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://dfbf.dk/ |
| Debt Management Office, Nigeria | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.dmo.gov.ng/ |
| Departamento de Asuntos del Consumidor (DACO) | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.daco.pr.gov |
| Department for Business, Energy & Industrial Strategy, United Kingdom | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.gov.uk/government/organisations/department-for-business-energy-and-industrial-strategy |
| Department for Work and Pensions, UK | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.gov.uk |
| Department of Census and Statistics - Sri Lanka | Office statistique national | gratuit (données publiques) | 1 | 1 |  |
| Department of Employment Affairs and Social Protection | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.lowpaycommission.ie/ |
| Department of Employment and Labour - South Africa | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.gov.za/ |
| Department of Employment, Ministry of Labour | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.doe.go.th/prd/lmia?page=home |
| Department of Energy, Republic of Philippines | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.doe.gov.ph |
| Department of Finance & Administration (DOFA) | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Department of Finance and Administration | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Department of Finance of the Philippine Republic | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.dof.gov.ph |
| Department of Investment Review, MOEA | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.moea.gov.tw/Mns/dir_e/home/Home.aspx |
| Department of Labor | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.dol.gov |
| Department of Labor and Employment | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.dole.gov.ph |
| Department of Revenue and Customs | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Department of Tourism, India | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.tourism.gov.in/ |
| Department of Tourism, Philippines | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.tourism.gov.ph/dot/ |
| Department of Treasury | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.treasury.gov.pg/ |
| Department of Treasury, Papua New Guinea | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.treasury.gov.pg/ |
| Department of the Treasury of Puerto Rico | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://hacienda.pr.gov/ |
| Development Bank of Singapore | Banque centrale | gratuit (données publiques) | 1 | 1 | https://www.dbs.com |
| Digestyc, El Salvador | Office statistique national | gratuit (données publiques) | 1 | 1 | http://www.digestyc.gob.sv/index.php |
| Direccion De Presupuestos, Chile | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.dipres.gob.cl |
| Dirección General de Estadística, Encuestas y Censos | Office statistique national | gratuit (données publiques) | 1 | 1 | http://www.dgeec.gov.py |
| Dirección Reguladora de Hidrocarburos y Minas | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.dgehm.gob.sv/ |
| Direction Generale des Impots et des Domaines | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.finances.gouv.sn/ |
| Direction Générale des Impôts, Morocco | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Direction Générale du Tresor | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.tresorpublic.mg/ |
| Directorate of Investment and Company Administration, Myanmar | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.dica.gov.mm/en |
| Directorate of Labour, Iceland | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.vinnumalastofnun.is/ |
| Direcção-Geral de Energia e Geologia, Portugal | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.precoscombustiveis.dgeg.pt/ |
| Dun & Bradstreet | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 |  |
| EFKA, Greece | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Economic and Financial Committee | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Electricite du Liban | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | http://www.bdl.gov.lb |
| Employment Ministry | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Enemed, Malta | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | http://www.enemed.com.mt |
| Energy Department of Republic of South Africa | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.energy.gov.za |
| Energy Regulatory Commission of the Republic of Macedonia | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.erc.org.mk |
| Energy and Petroleum Regulatory Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.epra.go.ke/ |
| Energy and Water Utilities Regulatory Authority (EWURA) | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.ewura.go.tz |
| Estonian Institute of Economic Research | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.ki.ee |
| Estonian Transport Administration | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.transpordiamet.ee/ |
| Estonian Unemployment Insurance Fund | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.tootukassa.ee/en |
| European Commssion | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://ec.europa.eu |
| FEBIAC | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.febiac.be/fr |
| FENALCO, Federación Nacional de Comerciantes Empresarios | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.fenalco.com.co/ |
| FIPE - Fundacao Instituto de Pesquisas Economicas | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.fipe.org.br/ |
| FRED | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://fred.stlouisfed.org/ |
| Fair Work Commission | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.fwc.gov.au |
| Federal Chamber of Automotive Industries | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 |  |
| Federal Reserve Bank of Atlanta | Banque centrale | gratuit (données publiques) | 1 | 1 | https://www.atlantafed.org/ |
| Federal Reserve Bank of San Francisco | Banque centrale | gratuit (données publiques) | 1 | 1 | https://www.frbsf.org/ |
| Federal Reserve Bank of St. Louis | Banque centrale | gratuit (données publiques) | 1 | 1 | https://fred.stlouisfed.org/ |
| Finance Department, San Marino | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://sanmarinoca.gov/government/departments/finance/index.php |
| Finance Ministry of the Republic of Moldova | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Financial Management Service, US Treasury | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.fiscal.treasury.gov |
| Financial Service Bureau | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Finans Norge & Verian | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.finansnorge.no |
| FipeZAP | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | http://fipezap.zapimoveis.com.br/ |
| Fit Small Business | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 |  |
| Fitch Auto ABS Indices | Institut privé / association | payant | 1 | 1 | https://www.fitchratings.com/structured-finance/abs/auto-indices |
| Foreign Trade Chamber of Bosnia and Herzegovina | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://komorabih.ba |
| GFK | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.gfk.com |
| GOIL Company Limited (GOIL) | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.goil.com.gh/ |
| Gallup/Capacent | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | http://www.gallup.is |
| Gambia Bureau of Statistics (GBoS) | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.gbosdata.org/ |
| General Administration of Customs of the People’s Republic of China | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.customs.gov.cn/ |
| General Commission for Taxes | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| General Department of Taxation Cambodia | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| General Organization for Social Insurance, Bahrain | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.sio.bh |
| Georgia Department of Revenue | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Getulio Vargas Foundation | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://portalibre.fgv.br/ |
| GlobalDairyTrade | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.globaldairytrade.info/ |
| Gobierno de Mexico | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.gob.mx/ |
| GovHK | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.gov.hk |
| Government Debt Management Agency Ltd, Hungary | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Government Development Bank for Puerto Rico | Banque centrale | gratuit (données publiques) | 1 | 1 | http://www.gdb-pur.com |
| Government of Pakistan Ministry of Finance | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.finance.gov.pk |
| Government of Singapore | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.gov.sg/ |
| Government of the Federation of Bosnia and Herzegovina | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://fbihvlada.gov.ba/ |
| Greek Tourism Confederation | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://sete.gr/ |
| Greenlandic Tax Agency | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://aka.gl/ |
| HALPIM | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | http://logisztika.hu |
| Haut Commissariat au Plan, Morocco | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.hcp.ma/ |
| Hellenic Association of Motor Vehicle Importers-Representatives | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.seaa.gr |
| Hellenic Ministry of Development, Competitiveness, Infrastructure, Transport and Networks | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.fuelprices.gr/ |
| Home Office, UK Government | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.gov.uk/government/statistical-data-sets/asylum-and-resettlement-datasets |
| Hong Kong Association of Banks | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | http://www.hkab.org.hk/ |
| Housing Industry Association (HIA) | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://hia.com.au |
| INDEC | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.indec.gob.ar/ |
| INE - Instituto Nacional de Estatistica de Angola | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.ine.gov.ao/ |
| INE - Instituto Nicaragüense de Energía | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.ine.gob.ni |
| INE Honduras | Office statistique national | gratuit (données publiques) | 1 | 1 | http://www.ine.gob.hn |
| INE, Sao Tome and Principe | Office statistique national | gratuit (données publiques) | 1 | 1 | http://www.ine.st |
| INEC | Office statistique national | gratuit (données publiques) | 1 | 1 |  |
| INEGI - Instituto Nacional de Estadistica, Geografia e Informatica, Mexico | Office statistique national | gratuit (données publiques) | 1 | 1 | http://www.inegi.org.mx/ |
| INEGI, Mexico | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.inegi.org.mx/ |
| INS, Ivory Coast | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | http://www.ins.ci/n/ |
| INSD - Institut National de la Statistique et de la Demographie | Office statistique national | gratuit (données publiques) | 1 | 1 | http://www.insd.bf |
| INSEED Comoros | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.inseed-comores.org/ |
| Iceland Automobile Association | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | http://www.fib.is |
| Icelandic Tourist Board | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.ferdamalastofa.is |
| Icelandic Transport Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://island.is/en/o/transport-authority |
| Immigration Department and Cayman Islands Department of Tourism | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.eso.ky |
| Immigration New Zealand | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.immigration.govt.nz/ |
| Immigration, Refugees and Citizenship Canada | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.canada.ca/en.html |
| Indian Oil Corporation Ltd | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://iocl.com/home.aspx |
| Info Sapiens | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.sapiens.com.ua/ua/index |
| Inland Revenue Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Inland Revenue Department of Saint Lucia | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://irdstlucia.gov.lc/ |
| Inland Revenue Division Solomon Islands | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.ird.gov.sb |
| Insitut National de la Statistique (INSTAT) Madagascar | Office statistique national | gratuit (données publiques) | 1 | 1 | http://instat.mg |
| Institut Centrafricain des Statistiques et des Etudes Economiques et Sociales, CAR | Office statistique national | gratuit (données publiques) | 1 | 1 | https://icasees.org/ |
| Institut Haïtien de Statistique et d'Informatique | Office statistique national | gratuit (données publiques) | 1 | 1 |  |
| Institut National de la Statistique et de la Demographie | Office statistique national | gratuit (données publiques) | 1 | 1 | http://www.insd.bf/ |
| Institut National de la Statistique et de la Demographie (INStad) | Office statistique national | gratuit (données publiques) | 1 | 1 | https://instad.bj/ |
| Institut National de la Statistique, des Etudes Economiques et Démographiques | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.inseed.td/ |
| Institute of Statistics (INSTAT) | Office statistique national | gratuit (données publiques) | 1 | 1 | http://www.instat.gov.al |
| Instituto Chileno de Administración Racional de Empresas | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.icare.cl |
| Instituto Guatemalteco de Turismo, INGUAT | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://inguat.gob.gt/ |
| Instituto Nacional de Estadistica | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.ine.gob.bo |
| Instituto Nacional de Estadistica de Bolivia | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.ine.gob.bo |
| Instituto Nacional de Estadistica e Informatica | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.inei.gob.pe/ |
| Instituto Nacional de Estatistica Cabo Verde | Office statistique national | gratuit (données publiques) | 1 | 1 | https://ine.cv/ |
| Instituto Nacional de Información de Desarollo - INIDE | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.inide.gob.ni/ |
| Instituto Nacional de Información de Desarrrolo | Office statistique national | gratuit (données publiques) | 1 | 1 | http://www.inide.gob.ni/ |
| Instituto de Empleo, Servicio Publico de Empleo Estatal, Spain | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.sepe.es |
| Internal Revenue Directorate | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.rsk.is |
| Iran Vehicle Manufacturers Association | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | http://ivma.ir |
| Irish League of Credit Unions | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.creditunion.ie/ |
| Ivey Business School | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Japan Automobile Dealers Association | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | http://www.jada.or.jp/ |
| Japan Automobile Manufacturers Association (JAMA) | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | http://www.jama-english.jp/ |
| Japan Cement Association (JCA) | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | http://jcassoc.or.jp |
| Japan Machine Tool Builders' Association (JMTBA) | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.jmtba.or.jp |
| Japan National Tourist Organization | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.japan.travel |
| Japan Productivity Center for Socio-Economic Development | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.jpc-net.jp |
| Japan Real Estate Institute (JREI) | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.reinet.or.jp/ |
| Japanese Bankers Association | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.jbatibor.or.jp/ |
| KOSTAT - Statistics Korea | Office statistique national | gratuit (données publiques) | 1 | 1 | http://kostat.go.kr |
| Kookmin bank, South Korea | Banque centrale | gratuit (données publiques) | 1 | 1 | https://www.kbstar.com/ |
| Korea Customs Service | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.customs.go.kr/english/main.do |
| Korea Electric Power Corporation (KEPCO) | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | http://home.kepco.co.kr |
| Korea National Oil Corporation | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.knoc.co.kr |
| Korea Real Estate Board | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.reb.or.kr/reb/main.do |
| Kostat | Office statistique national | gratuit (données publiques) | 1 | 1 | https://mods.go.kr/ |
| Kuwait National Petroleum Company | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.knpc.com |
| Labour Bureau, Government of India | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Labour Department of the Government of the Hong Kong Special Administrative Region | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.labour.gov.hk |
| Land Transport Authority Singapore | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.lta.gov.sg |
| Landesbetrieb Information und Technik Nordrhein-Westfalen | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.it.nrw/ |
| Liberia Revenue Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://revenue.lra.gov.lr/ |
| Libyan Tax Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Lithuanian Labour Exchange | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.ldb.lt |
| MBIE, New Zealand | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.mbie.govt.nz |
| Malawi Revenue Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.mra.mw/ |
| Malta National Statistics Office | Office statistique national | gratuit (données publiques) | 1 | 1 | http://www.nso.gov.mt |
| Maybank Group | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.maybank.com |
| Minimum Wage Council | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.minimumwage.go.kr |
| Minister of Economy, Finance, & Planning | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Ministerio das Financas de Angola | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Ministerio de Economía y Finanzas, Paraguay | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.mef.gov.py/ |
| Ministerio de Energía y Minas, República de Guatemala | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://mem.gob.gt |
| Ministerio de Finanzas Públicas, Guatemala | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.minfin.gob.gt |
| Ministerio de Finanzas y Precios de la República de Cuba | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.mfp.gob.cu/inicio/portada.php |
| Ministerio de Fomento | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.fomento.gob.es/ |
| Ministerio de Industria, Energia Y Turismo | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://industria.gob.es/ |
| Ministerio de Minas y Energía | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.minenergia.gov.co |
| Ministerio de Trabajo y Desarrollo Laboral | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.mitradel.gob.pa/ |
| Ministerio de Trabajo y Previsión Social, El Salvador | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.mtps.gob.sv/ |
| Ministerio de Trabajo y Previsión Social, Guatemala | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.mintrabajo.gob.gt/ |
| Ministerio de Trabajo, Empleo y Seguridad Social | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://argentina.gob.ar |
| Ministerio de Transportes, Movilidad y Agenda Urbana | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.mitma.gob.es/ |
| Ministerio de Turismo y Deportes Argentina | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.yvera.tur.ar/ |
| Ministerio de Turismo, Uruguay | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://mintur.gub.uy |
| Ministerio del Poder Popular de Petróleo y Mineria | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.minpet.gob.ve |
| Ministerio del Trabajo Nicaragua | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.mitrab.gob.ni/ |
| Ministry Of Labour, Cambodia | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.mlvt.gov.kh/ |
| Ministry of Administrative Development, Labour and Social Affairs | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Ministry of Budget, Ministry of Finance | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://english.mofe.go.kr/ |
| Ministry of Business, Innovation & Employment | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Ministry of Business, Innovation and Employment | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Ministry of Commerce, Cambodia | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.moc.gov.kh |
| Ministry of Culture and Tourism, Turkey | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://yigm.ktb.gov.tr/ |
| Ministry of Domestic Trade, Cooperatives and Consumerism | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Ministry of Economic Affairs and Finance | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Ministry of Economic Affairs, Taiwan | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.moea.gov.tw/Mns/dos_e/home/Home.aspx |
| Ministry of Economic Inclusion, Small Business, Employment and Skills, Morocco | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://miepeec.gov.ma/ |
| Ministry of Economy of the Republic of Belarus | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.minfin.gov.by |
| Ministry of Employment and Labor Relations of the Republic of Uzbekistan | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://mehnat.uz/uz |
| Ministry of Employment and Labour Relations, Ghana | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.melr.gov.gh/ |
| Ministry of Employment and the Economy, Finland | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://tem.fi |
| Ministry of Employment, Labour Productivity and Skills Development | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.gov.bw/ |
| Ministry of Energy & Infrastructure, UAE | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.moei.gov.ae/ |
| Ministry of Energy and Mineral Resources, Republic of Indonesia | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.esdm.go.id/en |
| Ministry of Energy of Belarus | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://president.gov.by/en/statebodies/ministry-of-energy |
| Ministry of Energy, Industry and Mineral Resources | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.mim.gov.sa/en |
| Ministry of Environment and Tourism, Namibia | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.meft.gov.na/ |
| Ministry of Finance & Treasury, Solomon Islands | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://solomons.gov.sb/ministry-of-finance-and-treasury/ |
| Ministry of Finance & the Public Service | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.mof.gov.jm/ |
| Ministry of Finance Russia | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://minfin.gov.ru/ |
| Ministry of Finance and Economy of Armenia | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.armstat.am/en/ |
| Ministry of Finance and National Planning | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Ministry of Finance and Planning | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Ministry of Finance and Public Credit Republic of Colombia | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Ministry of Finance of Republic of Kazakhstan | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Ministry of Finance of the Kyrgyz Republic | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.minfin.gov.kg/ |
| Ministry of Finance of the Republic of Armenia | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.minfin.am/en/ |
| Ministry of Finance of the Slovak Republic | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.finance.gov.sk |
| Ministry of Finance, Budget and the Banking Sector | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://finances.gouv.km/ |
| Ministry of Finance, Customs Administration (Taiwan Customs Administration) | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://web.customs.gov.tw/en/ |
| Ministry of Finance, Georgia | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.mof.ge/ |
| Ministry of Finance, Lebanon | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Ministry of Finance, National Planning and Trade, Republic of Seychelles | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.finance.gov.sc/ |
| Ministry of Finance, Tax Administration, Croatia | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.vatlive.com |
| Ministry of Finance, Yemen | Office statistique national | gratuit (données publiques) | 1 | 1 | http://www.cso-yemen.org |
| Ministry of Gender, Labour and Social Development | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://mglsd.go.ug/ |
| Ministry of Housing, Communities and Local Government, UK | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.gov.uk |
| Ministry of Human Resources Malaysia | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.mohr.gov.my |
| Ministry of Justice, South Korea | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.moj.go.kr/moj/index.do |
| Ministry of Labor & Skills, Ethiopia | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://mols.gov.et/ |
| Ministry of Labor and Social Policy, Macedonia | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.rsm.global |
| Ministry of Labor and Social Welfare, Laos | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.molsw.gov.la/ |
| Ministry of Labor, Dominican Republic | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://mt.gob.do/ |
| Ministry of Labour & Employment | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://labour.gov.in |
| Ministry of Labour and Employment, India | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Ministry of Labour and Employment, Lesotho | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.gov.ls/ |
| Ministry of Labour and Social Development | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Ministry of Labour and Social Policy of the Republic of Bulgaria | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Ministry of Labour and Social Protection Republic of Belarus | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.mintrud.gov.by |
| Ministry of Labour and Social Protection, Moldova | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://social.gov.md/en/ |
| Ministry of Labour, Jordan | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.mol.gov.jo/Default/En |
| Ministry of Labour, Oman | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://gov.om/en/ministry-of-labour |
| Ministry of Labour, Vietnam | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://english.molisa.gov.vn |
| Ministry of Manpower and Transmigration | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Ministry of National Infrastructures, Energy and Water Resources | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.gov.il/he/departments/ministry_of_energy |
| Ministry of National Planning and Economic Development, Myanmar | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Ministry of Oil and Gas, Oman | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.ea.gov.om/ |
| Ministry of Overseas Pakistanis and Human Resource Development | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://ophrd.gov.pk/ |
| Ministry of Petroleum | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.petroleum.gov.eg |
| Ministry of Planning and Finance, Government of Venezuela | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Ministry of Production of Peru | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Ministry of Revenue & Customs | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.revenue.gov.to/ |
| Ministry of Strategy and Finance, South Korea | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://english.moef.go.kr/ |
| Ministry of Tourism & Sports, Thailand | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.thaiembassy.org |
| Ministry of Tourism, Brazil | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://dados.turismo.gov.br/ |
| Ministry of Tourism, Morocco | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Ministry of Transportation, R.O.C | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.motc.gov.tw/en/index |
| Ministry of Treasury and Finance, Turkey | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://en.hmb.gov.tr/ |
| Ministry of taxes of the Republic of Azerbaijan | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Ministry of the Economy & Finance | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Ministry of the Economy and Finance | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.hacienda.gob.es/en-GB/Paginas/Home.aspx |
| Ministry of the Interior, Taiwan | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.moi.gov.tw/Default.aspx |
| Ministère de l'Economie et des Finances, Haiti | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.mef.gouv.ht |
| Ministère des Affaires Sociales et du Travail, Haiti | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Ministère du Travail de l'Emploi et de la Sécurité Sociale, Algeria | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.mtess.gov.dz/fr/ |
| Ministère du Travail, de l'Emploi, de la Fonction Publique et des Lois Sociales, Madagascar | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.mfptls.gov.mg/ |
| Ministério da Administração Publica Trabalho e Segurança Social, Angola | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Ministério da Fazenda, Brazil | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://idg.receita.fazenda.gov.br |
| Ministério do Trabalho e Emprego, MTE | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://trabalho.gov.br |
| Minstry of Labour and Social Policy, Bulgaria | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Monetary Authority, Singapore | Banque centrale | gratuit (données publiques) | 1 | 1 | https://www.mas.gov.sg/ |
| NAV - Norwegian Labour and Welfare Organisation | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.nav.no/ |
| NIMA/DNB Markets | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.nima.no |
| Namibia Statistics Agency | Office statistique national | gratuit (données publiques) | 1 | 1 | https://nsa.org.na/ |
| National Administrative Department of Statistics (DANE) | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.dane.gov.co |
| National Association of Home Builders | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.nahb.org |
| National Bank of Albania | Banque centrale | gratuit (données publiques) | 1 | 1 | https://www.bankofalbania.org/home/ |
| National Bank of Rwanda (BNR) | Banque centrale | gratuit (données publiques) | 1 | 1 | https://www.bnr.rw/home/ |
| National Bank of the Republic of North Macedonia | Banque centrale | gratuit (données publiques) | 1 | 1 | http://www.nbrm.mk |
| National Bureau for Taxation, Bahrain | Office statistique national | gratuit (données publiques) | 1 | 1 |  |
| National Bureau of Statistics | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.nbs.gov.sc/ |
| National Centre for Statistics and Information (Oman) | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.ncsi.gov.om/ |
| National Federation of Independent Business | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.nfib.com |
| National Information System | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| National Institute of Economic Research (NIER) | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.konj.se/ |
| National Institute of Statistics of Uruguay | Office statistique national | gratuit (données publiques) | 1 | 1 |  |
| National Institute of Statistics, Cambodia | Office statistique national | gratuit (données publiques) | 1 | 1 | http://www.nis.gov.kh |
| National Institute of Statistics, Economic and Demographic Studies of Chad (INSEED) | Office statistique national | gratuit (données publiques) | 1 | 1 |  |
| National Insurance Institute of Israel | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.btl.gov.il |
| National Iranian Oil Products Distribution Company | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.niopdc.ir/ |
| National Minimum Wage Committee of Myanmar | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| National Oil and Gas Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://noga.gov.bh/moo/default.aspx |
| National Revenue Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.nra.gov.sl/ |
| National Statistical Coordination Board (NSCB), Philippines | Office statistique national | gratuit (données publiques) | 1 | 1 |  |
| National Statistical Office of Papua New Guinea | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.nso.gov.pg |
| National Statistics Bureau | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.nsb.gov.bt/ |
| National Statistics Office | Office statistique national | gratuit (données publiques) | 1 | 1 | https://nso.gov.mt/ |
| National Statistics Office of Malawi | Office statistique national | gratuit (données publiques) | 1 | 1 | http://www.nsomalawi.mw |
| National Statistics Office of Vietnam | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.nso.gov.vn/ |
| National Statistics of Georgia | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.geostat.ge/en |
| Nationwide Building Society, UK | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.nationwide.co.uk/ |
| Natural Resources Canada | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.nrcan.gc.ca |
| Nepal Central Bank | Banque centrale | gratuit (données publiques) | 1 | 1 |  |
| New Zealand Insolvency and Trustee Service | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.insolvency.govt.nz/ |
| New Zealand Institute of Economic Research | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://nzier.org.nz/ |
| North Korea Statistics | Office statistique national | gratuit (données publiques) | 1 | 1 |  |
| North Korean Economy Watch | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.nkeconwatch.com/ |
| Norwegian Labour and Welfare Organisation | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.nav.no |
| ONS, Algerie | Office statistique national | gratuit (données publiques) | 1 | 1 |  |
| OPEC | Organisation internationale | gratuit (données publiques) | 1 | 1 | https://www.opec.org/opec_web/en/ |
| Office of Management and Budget, The White House | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.whitehouse.gov |
| Office of the Economic Adviser, India | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://eaindustry.nic.in/ |
| Oficina Nacional Estadística | Office statistique national | gratuit (données publiques) | 1 | 1 |  |
| Oficina Nacional de Estadística y Censos (ONEC), Banco Central de Reserva de El Salvador | Banque centrale | gratuit (données publiques) | 1 | 1 | https://onec.bcr.gob.sv/ |
| Oficina Nacional de Estadística y CensosOficina Nacional de Estadística y Censos | Office statistique national | gratuit (données publiques) | 1 | 1 | https://onec.bcr.gob.sv/ |
| Oficina del Comisionado de Instituciones Financieras | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | http://www.ocif.pr.gov |
| Olerex AS | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | http://olerex.ee |
| Oman National Center for Statistics and Information | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.ncsi.gov.om |
| Oppotus Research | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.oppotus.com/ |
| Organisation for Economic Co-operation and Development | Organisation internationale | gratuit (données publiques) | 1 | 1 | https://www.oecd.org/ |
| Organismo Supervisor de la Inversión en Energía y Minería, Peru | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.osinergmin.gob.pe/empresas/hidrocarburos |
| PETROIntelligence | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://petrointelligence.com/precios-de-la-gasolina-y-diesel-hoy.php |
| Pakistan State Oil | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Palestine Monetary Authority (PMA) | Banque centrale | gratuit (données publiques) | 1 | 1 | https://www.pma.ps/en/ |
| Papua New Guinea National Statistics Office | Office statistique national | gratuit (données publiques) | 1 | 1 |  |
| Peking University Institute of Social Sciences | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Petrojam Limited | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | http://www.petrojam.com |
| Petroleum Products Pricing Regulatory Agency | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Petropar | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | http://www.petropar.gov.py/ |
| Prime Minister’s Office – Labour, Youth, Employment and Persons with Disabilities, Tanzania | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.kazi.go.tz/ |
| Public Employment Service Austria (AMS) | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.ams.at |
| Puerto Rican Department of Treasury | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Qatar Petroleum | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.qp.com.qa |
| Qatar Planning and Statistics Authority | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.npc.qa/en/Pages/default.aspx |
| RAC Motoring Services, UK | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.rac.co.uk |
| RECOPE - Refinadora Costarricense de Petróleo | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.recope.go.cr |
| Rating and Valuation Department Hong Kong Government | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Real Estate Norway | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://eiendomnorge.no/en/ |
| Redbook Research Inc | Institut privé / association | payant | 1 | 1 | http://www.redbookresearch.com/ |
| Refugee Council of Australia | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.refugeecouncil.org.au/ |
| Republic of Bulgaria National Statistic Institute | Office statistique national | gratuit (données publiques) | 1 | 1 | http://www.nsi.bg |
| Research Center for Taiwan Economic Development (RCTED) | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://rcted.ncu.edu.tw/ |
| Retail Fuel Prices Observatory, Cyprus Government | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://mobile.eservices.cyprus.gov.cy |
| Retailers Association of India (RAI) | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://rai.net.in/index.php |
| Revenue Authority (Office Burundais des Recettes) | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.obr.bi/ |
| Royal Institution of Chartered Surveyors (RICS) | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.rics.org/uk/ |
| Secretariat for Economy and Finance | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Secretariat of Labor & Soc Welf, Mexico | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.gob.mx |
| Secretaría de Energía de Panamá | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | http://www.energia.gob.pa |
| Secretaría de Energía, Honduras | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://sen.hn/ |
| Secretaría de Energía, República Argentina | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://argentina.gob.ar |
| Secretaría de Trabajo y Seguridad Social, Honduras | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.trabajo.gob.hn/ |
| Secretaría de Trabajo, Empleo y Seguridad Social, Ministerio de Capital Humano | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Securities Consultants Ltd., Bangladesh | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 |  |
| Servicio de Impuestos Nacionales | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Shanghai Port | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.portshanghai.com.cn/ |
| Shell Luxembourg | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.shell.lu |
| Shell Suisse | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.shell.ch |
| Shell i Danmark | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.shell.dk |
| Singapore Institute of Purchasing & Materials Management, SIPMM | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Singapore Petroleum Company Limited | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.spc.com.sg/ |
| Singapore Statistics | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.singstat.gov.sg/ |
| Singapore Tourism Board | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.stb.gov.sg |
| Sinyi Realty Inc | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 |  |
| Social Insurance Organization | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.sio.gov.bh/ |
| Social Insurance and Pension Agency, Tajikistan | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://nafaka.tj/en |
| South African Chamber of Commerce and Industry (SACCI) | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.sacci.org.za |
| South Korea Economics Statistics System | Office statistique national | gratuit (données publiques) | 1 | 1 | https://ecos.bok.or.kr/#/ |
| State Employment Agency, Latvia | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.nva.gov.lv/en |
| State Statistical Committee of the Republic of Azerbaijan | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.stat.gov.az/?lang=en |
| State Statistical Committee, Azerbaijan | Office statistique national | gratuit (données publiques) | 1 | 1 | http://www.stat.gov.az |
| Statistica Moldovei | Office statistique national | gratuit (données publiques) | 1 | 1 | https://statistica.gov.md/ro |
| Statistical Centre of Iran | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.amar.org.ir/ |
| Statistical Committee of the Republic of Armenia | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.armstat.am |
| Statistical Office of Montenegro (MONSTAT) | Office statistique national | gratuit (données publiques) | 1 | 1 | http://www.monstat.org |
| Statistical Service, Ministry of Finance of Republic of Cyprus | Office statistique national | gratuit (données publiques) | 1 | 1 | http://www.mof.gov.cy |
| Statistics Botswana | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.statsbots.org.bw/ |
| Statistics Korea (KOSTAT) | Office statistique national | gratuit (données publiques) | 1 | 1 | http://kostat.go.kr |
| Statistics Mauritius | Office statistique national | gratuit (données publiques) | 1 | 1 | https://statsmauritius.govmu.org/ |
| Statistics NZ | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.stats.govt.nz/ |
| Statistik Hessen | Office statistique national | gratuit (données publiques) | 1 | 1 | https://statistik.hessen.de/ |
| Statistisches Landesamt Baden-Württemberg | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.statistik-bw.de/ |
| Statistisches Landesamt des Freistaates Sachsen | Office statistique national | gratuit (données publiques) | 1 | 1 | https://www.statistik.sachsen.de/ |
| Statoil Fuel & Retail | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.circlek.lv |
| Superintendencia Financiera de Colombia | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.superfinanciera.gov.co |
| Swaziland Revenue Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.ers.org.sz/ |
| Swedish Financial Benchmark Facility | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://swfbf.se/ |
| Swedish National Mediation Office | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.mi.se/ |
| Swedish Public Employment Service | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://arbetsformedlingen.se/ |
| TOBB - Union of Chambers and Commodity Exchanges of Turkey | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 |  |
| TTVMA - Taiwan Transportation Vehicle Manufacturers Association | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.ttvma.org.tw/ |
| Taiwan | Office statistique national | gratuit (données publiques) | 1 | 1 | http://eng.stat.gov.tw |
| Tankan Sponsored by Thomson Reuters | Institut privé / association | payant | 1 | 1 |  |
| Tariff (price) Council of Azerbaijan Republic | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.tariffcouncil.gov.az |
| Tax Committee Ministry of Finance of the Republic of Kazakhstan | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://kgd.gov.kz/ru |
| Taxation and Revenue | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Technometrica Market Intelligence/RealClearMarkets | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 |  |
| Terna - Rete Elettrica Nazionale, Italy | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.terna.it/it |
| The Central Bank of The Bahamas | Banque centrale | gratuit (données publiques) | 1 | 1 | http://www.centralbankbahamas.com |
| The China Passenger Car Association (CPCA) | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.cpcaauto.com/ |
| The Economics & Statistics Office | Office statistique national | gratuit (données publiques) | 1 | 1 | http://www.eso.ky |
| The Federal Service of Labour and Employment | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| The Government of Bahamas | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.bahamas.gov.bs/ |
| The Government of Cape Verde | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| The Hashemite Kingdom of Jordan Department of Statistics | Office statistique national | gratuit (données publiques) | 1 | 1 |  |
| The Insolvency Service, United Kingdom | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.gov.uk |
| The Institute for Applied Economic Research (IPEA) | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.ipea.gov.br |
| The Mauritius Chamber of Commerce and Industry | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| The Ministry of Oil and Gas, Sudan | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.mop.gov.sd/ |
| The People's Bank of China | Banque centrale | gratuit (données publiques) | 1 | 1 | https://www.pbc.gov.cn/ |
| The Transparency Board | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | http://www.mylpg.eu |
| The ministry of economy of the Kyrgyz Republic | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://mineconom.gov.kg/en |
| Tokyo Shoko Research | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.tsr-net.co.jp |
| Torcuato di Tella University | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.utdt.edu |
| Treasury | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| Türkiye Petrolleri | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.tppd.com.tr/ |
| U.S. Bureau of Economics Analysis | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.bea.gov |
| UBS & CFA Society Switzerland | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://cfasocietyswitzerland.org/ |
| UK Finance | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| US Bankruptcy Court of the District of Puerto Rico | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.prb.uscourts.gov |
| US Department of Justice | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.justice.gov/ |
| Unified Federal Register of Information on Bankruptcy | Ministère / agence publique | gratuit (données publiques) | 1 | 1 |  |
| United States International Trade Commission | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.usitc.gov/ |
| University of the Thai Chamber of Commerce | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://ism.utcc.ac.th/ |
| Vietnam Automobile Manufacturers Association | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | http://vama.org.vn |
| Vietnam National Petroleum Group | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.petrolimex.com.vn |
| Wards Intelligence | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://wardsintelligence.informa.com/ |
| Westpac Banking Corporation, McDermott Miller | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.westpac.co.nz |
| YouGov/Citigroup | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 |  |
| Zimbabwe Energy Regulatory Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | https://www.zera.co.zw |
| Zimbabwe Tourism Authority | Ministère / agence publique | gratuit (données publiques) | 1 | 1 | http://www.zimbabwetourism.net |
| https://www.bundesbank.de/en | Banque centrale | gratuit (données publiques) | 1 | 1 |  |
| procure.ch & Credit Suisse | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.procure.ch |
| procure.ch & UBS | Institut privé / association | chiffre-titre gratuit, série payante | 1 | 1 | https://www.procure.ch |

### Sources par pays

- **AFG** (AFG, 53 indicateurs) : National Statistics and Information Authority (NSIA) (28) · World Bank (6) · Central Statistics Organization of Afghanistan (3) · World Health Organization (3) · Afghanistan Revenue Department (3) · Transparency International (2) · AKAMAI (2) · International Monetary Fund (1) · Afghanistan Bank (1) · SIPRI (1) · Da Afghanistan Bank (1) · Institute for Economics and Peace (1) · International Labor Organization (ILO) (1)
- **ASM** (ASM, 4 indicateurs) : AKAMAI (2) · World Bank (2)
- **Albania** (ALB, 112 indicateurs) : Instituti i Statistikave (INSTAT) (50) · Bank of Albania (21) · Albanian Taxation Office (7) · Ministry of Finance of the Republic of Albania (7) · World Bank (6) · World Health Organization (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · ISSH, Albania (2) · U.S. Energy Information Administration (1) · National Bank of Albania (1) · The Transparency Board (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · EUROSTAT (1) · OECD (1) · SIPRI (1) · Institute of Statistics (INSTAT) (1) · Institute for Economics and Peace (1)
- **Algeria** (DZA, 79 indicateurs) : Office National des Statistiques (ONS) (33) · Bank of Algeria (10) · World Bank (9) · Direction Générale des Impôts, Algeria (6) · World Health Organization (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Ministère du Travail, de L'emploi et de la Sécurité Sociale (2) · Organization of the Petroleum Exporting Countries (1) · Baker Hughes Company (1) · International Monetary Fund (1) · ONS, Algerie (1) · World Gold Council (1) · IMF (1) · SIPRI (1) · Ministère du Travail de l'Emploi et de la Sécurité Sociale, Algeria (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **Andorra** (AND, 13 indicateurs) : World Bank (4) · World Health Organization (3) · AKAMAI (2) · Ministry of Finance, Andorra (2) · United Nations Economic Commission for Europe (UNECE) (1) · OECD (1)
- **Angola** (AGO, 86 indicateurs) : Instituto Nacional de Estatística, Angola (29) · National Bank of Angola (21) · World Bank (7) · Direcção Nacional dos Impostos, Ministério das Finanças (6) · World Health Organization (3) · Ministry of Finance of Angola (3) · World Economic Forum (2) · Transparency International (2) · African Development Bank (2) · AKAMAI (2) · Central Bank of Angola (2) · Baker Hughes Company (1) · Ministerio das Financas de Angola (1) · SIPRI (1) · Ministério da Administração Publica Trabalho e Segurança Social, Angola (1) · INE - Instituto Nacional de Estatistica de Angola (1) · Autoridade Tributária (1) · Institute for Economics and Peace (1)
- **Antigua and Barbuda** (ATG, 24 indicateurs) : National Bureau of Statistics Antigua and Barbuda (11) · World Bank (7) · World Health Organization (3) · AKAMAI (2) · Inland Revenue Department (1)
- **Argentina** (ARG, 118 indicateurs) : Instituto Nacional de Estadística y Censos (INDEC) (62) · Central Bank of Argentina (9) · World Bank (6) · Ministerio de Hacienda, Argentina (6) · World Health Organization (3) · Federal Administration of Public Revenue (3) · Federal Administration of Public Revenue (AFIP) (3) · World Economic Forum (2) · Universidad Torcuato di Tella (2) · Transparency International (2) · AKAMAI (2) · ADEFA - The Association of Automotive Makers, Argentina (1) · Asociación de Fábricas de Automotores (ADEFA) (1) · U.S. Energy Information Administration (1) · Inter-American Development Bank (1) · IMF (1) · Secretaría de Energía, República Argentina (1) · World Gold Council (1) · Banco Central de la República Argentina (1) · Bank for International Settlements (1) · Torcuato di Tella University (1) · Banco Central de la Republica Argentina (1) · SIPRI (1) · World Steel Association (1) · Institute for Economics and Peace (1) · Ministerio de Turismo y Deportes Argentina (1) · INDEC (1) · Ministerio de Trabajo, Empleo y Seguridad Social (1) · Secretaría de Trabajo, Empleo y Seguridad Social, Ministerio de Capital Humano (1)
- **Armenia** (ARM, 95 indicateurs) : National Statistical Service of the Republic of Armenia (43) · Central Bank of Armenia (20) · World Bank (6) · Ministry of Labor and Social Affairs (4) · World Health Organization (3) · Tax Service of Republic of Armenia (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · State Social Security Service (2) · World Gold Council (1) · Ministry of Finance of the Republic of Armenia (1) · United Nations Economic Commission for Europe (UNECE) (1) · OECD (1) · SIPRI (1) · Institute for Economics and Peace (1) · Statistical Committee of the Republic of Armenia (1) · Ministry of Finance and Economy of Armenia (1)
- **Aruba** (ABW, 28 indicateurs) : Aruba Central Bureau of Statistics (14) · World Health Organization (3) · Ministerio di Finansas, Comunicacion, Utilidad y Energia (3) · World Bank (3) · Centrale Bank van Aruba (2) · AKAMAI (2) · World Gold Council (1)
- **Australia** (AUS, 198 indicateurs) : Australian Bureau of Statistics (104) · Reserve Bank of Australia (19) · Australian Taxation Office (8) · OECD (7) · World Bank (6) · S&P Global (5) · Australian Industry Group (5) · National Australia Bank (3) · Federal Chamber of Automotive Industries (FCAI) (3) · World Health Organization (3) · Melbourne Institute (3) · World Economic Forum (2) · Westpac Banking Corporation, Melbourne Institute (2) · Transparency International (2) · Department of Finance and Administration, Australia (2) · Australian Government (2) · Bank for International Settlements (2) · AKAMAI (2) · SIPRI (2) · Refugee Council of Australia (1) · Australian Securities and Investments Commission (1) · Federal Chamber of Automotive Industries (1) · Cotality, Australia (1) · U.S. Energy Information Administration (1) · Australian Institute of Petroleum (1) · World Gold Council (1) · Australian Government Department of Finance (1) · Australian Office of Financial Management (AOFM) (1) · Australian Bureau of Statistic (1) · ANZ - Indeed Australian Job Ads (1) · Fair Work Commission (1) · Housing Industry Association (HIA) (1) · Australia Chamber of Commerce and Industry (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **Austria** (AUT, 151 indicateurs) : Statistics Austria (54) · EUROSTAT (26) · Oesterreichische Nationalbank (17) · Federal Ministry of Finance (9) · OECD (7) · World Bank (6) · European Commission (3) · Statistik Austria (3) · World Health Organization (3) · National Bank of Austria (2) · World Economic Forum (2) · Transparency International (2) · European Central Bank (2) · AKAMAI (2) · Arbeitsmarktservice Oesterreich (2) · SIPRI (2) · European Automobile Manufacturers’ Association (ACEA) (1) · Europe's Energy Portal (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · Bank for International Settlements (1) · S&P Global (1) · World Steel Association (1) · Institute for Economics and Peace (1) · Public Employment Service Austria (AMS) (1)
- **Azerbaijan** (AZE, 104 indicateurs) : The State Statistical Committee of the Republic of Azerbaijan (45) · Central Bank of the Republic of Azerbaijan (20) · World Bank (8) · Republic of Azerbaijan Ministry of Labour and Social Protection of Population (5) · World Health Organization (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Ministry of Taxes (2) · Central Bank of the Republic Azerbaijan (2) · Ministry of taxes of the Republic of Azerbaijan (1) · U.S. Energy Information Administration (1) · IMF (1) · State Statistical Committee, Azerbaijan (1) · SESRIC (1) · Tariff (price) Council of Azerbaijan Republic (1) · World Gold Council (1) · International Monetary Fund (1) · United Nations Economic Commission for Europe (UNECE) (1) · OECD (1) · State Statistical Committee of the Republic of Azerbaijan (1) · SIPRI (1) · Institute for Economics and Peace (1)
- **BMU** (BMU, 17 indicateurs) : Bermuda Government Department of Statistics (11) · Office of the Tax Commissioner, Government of Bermuda (3) · AKAMAI (2) · World Bank (1)
- **Bahamas** (BHS, 45 indicateurs) : Central Bank of The Bahamas (17) · World Bank (6) · Ministry of Finance, Bahamas (4) · Department of Statistics of The Bahamas (3) · World Health Organization (3) · Ministry of Labour and Immigration (3) · Transparency International (2) · AKAMAI (2) · IMF (1) · Inter-American Development Bank (1) · The Government of Bahamas (1) · The Central Bank of The Bahamas (1) · Bahamas Department of Statistics (1)
- **Bahrain** (BHR, 85 indicateurs) : Central Bank of Bahrain (21) · Information & eGovernment Authority, Bahrain (21) · World Bank (8) · Ministry of Finance - Kingdom of Bahrain (7) · Central Informatics Organisation - Kingdom of Bahrain (4) · World Health Organization (3) · Ministry of Finance and National Economy (3) · Ministry of Finance (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · U.S. Energy Information Administration (1) · General Organization for Social Insurance, Bahrain (1) · National Oil and Gas Authority (1) · World Gold Council (1) · IMF (1) · SIPRI (1) · National Bureau for Taxation, Bahrain (1) · Institute for Economics and Peace (1) · Social Insurance Organization (1)
- **Bangladesh** (BGD, 99 indicateurs) : Bangladesh Bank (35) · Bangladesh Bureau of Statistics (22) · Ministry of Finance, Bangladesh (9) · World Bank (8) · Bangladesh Bureau of Statistics (BBS) (4) · World Health Organization (3) · National Board of Revenue (NBR), Bangladesh (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Government of the People's Republic of Bangladesh (2) · Associates for Development Services Limited (1) · Securities Consultants Ltd., Bangladesh (1) · Bangladesh Petroleum Corporation (1) · World Gold Council (1) · SIPRI (1) · Employment Ministry (1) · Institute for Economics and Peace (1)
- **Barbados** (BRB, 52 indicateurs) : Central Bank Of Barbados (15) · Barbados Statistical Service (15) · World Bank (6) · Ministry of Finance, Barbados (4) · World Health Organization (3) · Ministry of Finance (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2)
- **Belarus** (BLR, 93 indicateurs) : National Statistical Committee of the Republic of Belarus (36) · National Bank of the Republic of Belarus (19) · World Bank (7) · World Health Organization (3) · Ministry for Taxes and Levies of the Republic of Belarus (3) · Ministry of Finance of the Republic of Belarus (3) · Ministry of Labor and Social Protection (3) · Transparency International (2) · Ministry of Finance of the Republic of Belarus Belarus (2) · AKAMAI (2) · SIPRI (2) · Social Protection Fund of the Ministry of Labor and Social Protection of the Republic of Belarus (2) · U.S. Energy Information Administration (1) · Ministry of Energy of Belarus (1) · World Gold Council (1) · Ministry of Economy of the Republic of Belarus (1) · United Nations Economic Commission for Europe (UNECE) (1) · OECD (1) · Ministry of Labour and Social Protection Republic of Belarus (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **Belgium** (BEL, 155 indicateurs) : National Bank of Belgium (43) · Statistics Belgium (30) · EUROSTAT (28) · OECD (8) · World Bank (6) · Banque Nationale de Belgique (5) · European Central Bank (4) · Service Public Fédéral Finances (4) · World Health Organization (3) · National Social Security Office (3) · BelgoStat (2) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · SIPRI (2) · Commission Bancaire, Financière et des Assurances (2) · European Commission (1) · ACEA - European Automobile Manufacturers' Association (1) · European Automobile Manufacturers’ Association (ACEA) (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · Bank for International Settlements (1) · FEBIAC (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **Belize** (BLZ, 70 indicateurs) : Statistical Institute of Belize (25) · Central Bank of Belize (25) · World Bank (7) · World Health Organization (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Belize Tax Services (2) · Inter-American Development Bank (1) · SIPRI (1)
- **Benin** (BEN, 53 indicateurs) : Central Bank of West African States (BCEAO) (9) · Institut National de la Statistique et de la Démographie (INStaD) (8) · Institut National de la Statistique et de l’Analyse Economique, Benin (7) · World Bank (6) · INSAE, Benin (3) · World Health Organization (3) · World Economic Forum (2) · Direction Générale des Impôts (2) · Transparency International (2) · BCEAO (2) · AKAMAI (2) · African Development Bank Group (1) · Central Bank of West African States (1) · SIPRI (1) · Caisse Nationale de Sécurité Sociale, Benin (1) · Institut National de la Statistique et de la Demographie (INStad) (1) · Institute for Economics and Peace (1) · International Labour Organization (ILO) (1)
- **Bhutan** (BTN, 64 indicateurs) : Royal Monetary Authority of Bhutan (15) · National Statistics Bureau of Bhutan (13) · National Statistics Bureau Bhutan (12) · World Bank (7) · Ministry of Finance, Bhutan (4) · World Health Organization (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · IMF (1) · Department of Revenue and Customs (1) · Institute for Economics and Peace (1) · National Statistics Bureau (1)
- **Bolivia** (BOL, 90 indicateurs) : Instituto Nacional de Estadística de Bolivia (41) · Banco Central de Bolivia (15) · World Bank (6) · Autoridad de Impugnación Tributaria (AIT), Bolivia (5) · Ministerio de Economía y Finanzas Públicas (4) · World Health Organization (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Instituto Nacional de Estadistica (1) · U.S. Energy Information Administration (1) · Inter-American Development Bank (1) · Agencia Nacional de Hidrocarburos (1) · Instituto Nacional de Estadistica de Bolivia (1) · World Gold Council (1) · International Monetary Fund (1) · SIPRI (1) · Servicio de Impuestos Nacionales (1) · Institute for Economics and Peace (1)
- **Bosnia and Herzegovina** (BIH, 103 indicateurs) : Bosnia and Herzegovina Agency for Statistics (BHAS) (42) · Central Bank of Bosnia and Herzegovina (24) · World Bank (6) · Agency for Statistics of Bosnia and Herzegovina (4) · Indirect Taxation Authority of Bosnia and Herzegovina (4) · World Health Organization (3) · Tax Administration of the Federation of B&H (3) · Federal Office of Statistics, Federation of Bosnia and Herzegovina (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Agencija za statistiku Bosne i Hercegovine (1) · Foreign Trade Chamber of Bosnia and Herzegovina (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · SIPRI (1) · Government of the Federation of Bosnia and Herzegovina (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **Botswana** (BWA, 74 indicateurs) : Central Statistics Office, Botswana (28) · Bank of Botswana (22) · World Bank (5) · Botswana Unified Revenue Service (4) · World Economic Forum (2) · Transparency International (2) · Central Statistics Office (2) · AKAMAI (2) · African Development Bank Group (1) · International Monetary Fund (1) · SIPRI (1) · Ministry of Employment, Labour Productivity and Skills Development (1) · Statistics Botswana (1) · Institute for Economics and Peace (1) · International Labor Organization (ILO) (1)
- **Brazil** (BRA, 139 indicateurs) : Instituto Brasileiro de Geografia e Estatística (IBGE) (45) · Banco Central do Brasil (31) · Secretaria da Receita Federal do Brasil (6) · World Bank (5) · Ministério do Desenvolvimento, Indústria e Comércio Exterior (3) · Confederacao Nacional da Industria (CNI) (3) · S&P Global (3) · World Health Organization (3) · FUNCEX, Brazil (3) · FGV - Fundacao Getulio Vargas (3) · Ministério da Previdência Social (3) · ANFAVEA, Brazil (2) · World Economic Forum (2) · OECD (2) · Transparency International (2) · Bank for International Settlements (2) · AKAMAI (2) · SIPRI (2) · U.S. Energy Information Administration (1) · IMF (1) · Chamber of Commerce (1) · Agência Nacional do Petróleo, Gás Natural e Biocombustíveis (1) · World Gold Council (1) · Ministério da Fazenda, Brazil (1) · FipeZAP (1) · Central Bank of Brazil (1) · FIPE - Fundacao Instituto de Pesquisas Economicas (1) · The Institute for Applied Economic Research (IPEA) (1) · Ministério do Trabalho e Emprego, MTE (1) · Brazilian Institute of Geography and Statistics (IBGE) (1) · Getulio Vargas Foundation (1) · CNI - Brazilian National Confederation of Industry (1) · World Steel Association (1) · Institute for Economics and Peace (1) · Ministry of Tourism, Brazil (1)
- **Brunei** (BRN, 56 indicateurs) : Department of Economic Planning and Development, Brunei (27) · Autoriti Monetari Brunei Darussalam (4) · Brunei Darussalam Central Bank (4) · World Bank (4) · World Health Organization (3) · Treasury Department, Ministry of Finance and Economy (3) · World Economic Forum (2) · Ministry of Finance, Revenue Division, Brunei (2) · Transparency International (2) · International Monetary Fund (2) · U.S. Energy Information Administration (1) · Asian Development Bank (1) · SIPRI (1)
- **Bulgaria** (BGR, 146 indicateurs) : National Statistical Institute, Bulgaria (62) · EUROSTAT (21) · Bulgarian National Bank (16) · National Revenue Agency, Bulgaria (7) · World Bank (7) · Ministry of Finance of the Republic of Bulgaria (5) · World Health Organization (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · SIPRI (2) · National Statistical Institute of Bulgaria (2) · National Revenue Agency (2) · European Commission (1) · ACEA - European Automobile Manufacturers' Association (1) · European Automobile Manufacturers’ Association (ACEA) (1) · Europe's Energy Portal (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · OECD (1) · Republic of Bulgaria National Statistic Institute (1) · Bank for International Settlements (1) · World Steel Association (1) · Institute for Economics and Peace (1) · Minstry of Labour and Social Policy, Bulgaria (1) · Ministry of Labour and Social Policy of the Republic of Bulgaria (1)
- **Burkina Faso** (BFA, 59 indicateurs) : Central Bank of West African States (BCEAO) (13) · Institut National de la Statistique et de la Demographie, Burkina Faso (9) · INSD, Burkina Faso (8) · World Bank (7) · World Health Organization (3) · General Direction of Taxes (3) · BCEAO (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · African Development Bank Group (1) · Central Bank of West African States (1) · SIPRI (1) · INSD - Institut National de la Statistique et de la Demographie (1) · Institut National de la Statistique et de la Demographie (1) · Institute for Economics and Peace (1) · ILO (1)
- **Burundi** (BDI, 49 indicateurs) : Bank of Burundi (23) · World Bank (8) · L'Institut de Statistiques et d’Etudes Economiques du Burundi (ISTEEBU) (5) · Institut de Statistiques et d’Etudes Economiques du Burundi (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · SIPRI (1) · Revenue Authority (Office Burundais des Recettes) (1) · Institute for Economics and Peace (1) · International Labour Organization (ILO) (1)
- **CUB** (CUB, 39 indicateurs) : National Office of Statistics, Republic of Cuba (22) · World Bank (4) · World Health Organization (3) · Transparency International (2) · AKAMAI (2) · U.S. Energy Information Administration (1) · Central Bank of Cuba (1) · SIPRI (1) · Ministerio de Finanzas y Precios de la República de Cuba (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **CYM** (CYM, 28 indicateurs) : Cayman Islands - The Economics & Statistics Office (14) · World Bank (5) · World Health Organization (3) · Ministry of Finance, Tourism and Development (2) · AKAMAI (2) · The Economics & Statistics Office (1) · Immigration Department and Cayman Islands Department of Tourism (1)
- **Cambodia** (KHM, 65 indicateurs) : National Bank of Cambodia (22) · Asian Development Bank (ADB) (9) · World Bank (7) · National Institute of Statistics of Cambodia (5) · World Health Organization (3) · World Economic Forum (2) · Transparency International (2) · International Monetary Fund (2) · AKAMAI (2) · General Department of Taxation (2) · General Department of Taxation Cambodia (1) · Ministry of Commerce, Cambodia (1) · World Gold Council (1) · Asian Development Bank (1) · Ministry of Economy and Finance (1) · SIPRI (1) · Ministry Of Labour, Cambodia (1) · Institute for Economics and Peace (1) · National Institute of Statistics, Cambodia (1)
- **Cameroon** (CMR, 55 indicateurs) : Institut National de la Statistique du Cameroun (23) · World Bank (7) · Bank of Central African States (5) · World Health Organization (3) · Ministry of Labor and Social Security (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Ministry of Finance (2) · General Direction of Taxes (1) · U.S. Energy Information Administration (1) · International Monetary Fund (1) · SIPRI (1) · Institute for Economics and Peace (1) · International Labour Organization (ILO) (1)
- **Canada** (CAN, 179 indicateurs) : Statistics Canada (110) · Bank of Canada (10) · OECD (8) · Canada Revenue Agency (7) · World Bank (6) · Department Of Finance Canada (5) · S&P Global (3) · World Health Organization (3) · Canadian Real Estate Association (CREA) (2) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · SIPRI (2) · Service Canada (2) · Automatic Data Processing, Inc (1) · Immigration, Refugees and Citizenship Canada (1) · Ivey Business School (1) · Ipsos (1) · U.S. Energy Information Administration (1) · Baker Hughes Company (1) · Natural Resources Canada (1) · World Gold Council (1) · IMF (1) · United Nations Economic Commission for Europe (UNECE) (1) · Canada Mortgage And Housing Corporation (1) · Bank for International Settlements (1) · CFIB - Canadian Federation of Independent Business (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **Cape Verde** (CPV, 75 indicateurs) : INE, Cape Verde (35) · Banco de Cabo Verde (20) · World Bank (7) · World Health Organization (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · SIPRI (1) · The Government of Cape Verde (1) · Instituto Nacional de Estatistica Cabo Verde (1) · Ministry of Finance (1)
- **Central African Republic** (CAF, 27 indicateurs) : World Bank (8) · Bank of Central African States (5) · World Health Organization (3) · Transparency International (2) · AKAMAI (2) · International Monetary Fund (1) · Institut Centrafricain des Statistiques et des Etudes Economiques et Sociales, CAR (1) · Central Bank of West African States (BCEAO) (1) · SIPRI (1) · Ministry of Finance (1) · Institute for Economics and Peace (1) · International Labour Organization (ILO) (1)
- **Chad** (TCD, 42 indicateurs) : Bank of Central African States (7) · World Bank (7) · General Inspectorate of Finance (6) · Institut National de la Statistique du Tchad (INSEED) (4) · World Health Organization (3) · World Economic Forum (2) · Transparency International (2) · Institut National de la Statistique, des Etudes Economiques et Démographiques (INSEED), Tchad (2) · AKAMAI (2) · U.S. Energy Information Administration (1) · International Monetary Fund (IMF) (1) · SIPRI (1) · National Institute of Statistics, Economic and Demographic Studies of Chad (INSEED) (1) · Institut National de la Statistique, des Etudes Economiques et Démographiques (1) · Institute for Economics and Peace (1) · International Labour Organization (ILO) (1)
- **Chile** (CHL, 122 indicateurs) : Central Bank of Chile (30) · National Institute of Statistics, Chile (28) · Banco Central de Chile (13) · Chilean Tax Administration (8) · World Bank (7) · OECD (5) · World Health Organization (3) · Chilean Chamber of Construction (3) · World Economic Forum (2) · Transparency International (2) · DIPRES - Direccion de Presupuestos, Chile (2) · Ministry of Finance, Government of Chile (2) · AKAMAI (2) · Instituto Chileno de Administración Racional de Empresas (1) · Adimark GfK (1) · Inter-American Development Bank (1) · Comisión Nacional de Energía, Chile (1) · World Gold Council (1) · Direccion De Presupuestos, Chile (1) · Bank for International Settlements (1) · Chilean Benchmark Facility (1) · National Institute of Statistics (INE) (1) · SIPRI (1) · Ministry of Labor and Social Welfare (1) · Chile Central Bank (1) · World Steel Association (1) · Institute for Economics and Peace (1) · Asociación National Automotriz de Chile, ANAC (1)
- **China** (CHN, 190 indicateurs) : National Bureau of Statistics of China (80) · People's Bank of China (24) · General Administration of Customs (14) · State Administration of Taxation (9) · State Administration of Foreign Exchange, China (7) · World Bank (7) · Ministry of Human Resources and Social Security, China (4) · Ministry of Finance of the People's Republic of China (4) · China Association of Automobile Manufacturers (3) · OECD (3) · S&P Global (3) · World Health Organization (3) · World Economic Forum (2) · Transparency International (2) · Ministry of Commerce of the People's Republic of China (2) · Bank for International Settlements (2) · AKAMAI (2) · SIPRI (2) · MNI Deutsche Börse Group (2) · CKGSB Knowledge (1) · The China Passenger Car Association (CPCA) (1) · Organisation for Economic Co-operation and Development (1) · Shanghai Port (1) · U.S. Energy Information Administration (1) · World Gold Council (1) · IMF (1) · Peking University Institute of Social Sciences (1) · CKGSB Cheung Kong Graduate School of Business (1) · The Conference Board (1) · The People's Bank of China (1) · China Real Estate Information Corp (1) · General Administration of Customs of the People’s Republic of China (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **Colombia** (COL, 108 indicateurs) : Dane, Colombia (42) · Banco de la República de Colombia (16) · World Bank (7) · OECD (3) · Banco de la Republica de Colombia (3) · World Health Organization (3) · Directorate of National Taxes and Customs (DIAN) (3) · Banco de la Republica, Colombia (3) · Instituto de Seguros Sociales (3) · Banco de la República, Colombia (2) · Fedesarrollo (2) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Asociacion Nacional de Empresarios de Colombia (ANDI) (1) · FENALCO, Federación Nacional de Comerciantes Empresarios (1) · Superintendencia Financiera de Colombia (1) · U.S. Energy Information Administration (1) · National Administrative Department of Statistics (DANE) (1) · Inter-American Development Bank (1) · Ministerio de Minas y Energía (1) · World Gold Council (1) · Ministry of Finance and Public Credit Republic of Colombia (1) · Bank for International Settlements (1) · Banco de la Republica (1) · S&P Global (1) · SIPRI (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **Comoros** (COM, 31 indicateurs) : Banque Centrale des Comores (16) · World Bank (8) · Transparency International (2) · AKAMAI (2) · INSEED Comoros (1) · Ministry of Finance, Budget and the Banking Sector (1) · International Labour Organization (ILO) (1)
- **Congo - Brazzaville** (COG, 34 indicateurs) : World Bank (9) · CNSEE - Centre Nationale de la Statistique et des Etudes Economiques, Congo (4) · World Health Organization (3) · Ministry of Commerce (3) · Ministry of Labor, Employment, and Social Security (3) · Transparency International (2) · CNSEE, Congo (2) · Organization of the Petroleum Exporting Countries (1) · African Development Bank Group (1) · International Monetary Fund (1) · Bank of Central African States (1) · Central Bank of West African States (BCEAO) (1) · SIPRI (1) · Institute for Economics and Peace (1) · International Labour Organization (ILO) (1)
- **Congo - Kinshasa** (COD, 61 indicateurs) : Banque Centrale du Congo (36) · World Bank (5) · DRC Tax Authority (4) · World Health Organization (3) · Ministry of Labor and Social Security (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · U.S. Energy Information Administration (1) · International Monetary Fund (IMF) (1) · SIPRI (1) · Institute for Economics and Peace (1)
- **Costa Rica** (CRI, 99 indicateurs) : Central Bank of Costa Rica (44) · Instituto Nacional de Estadística y Censos de Costa Rica (22) · World Bank (7) · Directorate General of Taxation of Costa Rica (4) · World Health Organization (3) · Banco Central de Costa Rica (3) · Ministerio de Trabajo y Seguridad Social de Costa Rica (3) · OECD (2) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Consejo Monetario Centroamericanono (1) · Inter-American Development Bank (1) · RECOPE - Refinadora Costarricense de Petróleo (1) · Consejo Monetario Centroamericano (1) · Institute for Economics and Peace (1)
- **Croatia** (HRV, 134 indicateurs) : Croatian Bureau of Statistics (49) · Croatian National Bank (27) · EUROSTAT (23) · World Bank (6) · World Health Organization (3) · Tax Administration - Ministry of Finance (3) · European Commission (2) · World Economic Forum (2) · Tax Administration (Porezna Uprava) (2) · Transparency International (2) · AKAMAI (2) · The Republic of Croatia, Tax Administration (2) · ACEA - European Automobile Manufacturers' Association (1) · European Automobile Manufacturers’ Association (ACEA) (1) · Petrol (1) · United Nations Economic Commission for Europe (UNECE) (1) · OECD (1) · SIPRI (1) · Croatian Tax Administration (1) · Bank for International Settlements (1) · Ministry of Finance, Tax Administration, Croatia (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **Cyprus** (CYP, 121 indicateurs) : Statistical Service of the Republic of Cyprus (52) · EUROSTAT (22) · Central Bank of Cyprus (9) · Inland Revenue Department of the Republic of Cyprus (6) · World Bank (5) · European Commission (3) · World Health Organization (3) · European Central Bank (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Ministry of Finance, Cyprus (2) · The Inland Revenue Department of the Republic of Cyprus (2) · European Automobile Manufacturers’ Association (ACEA) (1) · Retail Fuel Prices Observatory, Cyprus Government (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · OECD (1) · SIPRI (1) · Institute for Economics and Peace (1) · Statistical Service, Ministry of Finance of Republic of Cyprus (1)
- **Czechia** (CZE, 159 indicateurs) : Czech Statistical Office (63) · EUROSTAT (24) · Czech National Bank (24) · OECD (5) · World Bank (5) · Czech Social Security Administration (5) · Financial Administration of the Czech Republic (4) · World Health Organization (3) · Labor Office of the Czech Republic (3) · World Economic Forum (2) · Czech Statistical Office (CSU) (2) · Transparency International (2) · European Automobile Manufacturers’ Association (ACEA) (2) · AKAMAI (2) · SIPRI (2) · European Automobile Manufacturers Association (1) · Europe's Energy Portal (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · Bank for International Settlements (1) · Czech Financial Benchmark Facility (1) · European Central Bank (1) · S&P Global (1) · CSU - Czech Statistical Office (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **Côte d’Ivoire** (CIV, 56 indicateurs) : Central Bank of West African States (BCEAO) (11) · World Bank (7) · BCEAO (5) · Intitut National de la Statistique (3) · World Health Organization (3) · Institut National de la Statistique (3) · Institut National de la Statistique de Côte d'Ivoire (3) · Ministry of Employment, Social Affairs and Professional Formation (3) · World Economic Forum (2) · Transparency International (2) · Inst Natl De La Statistique, Ivory Coast (2) · AKAMAI (2) · Ministry of Finance (2) · General Direction of Taxes (1) · U.S. Energy Information Administration (1) · AFDB (1) · INS, Ivory Coast (1) · Central Bank of West African States (1) · SIPRI (1) · Institute for Economics and Peace (1) · International Labour Organization (ILO) (1)
- **Denmark** (DNK, 153 indicateurs) : Statistics Denmark (76) · EUROSTAT (17) · Danmarks Nationalbank (16) · Danish Central Tax Administration (9) · OECD (8) · World Bank (5) · World Health Organization (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · SIPRI (2) · European Commission (1) · U.S. Energy Information Administration (1) · European Automobile Manufacturers’ Association (ACEA) (1) · Shell i Danmark (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · Bank for International Settlements (1) · Danish Financial Benchmark Facility (1) · European Central Bank (1) · DILF - Danish Purchasing and Logistics Forum (1) · Institute for Economics and Peace (1)
- **Djibouti** (DJI, 43 indicateurs) : Central Bank of Djibouti (19) · World Bank (8) · National Institute of Statistics of Djibouti (INSTAD) (5) · World Health Organization (3) · Transparency International (2) · AKAMAI (2) · International Monetary Fund (1) · Minister of Economy, Finance, & Planning (1) · Institute for Economics and Peace (1) · International Labour Organization (ILO) (1)
- **Dominica** (DMA, 26 indicateurs) : Eastern Caribbean Central Bank (11) · World Bank (6) · World Health Organization (3) · Transparency International (2) · AKAMAI (2) · Inland Revenue Division, Dominica (2)
- **Dominican Republic** (DOM, 74 indicateurs) : Central Bank of Dominican Republic (42) · World Bank (5) · Central American Monetary Council (3) · World Health Organization (3) · Direccion General de Impuestos Internos (3) · World Economic Forum (2) · Transparency International (2) · SEMCA - Consejo Monetario Centroamericano (2) · Dirección General de Crédito Público, Ministerio de Hacienda (2) · AKAMAI (2) · Consejo Monetario Centroamericanono (1) · Consejo Monetario Centroamericano (1) · Inter-American Development Bank (1) · World Gold Council (1) · SIPRI (1) · Ministry of Labor, Dominican Republic (1) · Oficina Nacional Estadística (1) · Institute for Economics and Peace (1)
- **ERI** (ERI, 25 indicateurs) : World Bank (10) · AFDB (4) · World Health Organization (3) · Transparency International (2) · AKAMAI (2) · African Development Bank Group (1) · Ministry of Finance (1) · Institute for Economics and Peace (1) · International Labour Organization (ILO) (1)
- **Ecuador** (ECU, 101 indicateurs) : Banco Central del Ecuador (42) · INEC, Ecuador (23) · World Bank (6) · Instituto Nacional de Estadistica y Censos (INEC) (5) · World Health Organization (3) · Servicio de Rentas Internas del Ecuador (3) · Ministerio de Economía y Finanzas, Ecuador (3) · Instituto Ecuatoriano de Seguridad Social (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · U.S. Energy Information Administration (1) · Baker Hughes Company (1) · World Gold Council (1) · SIPRI (1) · World Steel Association (1) · Institute for Economics and Peace (1) · INEC (1)
- **Egypt** (EGY, 89 indicateurs) : Central Bank of Egypt (22) · CAPMAS, Egypt (11) · Ministry of Planning and Economic Development (9) · Egyptian Tax Authority (7) · World Bank (6) · Ministry of Planning, Egypt (5) · Ministry of Finance, Egypt (4) · Central Agency for Public Mobilization and Statistics (3) · World Health Organization (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · CAPMAS - Central Agency for Public Mobilization And Statistics, Egypt (2) · Nielsen (1) · U.S. Energy Information Administration (1) · IMF (1) · CAPMAS (1) · Ministry of Petroleum (1) · World Gold Council (1) · S&P Global (1) · SIPRI (1) · Central Agency for Public Mobilization and Statistics (CAPMAS) (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **El Salvador** (SLV, 92 indicateurs) : Central Reserve Bank of El Salvador (37) · Oficina Nacional de Estadística y Censos (15) · World Bank (8) · World Health Organization (3) · Ministry of Finance (3) · Social Insurance Institute (3) · World Economic Forum (2) · Transparency International (2) · The Central Reserve Bank of El Salvador (2) · AKAMAI (2) · Digestyc, El Salvador (1) · Oficina Nacional de Estadística y CensosOficina Nacional de Estadística y Censos (1) · Consejo Monetario Centroamericanono (1) · Inter-American Development Bank (1) · SEMCA - Consejo Monetario Centroamericano (1) · Dirección Reguladora de Hidrocarburos y Minas (1) · World Gold Council (1) · Banco Central de Reserva de El Salvador (1) · Consejo Monetario Centroamericano (1) · SIPRI (1) · Ministerio de Trabajo y Previsión Social, El Salvador (1) · World Steel Association (1) · Institute for Economics and Peace (1) · Oficina Nacional de Estadística y Censos (ONEC), Banco Central de Reserva de El Salvador (1) · Banco Central de Reserva (1)
- **Equatorial Guinea** (GNQ, 35 indicateurs) : Bank of Central African States (10) · World Bank (8) · World Health Organization (3) · Ministry of Finance (3) · Ministry of Labor and Social Security (3) · Transparency International (2) · AKAMAI (2) · Organization of the Petroleum Exporting Countries (1) · International Monetary Fund (1) · Institute for Economics and Peace (1) · International Labour Organization (ILO) (1)
- **Estonia** (EST, 143 indicateurs) : Statistics Estonia (59) · EUROSTAT (27) · Bank of Estonia (13) · Estonian Tax and Customs Board (8) · OECD (7) · World Bank (6) · European Central Bank (4) · World Health Organization (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Estonian Institute of Economic Research (1) · European Commission (1) · Estonian Transport Administration (1) · European Automobile Manufacturers’ Association (ACEA) (1) · Olerex AS (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · Estonian Unemployment Insurance Fund (1) · SIPRI (1) · Institute for Economics and Peace (1)
- **Eswatini** (SWZ, 49 indicateurs) : Central Bank of Swaziland (23) · World Bank (7) · Central Bank of Swaiziland (3) · World Health Organization (3) · Ministry of Finance, Swaziland (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · SIPRI (1) · Swaziland Revenue Authority (1) · Institute for Economics and Peace (1) · International Labour Organization (ILO) (1)
- **Ethiopia** (ETH, 42 indicateurs) : National Bank of Ethiopia (10) · World Bank (9) · Ethiopian Statistical Service (ESS) (5) · World Health Organization (3) · Ethiopian Revenues and Customs Authority (3) · World Economic Forum (2) · Transparency International (2) · Central Statistical Agency of Ethiopia (2) · AKAMAI (2) · Ministry of Finance and Economic Development (1) · SIPRI (1) · Ministry of Labor & Skills, Ethiopia (1) · Institute for Economics and Peace (1)
- **Euro area** (EA, 129 indicateurs) : EUROSTAT (73) · European Central Bank (31) · European Commission (13) · S&P Global (4) · World Bank (3) · Bank for International Settlements (2) · World Gold Council (1) · OECD (1) · Centre for European Economic Research (ZEW) (1)
- **European Union** (EU, 71 indicateurs) : EUROSTAT (56) · European Commission (8) · World Bank (3) · European Automobile Manufacturers’ Association (ACEA) (2) · European Commssion (1) · World Steel Association (1)
- **FRO** (FRO, 18 indicateurs) : Statistics Faroe Islands (18)
- **Fiji** (FJI, 55 indicateurs) : Fiji Bureau of Statistics (21) · Reserve Bank of Fiji (10) · World Bank (9) · World Health Organization (3) · Fiji Revenue & Customs Service (3) · Fiji National Provident Fund (3) · Transparency International (2) · AKAMAI (2) · World Gold Council (1) · SIPRI (1)
- **Finland** (FIN, 161 indicateurs) : Statistics Finland (74) · EUROSTAT (21) · Bank of Finland (10) · OECD (9) · Finnish Tax Administration (7) · World Bank (6) · State Treasury, Finland (4) · World Health Organization (3) · European Central Bank (3) · World Economic Forum (2) · Transparency International (2) · Customs Finland (2) · Bank for International Settlements (2) · AKAMAI (2) · SIPRI (2) · Ministry of Social Affairs and Health (2) · Confederation of Finnish Industries (EK) (1) · European Commission (1) · European Automobile Manufacturers’ Association (ACEA) (1) · Europe's Energy Portal (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · European Banking Federation (1) · Ministry of Employment and the Economy, Finland (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **France** (FRA, 178 indicateurs) : INSEE, France (68) · EUROSTAT (19) · Banque de France (18) · OECD (11) · World Bank (6) · Ministère des Solidarités et de la Santé (5) · European Central Bank (4) · S&P Global (4) · Direction Générale des Finances Publiques (4) · Ministere de l'Économie, des Finances, de l'Action et des Comptes publics, France (4) · DARES, France (4) · Ministère de l'Économie et des Finances (3) · Ministère de la Transition écologique et solidaire (3) · World Health Organization (3) · CCFA - Comite des Constructeurs Francais d'Automobiles (2) · World Economic Forum (2) · Transparency International (2) · Bank for International Settlements (2) · AKAMAI (2) · SIPRI (2) · OICA - International Organization of Motor Vehicle Manufacturers (1) · U.S. Energy Information Administration (1) · European Commission (1) · European Automobile Manufacturers’ Association (ACEA) (1) · Europe's Energy Portal (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · World Steel Association (1) · Institute for Economics and Peace (1) · Centre for European Economic Research (ZEW) (1)
- **GRL** (GRL, 9 indicateurs) : World Health Organization (3) · World Bank (3) · AKAMAI (2) · Greenlandic Tax Agency (1)
- **GUM** (GUM, 5 indicateurs) : Guam Department of Revenue and Taxation (2) · AKAMAI (2) · World Bank (1)
- **Gabon** (GAB, 37 indicateurs) : World Bank (8) · Direction Générale de la Statistique, Gabon (4) · World Health Organization (3) · General Direction of Taxes (3) · Bank of Central African States (3) · Ministry of Labor and Social Insurance (3) · World Economic Forum (2) · Transparency International (2) · International Monetary Fund (2) · AKAMAI (2) · Organization of the Petroleum Exporting Countries (1) · Bank of Central African States (BEAC) (1) · SIPRI (1) · Institute for Economics and Peace (1) · International Labour Organization (ILO) (1)
- **Gambia** (GMB, 60 indicateurs) : Central Bank of The Gambia (24) · GBoS - Gambia Bureau of Statistics (14) · World Bank (5) · World Health Organization (3) · Gambia Revenue Authority (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · IMF (1) · SIPRI (1) · Gambia Bureau of Statistics (GBoS) (1) · Institute for Economics and Peace (1) · International Labour Organization (ILO) (1)
- **Georgia** (GEO, 94 indicateurs) : National Statistics Office of Georgia (48) · National Bank of Georgia (12) · World Bank (6) · World Health Organization (3) · Ministry of Finance (3) · Ministry of Finance of Georgia (3) · The ISET Policy Institute (2) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Social Service Agency (2) · IMF (1) · World Gold Council (1) · Ministry of Finance, Georgia (1) · United Nations Economic Commission for Europe (UNECE) (1) · OECD (1) · SIPRI (1) · Georgia Department of Revenue (1) · National Statistics of Georgia (1) · Institute for Economics and Peace (1)
- **Germany** (DEU, 189 indicateurs) : Federal Statistical Office (69) · Deutsche Bundesbank (26) · EUROSTAT (16) · OECD (10) · Bundeszentralamt für Steuern (9) · World Bank (6) · S&P Global (4) · Bundesagentur für Arbeit (4) · European Central Bank (3) · Ifo Institute (3) · World Health Organization (3) · VDA - German Association of the Automotive Industry (2) · European Commission (2) · KBA - Federal Motor Transport Authority, Germany (2) · World Economic Forum (2) · Transparency International (2) · Europace AG, Germany (2) · Bank for International Settlements (2) · AKAMAI (2) · SIPRI (2) · Centre for European Economic Research (ZEW) (2) · Statistisches Landesamt Baden-Württemberg (1) · Bayerisches Landesamt für Statistik (1) · Amt für Statistik Berlin-Brandenburg (1) · GfK Group (1) · U.S. Energy Information Administration (1) · European Automobile Manufacturers’ Association (ACEA) (1) · Europe's Energy Portal (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · Statistik Hessen (1) · European Banking Federation (1) · Landesbetrieb Information und Technik Nordrhein-Westfalen (1) · Statistisches Landesamt des Freistaates Sachsen (1) · World Steel Association (1) · Institute for Economics and Peace (1) · https://www.bundesbank.de/en (1)
- **Ghana** (GHA, 74 indicateurs) : Bank of Ghana (21) · Ghana Statistical Service (21) · World Bank (7) · World Health Organization (3) · Ghana Revenue Authority (3) · Ghana Statistical Services (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · S&P Global (1) · U.S. Energy Information Administration (1) · IMF (1) · GOIL Company Limited (GOIL) (1) · World Gold Council (1) · International Monetary Fund (IMF) (1) · SIPRI (1) · Ministry of Employment and Labour Relations, Ghana (1) · Institute for Economics and Peace (1) · International Labour Organization (ILO) (1)
- **Greece** (GRC, 146 indicateurs) : National Statistical Service of Greece (55) · Bank of Greece (20) · EUROSTAT (16) · OECD (6) · World Bank (6) · Hellenic Ministry of Labour, Social Insurance and Social Solidarity (5) · European Commission (4) · GSIS, Greece (4) · European Central Bank (3) · World Health Organization (3) · Hellenic Republic Ministry of Economy and Finance (3) · World Economic Forum (2) · Transparency International (2) · Bank for International Settlements (2) · AKAMAI (2) · SIPRI (2) · Hellenic Association of Motor Vehicle Importers-Representatives (1) · European Automobile Manufacturers’ Association (ACEA) (1) · Hellenic Ministry of Development, Competitiveness, Infrastructure, Transport and Networks (1) · World Gold Council (1) · Ministry of Economy and Finance (1) · United Nations Economic Commission for Europe (UNECE) (1) · S&P Global (1) · World Steel Association (1) · Institute for Economics and Peace (1) · Greek Tourism Confederation (1) · EFKA, Greece (1)
- **Grenada** (GRD, 27 indicateurs) : Eastern Caribbean Central Bank (12) · World Bank (6) · World Health Organization (3) · Transparency International (2) · AKAMAI (2) · Inland Revenue Department, Grenada (2)
- **Guatemala** (GTM, 84 indicateurs) : Banco de Guatemala (33) · Instituto Nacional de Estadística Guatemala (18) · World Bank (6) · World Health Organization (3) · Superintendence of the Tax Administration (3) · Instituto Guatemalteco de Seguridad Social (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Consejo Monetario Centroamericanono (1) · Inter-American Development Bank (1) · Ministerio de Energía y Minas, República de Guatemala (1) · World Gold Council (1) · Ministerio de Finanzas Públicas, Guatemala (1) · Consejo Monetario Centroamericano (1) · SIPRI (1) · Ministerio de Trabajo y Previsión Social, Guatemala (1) · Bank of Guatemala (1) · World Steel Association (1) · Institute for Economics and Peace (1) · Instituto Guatemalteco de Turismo, INGUAT (1)
- **Guinea** (GIN, 51 indicateurs) : Institut National de la Statistique de Guinée (9) · Central Bank of Republic of Guinea (8) · Institut National de la Statistique, Guinea (7) · World Bank (7) · World Health Organization (3) · National Social Security Fund (3) · World Economic Forum (2) · Transparency International (2) · Ministère du Budget (2) · AKAMAI (2) · Direction Générale des Impôts (2) · Ministry of Economy and Finance (1) · SIPRI (1) · Institute for Economics and Peace (1) · International Labour Organization (ILO) (1)
- **Guinea-Bissau** (GNB, 31 indicateurs) : Central Bank of West African States (BCEAO) (17) · World Bank (7) · Transparency International (2) · BCEAO (1) · Central Bank of West African States (1) · SIPRI (1) · Institute for Economics and Peace (1) · International Labor Organization (ILO) (1)
- **Guyana** (GUY, 39 indicateurs) : Bank of Guyana (13) · World Bank (7) · Statistics Guyana (5) · World Health Organization (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Inter-American Development Bank (1) · International Monetary Fund (1) · SIPRI (1) · Ministry of Finance (1) · Institute for Economics and Peace (1)
- **Haiti** (HTI, 37 indicateurs) : World Bank (7) · Banque de la République d'Haiti (6) · L'Institut Haïtien de Statistique et d'Informatique (IHSI) (5) · World Health Organization (3) · World Economic Forum (2) · Transparency International (2) · IMF (2) · AKAMAI (2) · Inter-American Development Bank (1) · Ministère de l'Economie et des Finances, Haiti (1) · Institut Haïtien de Statistique et d'Informatique (1) · World Gold Council (1) · Ministère des Affaires Sociales et du Travail, Haiti (1) · Ministry of the Economy & Finance (1) · Institute for Economics and Peace (1) · International Labour Organization (ILO) (1)
- **Honduras** (HND, 76 indicateurs) : Banco Central De Honduras (37) · Central Bank of Honduras (7) · World Bank (6) · Consejo Monetario Centroamericano (4) · World Health Organization (3) · Dirección Ejecutiva de Ingresos (3) · World Economic Forum (2) · Transparency International (2) · SECMCA - Consejo Monetario Centroamericano (2) · AKAMAI (2) · Inter-American Development Bank (1) · Secretaría de Energía, Honduras (1) · World Gold Council (1) · SEMCA - Consejo Monetario Centroamericano (1) · SIPRI (1) · Secretaría de Trabajo y Seguridad Social, Honduras (1) · Institute for Economics and Peace (1) · INE Honduras (1)
- **Hong Kong SAR China** (HKG, 101 indicateurs) : Census and Statistics Department, Hong Kong (59) · Hong Kong Monetary Authority (10) · World Bank (6) · World Health Organization (3) · World Economic Forum (2) · Inland Revenue Department, Hong Kong (2) · Transparency International (2) · Bank for International Settlements (2) · AKAMAI (2) · Civil Service Bureau (2) · City University of Hong Kong (1) · Consumer Council, Hong Kong (1) · World Gold Council (1) · GovHK (1) · International Monetary Fund (1) · Rating and Valuation Department Hong Kong Government (1) · Centaline Property Agency Ltd., Hong Kong (1) · Hong Kong Association of Banks (1) · IMF (1) · S&P Global (1) · Labour Department of the Government of the Hong Kong Special Administrative Region (1)
- **Hungary** (HUN, 145 indicateurs) : Hungarian Central Statistical Office (48) · EUROSTAT (26) · National Bank of Hungary (21) · National Tax and Customs Administration of Hungary (9) · OECD (7) · World Bank (6) · World Health Organization (3) · Ministry of Finance, Hungary (3) · GKI Economic Research Co (2) · World Economic Forum (2) · Transparency International (2) · European Automobile Manufacturers’ Association (ACEA) (2) · AKAMAI (2) · Central Bank of Hungary (1) · European Automobile Manufacturers Association (1) · U.S. Energy Information Administration (1) · Europe's Energy Portal (1) · World Gold Council (1) · Government Debt Management Agency Ltd, Hungary (1) · United Nations Economic Commission for Europe (UNECE) (1) · Bank for International Settlements (1) · HALPIM (1) · SIPRI (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **IMN** (IMN, 11 indicateurs) : World Health Organization (3) · World Bank (3) · Income Tax Services, Isle of Man Government (2) · AKAMAI (2) · Treasury (1)
- **Iceland** (ISL, 118 indicateurs) : Statistics Iceland (45) · Central Bank of Iceland (15) · EUROSTAT (13) · OECD (9) · World Bank (7) · Directorate of Internal Revenue (5) · World Health Organization (3) · Icelandic Ministry of Finance (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Ministry of Finance, Iceland (2) · Icelandic Transport Authority (1) · Gallup/Capacent (1) · European Automobile Manufacturers’ Association (ACEA) (1) · Iceland Automobile Association (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · Directorate of Labour, Iceland (1) · Internal Revenue Directorate (1) · Institute for Economics and Peace (1) · Icelandic Tourist Board (1)
- **India** (IND, 153 indicateurs) : Ministry of Statistics and Programme Implementation (MOSPI) (54) · Reserve Bank of India (27) · Office of the Economic Advisor, India (12) · Ministry of Finance, Government of India (10) · World Bank (6) · Ministry of Commerce and Industry, India (3) · SIAM - Society of Indian Automobile Manufacturers (3) · OECD (3) · S&P Global (3) · World Health Organization (3) · Controller General Of Accounts, India (3) · World Economic Forum (2) · Transparency International (2) · Bank for International Settlements (2) · National Housing Bank, India (2) · AKAMAI (2) · SIPRI (2) · MNI Deutsche Börse Group (2) · Office of the Economic Adviser, India (1) · U.S. Energy Information Administration (1) · Central Electricity Authority, Ministry of Power, Government of India (1) · Indian Oil Corporation Ltd (1) · World Gold Council (1) · Ministry of Labour & Employment (1) · Retailers Association of India (RAI) (1) · World Steel Association (1) · Institute for Economics and Peace (1) · Department of Tourism, India (1) · Ministry of Labour and Employment, India (1) · Labour Bureau, Government of India (1)
- **Indonesia** (IDN, 114 indicateurs) : Statistics Indonesia (43) · Bank Indonesia (27) · Direktorat Jenderal Pajak (8) · World Bank (6) · Ministry of Finance of Republic of Indonesia (4) · OECD (3) · World Health Organization (3) · PT Astra International TBK (2) · World Economic Forum (2) · Transparency International (2) · Investment Coordinating Board of the Republic of Indonesia (BKPM) (2) · AKAMAI (2) · OICA - International Organization of Motor Vehicle Manufacturers (1) · U.S. Energy Information Administration (1) · Ministry of Energy and Mineral Resources, Republic of Indonesia (1) · World Gold Council (1) · IMF (1) · Bank for International Settlements (1) · S&P Global (1) · SIPRI (1) · Ministry of Manpower and Transmigration (1) · Institute for Economics and Peace (1)
- **Iran** (IRN, 67 indicateurs) : Statistical Center of Iran (21) · Central Bank of Iran (18) · World Bank (7) · World Health Organization (3) · International Monetary Fund (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Iran Vehicle Manufacturers Association (1) · Organization of the Petroleum Exporting Countries (1) · OPEC (1) · Statistical Centre of Iran (1) · National Iranian Oil Products Distribution Company (1) · SIPRI (1) · Ministry of Economic Affairs and Finance (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **Iraq** (IRQ, 55 indicateurs) : Central Bank of Iraq (18) · World Bank (8) · Central Statistical Organization Iraq (COSIT) (6) · World Health Organization (3) · IMF (3) · Ministry of Labor and Social Affairs (3) · Transparency International (2) · Central Statistical Organization Iraq (2) · AKAMAI (2) · General Commission for Taxes (1) · Organization of the Petroleum Exporting Countries (1) · Baker Hughes Company (1) · World Gold Council (1) · SIPRI (1) · Ministry of Finance (1) · Institute for Economics and Peace (1) · International Labor Organization (ILO) (1)
- **Ireland** (IRL, 158 indicateurs) : Central Statistics Office Ireland (84) · EUROSTAT (15) · Central Bank of Ireland (10) · OECD (9) · The Office of the Revenue Commissioners (9) · World Bank (6) · S&P Global (3) · World Health Organization (3) · European Central Bank (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · European Commission (1) · Irish League of Credit Unions (1) · European Automobile Manufacturers’ Association (ACEA) (1) · AA Ireland (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · Bank for International Settlements (1) · SIPRI (1) · Department of Employment Affairs and Social Protection (1) · Institute for Economics and Peace (1)
- **Israel** (ISR, 132 indicateurs) : Central Bureau of Statistics, Israel (66) · Bank of Israel (21) · OECD (9) · World Bank (6) · Ministry of Social Affairs and Social Services (5) · World Health Organization (3) · Israel Ministry of Finance Tax Authority (3) · Israel Ministry Of Finance (3) · World Economic Forum (2) · Transparency International (2) · Bank for International Settlements (2) · AKAMAI (2) · SIPRI (2) · U.S. Energy Information Administration (1) · Ministry of National Infrastructures, Energy and Water Resources (1) · United Nations Economic Commission for Europe (UNECE) (1) · Bank Hapoalim/ILPMA (1) · National Insurance Institute of Israel (1) · Institute for Economics and Peace (1)
- **Italy** (ITA, 170 indicateurs) : National Institute of Statistics (ISTAT) (76) · Banca d'Italia (18) · EUROSTAT (16) · OECD (12) · World Bank (6) · Ministero del Lavoro e delle Politiche Sociali (5) · S&P Global (4) · Agenzia delle Entrate, Ministero dell'Economia e delle finanze (4) · European Central Bank (3) · World Health Organization (3) · European Commission (2) · ANFIA - Italian Association of the Automotive Industry (2) · World Economic Forum (2) · Transparency International (2) · Bank for International Settlements (2) · AKAMAI (2) · SIPRI (2) · U.S. Energy Information Administration (1) · European Automobile Manufacturers’ Association (ACEA) (1) · Terna - Rete Elettrica Nazionale, Italy (1) · Europe's Energy Portal (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · World Steel Association (1) · Institute for Economics and Peace (1) · Centre for European Economic Research (ZEW) (1)
- **Jamaica** (JAM, 74 indicateurs) : Statistical Institute of Jamaica (29) · Bank of Jamaica (18) · World Bank (7) · World Health Organization (3) · Tax Admministration of Jamaica (3) · Ministry of Labour and Social Security (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Inter-American Development Bank (1) · Petrojam Limited (1) · Ministry of Finance & the Public Service (1) · SIPRI (1) · Institute for Economics and Peace (1)
- **Japan** (JPN, 201 indicateurs) : Bank of Japan (30) · Cabinet Office, Japan (30) · Statistics Bureau of Japan (25) · Ministry of Finance, Japan (21) · Ministry of Internal Affairs & Communications (13) · Ministry of Health, Labour and Welfare, Japan (13) · Ministry of Economy Trade & Industry (METI) (10) · OECD (10) · National Tax Agency (7) · World Bank (6) · Ministry of Land, Infrastructure, Transport and Tourism, Japan (4) · S&P Global (3) · World Health Organization (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · SIPRI (2) · United Nations Refugee Agency (1) · Tokyo Shoko Research (1) · Japan Automobile Manufacturers Association (JAMA) (1) · Japan Automobile Dealers Association (1) · Japan Cement Association (JCA) (1) · Agency for Natural Resources and Energy (1) · World Gold Council (1) · Bank for International Settlements (1) · Japan Real Estate Institute (JREI) (1) · Japanese Bankers Association (1) · Association of Job Information of Japan (1) · EUROSTAT (1) · Japan Machine Tool Builders' Association (JMTBA) (1) · Japan Productivity Center for Socio-Economic Development (1) · Tankan Sponsored by Thomson Reuters (1) · World Steel Association (1) · Institute for Economics and Peace (1) · Japan National Tourist Organization (1)
- **Jordan** (JOR, 99 indicateurs) : Central Bank of Jordan (28) · Department Of Statistics (Jordan) (19) · Department of Statistics (DOS), Jordan (15) · World Bank (7) · Ministry of Finance, Jordan (4) · The Hashemite Kingdom of Jordan Department of Statistics (DOS) (3) · World Health Organization (3) · Social Security Corporation (3) · World Economic Forum (2) · SESRIC (2) · Jordan Tax Service (2) · Transparency International (2) · AKAMAI (2) · SIPRI (2) · The Hashemite Kingdom of Jordan Department of Statistics (1) · World Gold Council (1) · Ministry of Labour, Jordan (1) · National Information System (1) · Institute for Economics and Peace (1)
- **Kazakhstan** (KAZ, 110 indicateurs) : Agency of Statistics of the Republic of Kazakhstan (40) · The National Bank of Kazakhstan (14) · Agency of the Republic of Kazakhstan on Statistics (10) · World Bank (6) · National Bank of Kazakhstan (5) · S&P Global (3) · World Health Organization (3) · Tax Committee of the Ministry of Finance of the Republic of Kazakhstan (3) · Ministry of Finance of the Republic of Kazakhstan (3) · Tax Committee of the Ministry of Finance (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Ministry of Labor and Social Protection of Population of the Republic of Kazakhstan (2) · Nielsen (1) · U.S. Energy Information Administration (1) · World Gold Council (1) · Ministry of Finance of Republic of Kazakhstan (1) · IMF (1) · United Nations Economic Commission for Europe (UNECE) (1) · OECD (1) · SIPRI (1) · Tax Committee Ministry of Finance of the Republic of Kazakhstan (1) · Bureau of National Statistics of the Republic of Kazakhstan (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **Kenya** (KEN, 87 indicateurs) : Kenya National Bureau of Statistics (35) · Central Bank of Kenya (22) · World Bank (7) · Kenya Revenue Authority (4) · World Health Organization (3) · National Social Security Fund, Kenya (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Energy and Petroleum Regulatory Authority (1) · World Gold Council (1) · S&P Global (1) · SIPRI (1) · MNI Deutsche Börse Group (1) · Institute for Economics and Peace (1) · International Labour Organization (ILO) (1)
- **Kiribati** (KIR, 10 indicateurs) : World Bank (6) · Transparency International (2) · AKAMAI (2)
- **Kosovo** (KOS, 81 indicateurs) : Kosovo Agency of Statistics (53) · Central Bank of the Republic of Kosovo (11) · Ministry of Finance, Republic of Kosovo (5) · World Bank (4) · Ministry Of Labour and Social Welfare (3) · Transparency International (2) · EUROSTAT (1) · SIPRI (1) · Institute for Economics and Peace (1)
- **Kuwait** (KWT, 74 indicateurs) : Central Bank of Kuwait (26) · Kuwait Central Statistical Bureau (15) · World Bank (8) · World Health Organization (3) · International Monetary Fund (3) · Department of Income Tax (3) · World Economic Forum (2) · Ministry of Finance, Kuwait (2) · Transparency International (2) · AKAMAI (2) · Organization of the Petroleum Exporting Countries (1) · Baker Hughes Company (1) · Kuwait National Petroleum Company (1) · World Gold Council (1) · SIPRI (1) · Central Statistical Bureau (Kuwait) (1) · Institute for Economics and Peace (1) · SESRIC (1)
- **Kyrgyzstan** (KGZ, 73 indicateurs) : National Statistical Committee of Kyrgyz Republic (30) · National Bank of the Kyrgyz Republic (19) · World Bank (7) · World Health Organization (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · World Gold Council (1) · Ministry of Finance of the Kyrgyz Republic (1) · The ministry of economy of the Kyrgyz Republic (1) · United Nations Economic Commission for Europe (UNECE) (1) · OECD (1) · SIPRI (1) · Ministry of Labour and Social Development (1) · State tax Service (1)
- **LBN** (LBN, 64 indicateurs) : Banque Du Liban (18) · World Bank (9) · Central Administration of Statistics, Republic of Lebanon (7) · Central Administration for Statistics, Lebanon (4) · Ministry Of Finance, Republic of Lebanon (4) · World Health Organization (3) · Ministry of Labor (3) · World Economic Forum (2) · Ministry of Finance (2) · Transparency International (2) · International Monetary Fund (2) · AKAMAI (2) · Electricite du Liban (1) · World Gold Council (1) · S&P Global (1) · SIPRI (1) · Ministry of Finance, Lebanon (1) · Institute for Economics and Peace (1)
- **Laos** (LAO, 63 indicateurs) : Bank of the Lao PDR (23) · Lao Statistical Bureau (15) · World Bank (8) · Ministry of Finance (3) · Ministry of Labor and Social Welfare (3) · World Economic Forum (2) · Transparency International (2) · Lao Statistics Bureau (2) · AKAMAI (2) · SIPRI (1) · Ministry of Labor and Social Welfare, Laos (1) · Institute for Economics and Peace (1)
- **Latvia** (LVA, 144 indicateurs) : Central Statistical Bureau of Latvia (55) · EUROSTAT (28) · Bank of Latvia (10) · OECD (8) · World Bank (6) · European Central Bank (4) · Latvijas Banka (3) · World Health Organization (3) · State Revenue Service, Latvia (3) · Treasury of the Republic of Latvia (3) · State Revenue Service - Ministry of Finance (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · State Social Insurance Agency, Latvia (2) · European Commission (1) · ACEA - European Automobile Manufacturers' Association (1) · European Automobile Manufacturers’ Association (ACEA) (1) · Statoil Fuel & Retail (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · SIPRI (1) · Bank for International Settlements (1) · Institute for Economics and Peace (1) · State Employment Agency, Latvia (1)
- **Lesotho** (LSO, 55 indicateurs) : Lesotho Bureau of Statistics (23) · Central Bank of Lesotho (13) · World Bank (6) · Lesotho Revenue Authority (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · SIPRI (1) · Ministry of Labour and Employment, Lesotho (1) · Institute for Economics and Peace (1) · International Labor Organization (ILO) (1)
- **Liberia** (LBR, 53 indicateurs) : Central Bank of Liberia (19) · Liberia Institute of Statistics and Geo-Information Services (12) · World Bank (8) · World Health Organization (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · IMF (1) · Central Bank Liberia (1) · SIPRI (1) · Institute for Economics and Peace (1) · Liberia Revenue Authority (1)
- **Libya** (LBY, 49 indicateurs) : Central Bank of Libya (11) · Central Bank of Lybia (9) · World Bank (7) · Bureau of Statistics and Census Libya (6) · Social Security Fund (3) · World Economic Forum (2) · Transparency International (2) · Libyan Tax Authority (1) · Organization of the Petroleum Exporting Countries (1) · Baker Hughes Company (1) · World Gold Council (1) · SIPRI (1) · Ministry of Finance (1) · World Steel Association (1) · Institute for Economics and Peace (1) · International Labour Organization (ILO) (1)
- **Liechtenstein** (LIE, 22 indicateurs) : National Administration of the Principality of Liechtenstein (8) · World Health Organization (3) · Ministry of Finance (3) · World Bank (3) · AKAMAI (2) · Ministry of Social Affairs, Liechtenstein (2) · EUROSTAT (1)
- **Lithuania** (LTU, 139 indicateurs) : Statistics Lithuania (53) · EUROSTAT (27) · Bank of Lithuania (18) · OECD (9) · World Bank (5) · World Health Organization (3) · State Tax Inspectorate (3) · State Tax Inspectorate (STI) (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · State Social Insurance Fund Board (2) · European Commission (1) · ACEA - European Automobile Manufacturers' Association (1) · European Automobile Manufacturers’ Association (ACEA) (1) · European Central Bank (1) · Circle K, Lithuania (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · SIPRI (1) · Institute for Economics and Peace (1) · Lithuanian Labour Exchange (1)
- **Luxembourg** (LUX, 133 indicateurs) : STATEC, Luxembourg (39) · EUROSTAT (29) · Banque Centrale Du Luxembourg (17) · OECD (10) · Administration des Contributions Directes (6) · European Commission (5) · World Bank (5) · World Health Organization (3) · World Economic Forum (2) · Transparency International (2) · European Automobile Manufacturers’ Association (ACEA) (2) · European Central Bank (2) · AKAMAI (2) · Caisse Nationale d’Assurance Pension (2) · Shell Luxembourg (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · Bank for International Settlements (1) · Agence pour le développement de l'emploi (ADEM) (1) · SIPRI (1) · World Steel Association (1)
- **MCO** (MCO, 18 indicateurs) : Monaco Statistics (6) · World Bank (4) · World Health Organization (3) · AKAMAI (2) · Department of Tax Services, Monaco (2) · OECD (1)
- **MNP** (MNP, 4 indicateurs) : AKAMAI (2) · World Bank (2)
- **MYT** (MYT, 4 indicateurs) : AKAMAI (2) · World Bank (2)
- **Macao SAR China** (MAC, 64 indicateurs) : Statistics and Census Service, Government of Macao SAR (39) · Monetary Authority of Macao (7) · World Bank (6) · Monetary Authority Macao (3) · World Health Organization (3) · Transparency International (2) · Monentary Authority of Macao (2) · Financial Service Bureau (1) · Secretariat for Economy and Finance (1)
- **Madagascar** (MDG, 45 indicateurs) : Institut National de la Statistique de Madagascar (10) · Central Bank of Madagascar (7) · World Bank (6) · World Health Organization (3) · General Direction of Taxes (3) · National Social Insurance Fund (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · IMF (1) · Direction Générale du Tresor (1) · Insitut National de la Statistique (INSTAT) Madagascar (1) · SIPRI (1) · Ministère du Travail, de l'Emploi, de la Fonction Publique et des Lois Sociales, Madagascar (1) · Institute for Economics and Peace (1) · International Labour Organization (ILO) (1)
- **Malawi** (MWI, 65 indicateurs) : National Statistical Office of Malawi (24) · Reserve Bank of Malawi (20) · World Bank (7) · World Economic Forum (2) · Ministry of Finance (2) · Transparency International (2) · AKAMAI (2) · National Statistics Office of Malawi (1) · IMF (1) · SIPRI (1) · Malawi Revenue Authority (1) · Institute for Economics and Peace (1) · International Labour Organization (ILO) (1)
- **Malaysia** (MYS, 118 indicateurs) : Department of Statistics, Malaysia (55) · Central Bank of Malaysia (19) · World Bank (7) · Inland Revenue Board of Malaysia (5) · World Health Organization (3) · Social Security Organisation (SOCSO) (3) · Malaysian Automotive Association (2) · Road Transport Department Malaysia (2) · World Economic Forum (2) · Transparency International (2) · Ministry of Finance Malaysia (2) · National Property Information Center, Malaysia (2) · AKAMAI (2) · Tourism Malaysia (2) · Oppotus Research (1) · U.S. Energy Information Administration (1) · Ministry of Domestic Trade, Cooperatives and Consumerism (1) · World Gold Council (1) · Bank for International Settlements (1) · Maybank Group (1) · S&P Global (1) · SIPRI (1) · Ministry of Human Resources Malaysia (1) · Institute for Economics and Peace (1)
- **Maldives** (MDV, 58 indicateurs) : Maldives Monetary Authority (30) · Maldives Bureau of Statistics (10) · World Bank (7) · World Health Organization (3) · National Bureau of Statistics, Maldives (2) · Transparency International (2) · AKAMAI (2) · Inland Revenue Authority (1) · International Labor Organization (ILO) (1)
- **Mali** (MLI, 40 indicateurs) : Institut National de la Statistique, Mali (12) · Central Bank of West African States (BCEAO) (8) · World Bank (8) · World Economic Forum (2) · Transparency International (2) · African Development Bank Group (2) · AKAMAI (2) · SIPRI (1) · Ministry of Economy and Finance (1) · Institute for Economics and Peace (1) · International Labour Organization (ILO) (1)
- **Malta** (MLT, 126 indicateurs) : National Statistics Office, Malta (45) · EUROSTAT (26) · Central Bank of Malta (15) · Government of Malta - Inland Revenue (6) · World Bank (6) · European Central Bank (4) · European Commission (3) · World Health Organization (3) · Commisioner of Inland Revenue (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · National Statistics Office (1) · European Automobile Manufacturers’ Association (ACEA) (1) · Malta National Statistics Office (1) · Enemed, Malta (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · OECD (1) · SIPRI (1) · Bank for International Settlements (1)
- **Marshall Islands** (MHL, 7 indicateurs) : World Bank (5) · AKAMAI (2)
- **Mauritania** (MRT, 47 indicateurs) : ANSADE, Mauritania (14) · World Bank (7) · Central Bank of Mauritania (5) · World Health Organization (3) · Ministry of Finance (3) · National Social Security Fund (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · IMF (1) · Banque Centrale de Mauritanie (1) · Central Bank of Mauritanita (1) · SIPRI (1) · Institute for Economics and Peace (1) · International Labour Organization (ILO) (1)
- **Mauritius** (MUS, 106 indicateurs) : Central Statistics Office, Mauritius (61) · Bank of Mauritius (15) · World Bank (7) · Mauritius Revenue Authority (4) · World Health Organization (3) · Ministry of Social Security, National Solidarity, and Reform Institutions (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Ministry of Labour, Human Resource Development and Training (2) · The Mauritius Chamber of Commerce and Industry (1) · World Gold Council (1) · Statistics Mauritius (1) · SIPRI (1) · Institute for Economics and Peace (1)
- **Mexico** (MEX, 149 indicateurs) : Instituto Nacional de Estadística y Geografía (INEGI) (66) · Banco de México (23) · World Bank (8) · OECD (6) · Mexican Social Security Institute (5) · World Health Organization (3) · Servicio de Administración Tributaria, SHCP (3) · Secretaría de Hacienda Y Crédito Público (3) · Instituto Nacional de Estadistica, Geografia e Informatica, Mexico (3) · Banco de Mexico (3) · World Economic Forum (2) · Transparency International (2) · Secretaria de Hacienda y Credito Publico (2) · Bank for International Settlements (2) · Bank of Mexico (2) · AKAMAI (2) · U.S. Energy Information Administration (1) · Baker Hughes Company (1) · IMF (1) · Inter-American Development Bank (1) · PETROIntelligence (1) · INEGI, Mexico (1) · World Gold Council (1) · Gobierno de Mexico (1) · S&P Global (1) · SIPRI (1) · INEGI - Instituto Nacional de Estadistica, Geografia e Informatica, Mexico (1) · World Steel Association (1) · Institute for Economics and Peace (1) · Secretariat of Labor & Soc Welf, Mexico (1)
- **Micronesia** (FSM, 14 indicateurs) : World Bank (7) · FSM Social Security Administration (5) · Department of Finance & Administration (DOFA) (1) · Department of Finance and Administration (1)
- **Moldova** (MDA, 80 indicateurs) : National Bureau of Statistics of the Republic of Moldova (34) · National Bank of Moldova (16) · World Bank (6) · World Health Organization (3) · State tax Service (3) · National Office of Social Insurance (3) · World Economic Forum (2) · Transparency International (2) · Ministry of Finance of Moldova (2) · Ministry of Finance of Republic of Moldova (2) · Statistica Moldovei (1) · Finance Ministry of the Republic of Moldova (1) · United Nations Economic Commission for Europe (UNECE) (1) · SIPRI (1) · Ministry of Labour and Social Protection, Moldova (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **Mongolia** (MNG, 98 indicateurs) : National Statistical Office of Mongolia (46) · Bank of Mongolia (13) · Mongolbank (10) · World Bank (6) · World Health Organization (3) · General Department of National Taxation (3) · Ministry of Labour and Social Protection (3) · Social Insurance General Office (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · U.S. Energy Information Administration (1) · International Monetary Fund (1) · World Gold Council (1) · SIPRI (1) · Institute for Economics and Peace (1)
- **Montenegro** (MNE, 84 indicateurs) : Statistical Office of Montenegro (36) · Central Bank of Montenegro (8) · World Bank (6) · EUROSTAT (5) · World Health Organization (3) · Department of Public Revenues, Montenegro (3) · Ministry Of Finance, Montenegro (3) · Ministry of Finance, Republic of Montenegro (3) · Ministry Of Labour and Social Welfare (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Employment Agency of Montenegro (2) · Petrol (1) · United Nations Economic Commission for Europe (UNECE) (1) · OECD (1) · SIPRI (1) · Institute for Economics and Peace (1) · Statistical Office of Montenegro (MONSTAT) (1)
- **Morocco** (MAR, 82 indicateurs) : Haut Commissariat au Plan (37) · Bank Al-Maghrib (8) · World Bank (7) · Office des Changes (3) · Office des Changes, Marocco (3) · World Health Organization (3) · National Social Security Fund (3) · Central Bank of Morocco (2) · World Economic Forum (2) · Department of Taxes (2) · Transparency International (2) · AKAMAI (2) · World Gold Council (1) · SIPRI (1) · Ministry of Economic Inclusion, Small Business, Employment and Skills, Morocco (1) · Direction Générale des Impôts, Morocco (1) · World Steel Association (1) · Institute for Economics and Peace (1) · Ministry of Tourism, Morocco (1) · Haut Commissariat au Plan, Morocco (1)
- **Mozambique** (MOZ, 74 indicateurs) : Instituto Nacional De Estatistica, Mozambique (36) · Banco de Moçambique (11) · World Bank (6) · World Health Organization (3) · Autoridade Tributária de Moçambique (3) · National Institute of Social Security (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · S&P Global (1) · IMF (1) · World Gold Council (1) · SIPRI (1) · Ministry of Labor, Employment, and Social Security (1) · Institute for Economics and Peace (1)
- **Myanmar (Burma)** (MMR, 39 indicateurs) : Central Statistics Organization, Myanmar (8) · World Bank (8) · World Health Organization (3) · Internal Revenue Department (3) · Central Bank of Myanmar (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Myanmar Statistical Information Service - MMSIS (2) · Ministry of National Planning and Economic Development, Myanmar (1) · Directorate of Investment and Company Administration, Myanmar (1) · S&P Global (1) · SIPRI (1) · National Minimum Wage Committee of Myanmar (1) · Institute for Economics and Peace (1)
- **NCL** (NCL, 34 indicateurs) : ISEE, New Caledonia (21) · World Bank (6) · World Health Organization (3) · Institute of Overseas Issuance (2) · AKAMAI (2)
- **Namibia** (NAM, 84 indicateurs) : Central Bureau of Statistics, Namibia (41) · Bank of Namibia (18) · World Bank (7) · Namibia Revenue Agency (4) · World Health Organization (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · International Monetary Fund (1) · Namibia Statistics Agency (1) · SIPRI (1) · Institute for Economics and Peace (1) · Ministry of Environment and Tourism, Namibia (1)
- **Nepal** (NPL, 52 indicateurs) : Nepal Rastra Bank (15) · Central Bureau of Statistics, Nepal (14) · World Bank (7) · World Health Organization (3) · World Economic Forum (2) · Nepal Central Bureau of Statistics (2) · Transparency International (2) · AKAMAI (2) · World Gold Council (1) · Nepal Central Bank (1) · SIPRI (1) · Inland Revenue Department (1) · Institute for Economics and Peace (1)
- **Netherlands** (NLD, 164 indicateurs) : Statistics Netherlands (70) · EUROSTAT (21) · De Nederlandsche Bank (17) · OECD (9) · World Bank (5) · Tax and Customs Administration (5) · Tax and Customs Administration - Belastingdienst (4) · European Central Bank (4) · World Health Organization (3) · Statistics Netherlands - CBS (3) · European Commission (2) · World Economic Forum (2) · Transparency International (2) · European Automobile Manufacturers’ Association (ACEA) (2) · Bank for International Settlements (2) · AKAMAI (2) · SIPRI (2) · European Automobile Manufacturers Association (1) · U.S. Energy Information Administration (1) · Europe's Energy Portal (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · S&P Global (1) · World Steel Association (1) · Institute for Economics and Peace (1) · CPB Netherlands Bureau for Economic Policy Analysis (1)
- **New Zealand** (NZL, 151 indicateurs) : Statistics New Zealand (76) · Reserve Bank of New Zealand (16) · New Zealand Inland Revenue Department (8) · OECD (6) · New Zealand Treasury (6) · World Bank (5) · Quotable Value (4) · Business New Zealand (3) · World Health Organization (3) · ANZ Bank New Zealand (2) · Australia and New Zealand Banking Group (2) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · SIPRI (2) · Immigration New Zealand (1) · New Zealand Insolvency and Trustee Service (1) · New Zealand Institute of Economic Research (1) · Westpac Banking Corporation, McDermott Miller (1) · Statistics NZ (1) · Ministry of Business, Innovation & Employment (1) · GlobalDairyTrade (1) · Bank for International Settlements (1) · MBIE, New Zealand (1) · Ministry of Business, Innovation and Employment (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **Nicaragua** (NIC, 78 indicateurs) : Banco Central de Nicaragua (27) · INIDE: Instituto Nacional de Información de Desarrollo (16) · World Bank (6) · Ministerio de Hacienda y Crédito Público (5) · Consejo Monetario Centroamericanono (4) · World Health Organization (3) · General Directorate of Revenues (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Inter-American Development Bank (1) · SEMCA - Consejo Monetario Centroamericano (1) · INE - Instituto Nicaragüense de Energía (1) · SIPRI (1) · Ministerio del Trabajo Nicaragua (1) · Instituto Nacional de Información de Desarrrolo (1) · Institute for Economics and Peace (1) · Instituto Nacional de Información de Desarollo - INIDE (1)
- **Niger** (NER, 44 indicateurs) : Institut National de la Statistique du Niger (13) · Central Bank of West African States (BCEAO) (9) · World Bank (7) · World Health Organization (3) · Transparency International (2) · AKAMAI (2) · U.S. Energy Information Administration (1) · Banque Centrale des Etats de l’Afrique de l’Ouest (BCEAO) (1) · African Development Bank Group (1) · BCEAO (1) · SIPRI (1) · Ministry of Finance (1) · Institute for Economics and Peace (1) · ILO (1)
- **Nigeria** (NGA, 105 indicateurs) : National Bureau of Statistics, Nigeria (46) · Central Bank of Nigeria (28) · World Bank (6) · Federal Inland Revenue Service, Nigeria (4) · World Health Organization (3) · World Economic Forum (2) · Transparency International (2) · IMF (2) · Central Bank of Nigeria (CBN) (2) · AKAMAI (2) · S&P Global (1) · Organization of the Petroleum Exporting Countries (1) · Baker Hughes Company (1) · Petroleum Products Pricing Regulatory Agency (1) · Debt Management Office, Nigeria (1) · SIPRI (1) · MNI Deutsche Börse Group (1) · Institute for Economics and Peace (1)
- **North Macedonia** (MKD, 98 indicateurs) : State Statistical Office of the Republic of Macedonia (42) · National Bank of the Republic of Macedonia (16) · EUROSTAT (6) · World Bank (5) · Public Revenue Office (4) · World Health Organization (3) · Ministry of Finance of the Republic of Macedonia (3) · World Economic Forum (2) · Transparency International (2) · Ministry of Finance, Republic of Macedonia (2) · AKAMAI (2) · Ministry of Finance (1) · Energy Regulatory Commission of the Republic of Macedonia (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · OECD (1) · National Bank of the Republic of North Macedonia (1) · SIPRI (1) · Ministry of Labor and Social Policy, Macedonia (1) · Bank for International Settlements (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **Norway** (NOR, 164 indicateurs) : Statistics Norway (92) · EUROSTAT (15) · OECD (8) · Norwegian Tax Administration (8) · Norges Bank (7) · World Bank (6) · Eiendom Norge/Real Estate Norway (4) · World Health Organization (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · SIPRI (2) · Real Estate Norway (1) · Finans Norge & Verian (1) · U.S. Energy Information Administration (1) · Baker Hughes Company (1) · European Automobile Manufacturers’ Association (ACEA) (1) · United Nations Economic Commission for Europe (UNECE) (1) · Bank for International Settlements (1) · NIMA/DNB Markets (1) · NAV - Norwegian Labour and Welfare Organisation (1) · Ministry of Finance (1) · World Steel Association (1) · Institute for Economics and Peace (1) · Norwegian Labour and Welfare Organisation (1)
- **Oman** (OMN, 83 indicateurs) : National Centre for Statistic & Information, Oman (27) · Central Bank of Oman (15) · National Center for Statistics & Information - Oman (9) · World Bank (6) · Ministry of National Economy, Oman (3) · World Health Organization (3) · Ministry of Finance, Oman (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · U.S. Energy Information Administration (1) · International Monetary Fund (1) · Ministry of Oil and Gas, Oman (1) · World Gold Council (1) · SIPRI (1) · Ministry of Labour, Oman (1) · National Centre for Statistics and Information (Oman) (1) · Economic and Financial Committee (1) · Institute for Economics and Peace (1) · Oman National Center for Statistics and Information (1) · International Labor Organization (ILO) (1)
- **PRK** (PRK, 34 indicateurs) : The Bank of Korea (8) · Bank of Korea (5) · World Bank (5) · International Trade Centre (3) · International Labour Organization (ILO) (3) · Transparency International (2) · Daily NK (2) · AKAMAI (2) · North Korean Economy Watch (1) · North Korea Statistics (1) · Ministry of Finance (1) · Institute for Economics and Peace (1)
- **PSE** (PSE, 61 indicateurs) : Palestinian Central Bureau of Statistics (43) · World Bank (6) · World Health Organization (3) · Palestine Monetary Authority (3) · Transparency International (2) · AKAMAI (2) · Palestine Monetary Authority (PMA) (1) · Institute for Economics and Peace (1)
- **PYF** (PYF, 4 indicateurs) : AKAMAI (2) · World Bank (2)
- **Pakistan** (PAK, 96 indicateurs) : Pakistan Bureau of Statistics (40) · State Bank of Pakistan (25) · World Bank (8) · World Health Organization (3) · Federal Board of Revenue, Government of Pakistan (3) · Central Bank of Pakistan (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · U.S. Energy Information Administration (1) · IMF (1) · Pakistan State Oil (1) · World Gold Council (1) · Government of Pakistan Ministry of Finance (1) · SIPRI (1) · Ministry of Overseas Pakistanis and Human Resource Development (1) · Institute for Economics and Peace (1)
- **Palau** (PLW, 8 indicateurs) : World Bank (6) · AKAMAI (2)
- **Panama** (PAN, 58 indicateurs) : Instituto Nacional de Estadística y Censo - Panamá (25) · World Bank (8) · World Health Organization (3) · General Directorate of Revenues, Panama (3) · Ministry of Labour and Social Security (3) · INEC, Panama (2) · World Economic Forum (2) · Transparency International (2) · IMF (2) · Ministry of Finance, Panama (2) · AKAMAI (2) · Inter-American Development Bank (1) · Secretaría de Energía de Panamá (1) · Ministerio de Trabajo y Desarrollo Laboral (1) · Institute for Economics and Peace (1)
- **Papua New Guinea** (PNG, 44 indicateurs) : Bank of Papua New Guinea (13) · World Bank (9) · Internal Revenue Commission, Papua New Guinea (4) · World Health Organization (3) · Internal Revenue Commission (3) · Transparency International (2) · AKAMAI (2) · National Statistical Office of Papua New Guinea (1) · U.S. Energy Information Administration (1) · Department of Treasury (1) · World Gold Council (1) · Department of Treasury, Papua New Guinea (1) · SIPRI (1) · Institute for Economics and Peace (1) · Papua New Guinea National Statistics Office (1)
- **Paraguay** (PRY, 85 indicateurs) : Banco Central del Paraguay (49) · World Bank (8) · World Health Organization (3) · Dirección General de Administración y Finanzas (3) · Social Insurance Institute (3) · World Economic Forum (2) · Transparency International (2) · Ministerio de Hacienda, Paraguay (2) · AKAMAI (2) · Instituto Nacional de Estadística Paraguay (2) · DGEEC (1) · Inter-American Development Bank (1) · Petropar (1) · World Gold Council (1) · Ministerio de Economía y Finanzas, Paraguay (1) · SIPRI (1) · Dirección General de Estadística, Encuestas y Censos (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **Peru** (PER, 101 indicateurs) : Central Reserve Bank of Peru (36) · Instituto Nacional de Estadística e Informática (INEI) (33) · World Bank (6) · Superintendencia Nacional de Aduanas y de Administración Tributaria (4) · World Health Organization (3) · World Economic Forum (2) · Transparency International (2) · Ministry of Economy and Finance of Peru (2) · AKAMAI (2) · Ministry of Production of Peru (1) · Ipsos (1) · GFK (1) · U.S. Energy Information Administration (1) · Inter-American Development Bank (1) · Organismo Supervisor de la Inversión en Energía y Minería, Peru (1) · Instituto Nacional de Estadistica e Informatica (1) · World Gold Council (1) · SIPRI (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **Philippines** (PHL, 120 indicateurs) : Philippine Statistics Authority (55) · Bangko Sentral ng Pilipinas (24) · Bureau of Internal Revenue (6) · World Bank (6) · Bureau of the Treasury, Philippines (4) · World Health Organization (3) · Philippine Central Bank (2) · Philippine National Statistical Coordination Board (2) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Department of Energy, Republic of Philippines (1) · World Gold Council (1) · Department of Finance of the Philippine Republic (1) · National Statistical Coordination Board (NSCB), Philippines (1) · S&P Global (1) · SIPRI (1) · Department of Labor and Employment (1) · Bank for International Settlements (1) · Institute for Economics and Peace (1) · Chamber of Automotive Manufacturers of the Philippines (CAMPI) (1) · Department of Tourism, Philippines (1) · ILO (1)
- **Poland** (POL, 151 indicateurs) : Central Statistical Office of Poland (GUS) (49) · EUROSTAT (29) · National Bank of Poland (23) · OECD (9) · Ministry of Finance, Poland (7) · World Bank (6) · World Health Organization (3) · Ministry of Finance, Income Tax Department (3) · World Economic Forum (2) · Transparency International (2) · European Automobile Manufacturers’ Association (ACEA) (2) · AKAMAI (2) · SIPRI (2) · Republic of Poland (2) · European Commission (1) · European Automobile Manufacturers Association (1) · U.S. Energy Information Administration (1) · Europe's Energy Portal (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · Bank for International Settlements (1) · S&P Global (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **Portugal** (PRT, 162 indicateurs) : Statistics Portugal (71) · Banco de Portugal (22) · EUROSTAT (18) · OECD (7) · World Bank (6) · Instituto da Segurança Social (5) · European Central Bank (3) · World Health Organization (3) · Autoridade Tributária e Aduaneira (3) · DGO - Direccao Geral do Orcamento, Portugal (3) · World Economic Forum (2) · Transparency International (2) · Bank for International Settlements (2) · AKAMAI (2) · IEFP - Institute of Employment and Professional Formation, Portugal (2) · SIPRI (2) · European Commission (1) · ACAP (1) · European Automobile Manufacturers Association (1) · European Automobile Manufacturers’ Association (ACEA) (1) · Direcção-Geral de Energia e Geologia, Portugal (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · Institute for Economics and Peace (1) · National Institute of Statistics (INE) (1)
- **Puerto Rico** (PRI, 59 indicateurs) : Statistical Institute of Puerto Rico (26) · Departamento del Trabajo y Recursos Humanos, Puerto Rico (8) · World Bank (7) · World Health Organization (3) · Junta de Planificación de Puerto Rico (2) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · US Bankruptcy Court of the District of Puerto Rico (1) · Oficina del Comisionado de Instituciones Financieras (1) · Government Development Bank for Puerto Rico (1) · Taxation and Revenue (1) · Departamento de Asuntos del Consumidor (DACO) (1) · Department of the Treasury of Puerto Rico (1) · Puerto Rican Department of Treasury (1)
- **Qatar** (QAT, 103 indicateurs) : National Planning Council, State of Qatar (44) · Qatar Central Bank (18) · Qatar Statistics Authority (6) · World Bank (6) · Ministry of Finance, Qatar (4) · World Health Organization (3) · Public Revenues and Taxes Department, Ministry of Finance (3) · Ministry of Development Planning and Statistics (2) · World Economic Forum (2) · Public Revenues and Tax Department, Qatar (2) · Transparency International (2) · AKAMAI (2) · U.S. Energy Information Administration (1) · Qatar Petroleum (1) · Qatar Planning and Statistics Authority (1) · World Gold Council (1) · S&P Global (1) · SIPRI (1) · Ministry of Administrative Development, Labour and Social Affairs (1) · Institute for Economics and Peace (1) · SESRIC (1)
- **Romania** (ROU, 128 indicateurs) : Institutul National de Statistica (42) · EUROSTAT (24) · National Bank of Romania (18) · World Bank (5) · Ministry Of Public Finance, Romania (5) · European Commission (3) · World Health Organization (3) · National Agency for Fiscal Administration (ANAF) (3) · Ministry of Labor, Family, and Social Protection (3) · World Economic Forum (2) · Transparency International (2) · Ministerul Finantelor Publice (2) · European Automobile Manufacturers’ Association (ACEA) (2) · AKAMAI (2) · National Revenue Agency (2) · ACEA - European Automobile Manufacturers' Association (1) · U.S. Energy Information Administration (1) · Europe's Energy Portal (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · OECD (1) · S&P Global (1) · SIPRI (1) · Institute for Economics and Peace (1) · ANOFM, Romania (1)
- **Russia** (RUS, 131 indicateurs) : Federal State Statistics Service (55) · Central Bank of Russia (22) · Federal Tax Service of Russia (6) · World Bank (6) · Federal Treasury Of Russia (4) · S&P Global (3) · World Health Organization (3) · Ministry of Finance (3) · Association of European Businesses (2) · World Economic Forum (2) · Transparency International (2) · OECD (2) · Ministry of Finance of the Russian Federation (2) · Bank for International Settlements (2) · AKAMAI (2) · Ministry of Economic Development (2) · SIPRI (2) · MNI Deutsche Börse Group (2) · Pension Fund of the Russian Federation (2) · Unified Federal Register of Information on Bankruptcy (1) · Organization of the Petroleum Exporting Countries (1) · Ministry of Finance Russia (1) · World Gold Council (1) · The Federal Service of Labour and Employment (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **Rwanda** (RWA, 76 indicateurs) : National Institute of Statistics of Rwanda (34) · National Bank of Rwanda (13) · World Bank (5) · National Institute of Statistics of Rwanda (NISR) (4) · World Health Organization (3) · Rwanda Revenue Authority (3) · Rwanda Social Security Board (3) · World Economic Forum (2) · Transparency International (2) · International Monetary Fund (2) · AKAMAI (2) · National Bank of Rwanda (BNR) (1) · SIPRI (1) · Institute for Economics and Peace (1)
- **SEN** (SEN, 72 indicateurs) : ANSD, Senegal (39) · Central Bank of West African States (BCEAO) (10) · World Bank (6) · World Health Organization (3) · General Direction of Taxes (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · IMF (1) · BCEAO (1) · SIPRI (1) · Direction Generale des Impots et des Domaines (1) · Institute for Economics and Peace (1)
- **SYR** (SYR, 44 indicateurs) : Central Bureau Of Statistics, Syrian Arab Republic (12) · Syrian Center for Policy Research (10) · World Bank (8) · Central Bank of Syria (3) · World Economic Forum (2) · Ministry of Finance, Syria (2) · Transparency International (2) · AKAMAI (2) · U.S. Energy Information Administration (1) · Institute for Economics and Peace (1) · International Labor Organization (ILO) (1)
- **Saint Kitts and Nevis** (KNA, 19 indicateurs) : Eastern Caribbean Central Bank (12) · World Bank (7)
- **Saint Lucia** (LCA, 28 indicateurs) : Eastern Caribbean Central Bank (12) · World Bank (6) · World Health Organization (3) · National Insurance Corporation (3) · Transparency International (2) · Inland Revenue Department (1) · Inland Revenue Department of Saint Lucia (1)
- **Saint Vincent and Grenadines** (VCT, 12 indicateurs) : World Bank (6) · World Health Organization (3) · Transparency International (2) · Inland Revenue Department (1)
- **Samoa** (WSM, 13 indicateurs) : World Bank (6) · Ministry of Customs and Revenue, Samoa (3) · Transparency International (2) · AKAMAI (2)
- **San Marino** (SMR, 8 indicateurs) : World Bank (3) · World Health Organization (2) · AKAMAI (2) · Finance Department, San Marino (1)
- **Saudi Arabia** (SAU, 124 indicateurs) : General Authority for Statistics, Saudi Arabia (56) · Saudi Central Bank (30) · General Authority of Zakat and Tax (GAZT) (7) · World Bank (5) · World Health Organization (3) · National Debt Management Center, Ministry of Finance (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Ministry of Labour (2) · Dun & Bradstreet (1) · Nielsen (1) · Organization of the Petroleum Exporting Countries (1) · Baker Hughes Company (1) · Ministry of Energy, Industry and Mineral Resources (1) · World Gold Council (1) · Bank for International Settlements (1) · S&P Global (1) · SIPRI (1) · World Steel Association (1) · Institute for Economics and Peace (1) · Central Department of Statistics and Information (1)
- **Serbia** (SRB, 100 indicateurs) : Statistical Office of the Republic of Serbia (45) · National Bank of Serbia (18) · Ministry of Finance, Republic of Serbia (9) · World Bank (5) · World Health Organization (3) · EUROSTAT (3) · Institute for Social Insurance (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · U.S. Energy Information Administration (1) · Petrol (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · OECD (1) · SIPRI (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **Seychelles** (SYC, 81 indicateurs) : National Bureau of Statistics, Seychelles (34) · Central Bank of Seychelles (26) · World Bank (5) · World Health Organization (3) · Seychelles Revenue Commission (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · International Monetary Fund (1) · SIPRI (1) · National Bureau of Statistics (1) · Ministry of Finance, National Planning and Trade, Republic of Seychelles (1)
- **Sierra Leone** (SLE, 44 indicateurs) : Bank of Sierra Leone (13) · Statistics Sierra Leone (8) · World Bank (7) · Ministry of Finance, Sierra Leone (3) · World Economic Forum (2) · Ministry of Finance and Economic Development (2) · Transparency International (2) · AKAMAI (2) · African Development Bank Group (1) · International Monetary Fund (1) · SIPRI (1) · National Revenue Authority (1) · Institute for Economics and Peace (1)
- **Singapore** (SGP, 121 indicateurs) : Statistics Singapore (60) · Monetary Authority of Singapore (13) · Inland Revenue Authority of Singapore (8) · World Bank (6) · Ministry of Manpower Singapore (4) · World Health Organization (3) · Urban Redevelopment Authority (3) · World Economic Forum (2) · Transparency International (2) · Bank for International Settlements (2) · Singapore Economic Development Board (2) · AKAMAI (2) · Land Transport Authority Singapore (1) · S&P Global (1) · Ipsos (1) · IMF (1) · Singapore Petroleum Company Limited (1) · World Gold Council (1) · Government of Singapore (1) · Development Bank of Singapore (1) · Singapore Institute of Purchasing & Materials Management, SIPMM (1) · SIPRI (1) · Monetary Authority, Singapore (1) · Singapore Statistics (1) · Institute for Economics and Peace (1) · Singapore Tourism Board (1)
- **Slovakia** (SVK, 155 indicateurs) : Statistical Office of the Slovak Republic (69) · EUROSTAT (21) · OECD (9) · National Bank of Slovakia (9) · Národná banka Slovenska (7) · Tax Directorate, Slovakia (6) · World Bank (6) · World Health Organization (3) · Ministry Of Finance Of Slovak Republic (3) · World Economic Forum (2) · Transparency International (2) · European Automobile Manufacturers’ Association (ACEA) (2) · European Central Bank (2) · AKAMAI (2) · SIPRI (2) · Social Insurance Agency, Slovak Republic (2) · Ministry of Labour, Social Affairs and Family of the Slovak Republic (2) · European Automobile Manufacturers Association (1) · World Gold Council (1) · Ministry of Finance of the Slovak Republic (1) · United Nations Economic Commission for Europe (UNECE) (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **Slovenia** (SVN, 147 indicateurs) : Statistical Office of the Republic of Slovenia (60) · EUROSTAT (22) · Bank of Slovenia (13) · OECD (9) · Tax Administration of the Republic of Slovenia (9) · World Bank (6) · Ministry of Finance, Republic of Slovenia (4) · European Central Bank (3) · World Health Organization (3) · Employment Service of Slovenia (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · European Commission (1) · European Automobile Manufacturers’ Association (ACEA) (1) · Petrol (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · SIPRI (1) · Bank for International Settlements (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **Solomon Islands** (SLB, 20 indicateurs) : World Bank (7) · Central Bank of Solomon Islands (CBSI) (5) · Transparency International (2) · Solomon Islands National Statistics Office (2) · AKAMAI (2) · Inland Revenue Division Solomon Islands (1) · Ministry of Finance & Treasury, Solomon Islands (1)
- **Somalia** (SOM, 31 indicateurs) : Somali National Bureau Of Statistics (8) · World Bank (7) · International Trade Centre (3) · Directorate of National Statistics, Somalia (3) · World Health Organization (3) · Transparency International (2) · AKAMAI (2) · SIPRI (1) · Ministry of Finance and Planning (1) · Institute for Economics and Peace (1)
- **South Africa** (ZAF, 149 indicateurs) : Statistics South Africa (65) · South African Reserve Bank (34) · World Bank (7) · South African Revenue Service (6) · Bureau for Economic Research (BER) (4) · National Treasury, South Africa (4) · World Health Organization (3) · South African Social Security Agency (SASSA) (3) · NAAMSA, South Africa (2) · World Economic Forum (2) · OECD (2) · Transparency International (2) · AKAMAI (2) · SIPRI (2) · South Africa Government Services (2) · South African Chamber of Commerce and Industry (SACCI) (1) · S&P Global (1) · IMF (1) · Energy Department of Republic of South Africa (1) · World Gold Council (1) · Bank for International Settlements (1) · Department of Employment and Labour - South Africa (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **South Korea** (KOR, 152 indicateurs) : The Bank of Korea (47) · Statistics Korea (33) · Bank of Korea (8) · World Bank (6) · Ministry of Trade, Industry & Energy (MOTIE) (5) · Korean Worker's Compensation & Welfare Service (5) · OECD (4) · World Health Organization (3) · National Tax Service, South Korea (3) · Korea Automobile Manufacturers Association (KAMA) (2) · Ministry of Land, Infrastructure and Transport (2) · Ministry of Trade, Industry and Energy (2) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Korea Productivity Center (2) · SIPRI (2) · Korea Tourism Organization (2) · Ministry of Employment and Labor, South Korea (2) · Ministry of Justice, South Korea (1) · Statistics Korea (KOSTAT) (1) · Korea Electric Power Corporation (KEPCO) (1) · Kostat (1) · Korea National Oil Corporation (1) · World Gold Council (1) · Ministry of Strategy and Finance, South Korea (1) · Korea Real Estate Board (1) · Bank for International Settlements (1) · Kookmin bank, South Korea (1) · S&P Global (1) · Minimum Wage Council (1) · South Korea Economics Statistics System (1) · Korea Customs Service (1) · World Steel Association (1) · Ministry of Budget, Ministry of Finance (1) · Institute for Economics and Peace (1) · KOSTAT - Statistics Korea (1)
- **South Sudan** (SSD, 25 indicateurs) : National Bureau of Statistics, South Sudan (14) · World Bank (5) · Transparency International (2) · Central Bank of South Sudan (1) · Bank of South Sudan (1) · SIPRI (1) · Institute for Economics and Peace (1)
- **Spain** (ESP, 168 indicateurs) : National Statistics Institute (INE) (60) · Bank of Spain (22) · EUROSTAT (18) · OECD (10) · Ministerio de Empleo y Seguridad Social (7) · World Bank (6) · Agencia Tributaria, Spain (4) · Ministerio de Economía, Comercio y Empresa (3) · S&P Global (3) · World Health Organization (3) · European Commission (2) · ANFAC - Asociacion Espanola de Fabricantes de Automoviles y Camiones (2) · Ministerio de Hacienda Y Administraciones Publicas (2) · World Economic Forum (2) · Transparency International (2) · European Automobile Manufacturers’ Association (ACEA) (2) · European Central Bank (2) · Bank for International Settlements (2) · AKAMAI (2) · SIPRI (2) · Ministerio de Industria, Energia Y Turismo (1) · ACEA - European Automobile Manufacturers' Association (1) · Centro de Investigaciones Sociológicas (1) · Europe's Energy Portal (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · Ministerio de Fomento (1) · Ministerio de Transportes, Movilidad y Agenda Urbana (1) · Instituto de Empleo, Servicio Publico de Empleo Estatal, Spain (1) · World Steel Association (1) · Institute for Economics and Peace (1) · Ministry of the Economy and Finance (1)
- **Sri Lanka** (LKA, 110 indicateurs) : Central Bank of Sri Lanka (61) · The Department of Census and Statistics, Sri Lanka (23) · World Bank (6) · World Health Organization (3) · Sri Lanka Inland Revenue Department (3) · Ministry of Employment and Labor (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Department of Census and Statistics - Sri Lanka (1) · World Gold Council (1) · SIPRI (1) · Institute for Economics and Peace (1) · Ceylon Tourist Board (1)
- **Sudan** (SDN, 34 indicateurs) : World Bank (8) · Central Bank of Sudan (6) · Sudan Central Bureau of Statistics (4) · World Health Organization (3) · Sudan Chamber of Taxation (3) · Transparency International (2) · AKAMAI (2) · U.S. Energy Information Administration (1) · The Ministry of Oil and Gas, Sudan (1) · IMF (1) · SIPRI (1) · Institute for Economics and Peace (1) · International Labour Organization (ILO) (1)
- **Suriname** (SUR, 46 indicateurs) : Central Bank of Suriname (17) · World Bank (8) · Statistics Suriname (4) · World Health Organization (3) · Ministry of Finance (3) · World Economic Forum (2) · Transparency International (2) · Suriname Debt Management Office (2) · AKAMAI (2) · U.S. Energy Information Administration (1) · Inter-American Development Bank (1) · World Gold Council (1)
- **Sweden** (SWE, 171 indicateurs) : Statistics Sweden (84) · EUROSTAT (20) · Sveriges Riksbank (11) · OECD (9) · Skatteverket (7) · World Bank (6) · National Institute of Economic Research, Sweden (3) · World Health Organization (3) · Swedish National Financial Management Authority (3) · World Economic Forum (2) · Transparency International (2) · Swedish National Debt Office (2) · AKAMAI (2) · Swedbank (2) · SIPRI (2) · Ministry of Health and Social Affairs, Sweden (2) · European Automobile Manufacturers’ Association (ACEA) (1) · Europe's Energy Portal (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · Bank for International Settlements (1) · Swedish Financial Benchmark Facility (1) · Swedish Public Employment Service (1) · National Institute of Economic Research (NIER) (1) · World Steel Association (1) · Institute for Economics and Peace (1) · Swedish National Mediation Office (1)
- **Switzerland** (CHE, 144 indicateurs) : Swiss Federal Statistical Office (39) · Swiss National Bank (19) · EUROSTAT (16) · State Secretariat for Economic Affairs (14) · OECD (8) · Swiss Federal Finance Administration (7) · World Bank (6) · Federal Department of Home Affairs (5) · Swiss Federal Tax Administration (4) · Federal Customs Administration (3) · World Health Organization (3) · Swiss Economic Institute (KOF) (2) · World Economic Forum (2) · Transparency International (2) · Bank for International Settlements (2) · AKAMAI (2) · SIPRI (2) · European Automobile Manufacturers’ Association (ACEA) (1) · Shell Suisse (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · procure.ch & UBS (1) · procure.ch & Credit Suisse (1) · Institute for Economics and Peace (1) · UBS & CFA Society Switzerland (1)
- **São Tomé and Príncipe** (STP, 29 indicateurs) : Central Bank of Sao Tome and Principe (12) · World Bank (5) · Instituto Nacional de Estatistica (3) · Transparency International (2) · AKAMAI (2) · IMF (1) · Banco de São Tomé e Príncipe (1) · INE, Sao Tome and Principe (1) · Banco Central de São Tomé e Príncipe (1) · International Labour Organization (ILO) (1)
- **Taiwan** (TWN, 119 indicateurs) : DGBAS, Taiwan (21) · Central Bank of the Republic of China (Taiwan) (16) · Directorate-General of Budget, Accounting and Statistics, Taiwan (13) · Ministry of Finance, R.O.C (9) · Directorate-General of Budget, Accounting and Statistics, Executive Yuan, Taiwan (9) · Ministry of Economic Affairs, R.O.C (5) · Ministry of Economic Affairs (MOEA), Taiwan (4) · World Health Organization (3) · Taxation Administration, Ministry of Finance, R.O.C (3) · World Bank (3) · National Statistics, Republic of China (3) · National Statistics, Republic of China (Taiwan) (3) · National Tax Administration, Ministry of Finance (3) · National Development Council (NDC), Taiwan (2) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Ministry of the Interior, Taiwan (1) · TTVMA - Taiwan Transportation Vehicle Manufacturers Association (1) · Ministry of Transportation, R.O.C (1) · Ministry of Economic Affairs, Taiwan (1) · Research Center for Taiwan Economic Development (RCTED) (1) · U.S. Energy Information Administration (1) · Department of Investment Review, MOEA (1) · World Gold Council (1) · Sinyi Realty Inc (1) · S&P Global (1) · SIPRI (1) · Council of Labour Affairs (1) · Ministry of Finance, Customs Administration (Taiwan Customs Administration) (1) · World Steel Association (1) · Institute for Economics and Peace (1) · Taiwan (1)
- **Tajikistan** (TJK, 64 indicateurs) : National Bank of Tajikistan (19) · State statistical committee of the Republic of Tajikistan (10) · World Bank (9) · Agency of Statistics of Tajikistan (5) · National Center of Legislation under the President of the Republic of Tajikistan (4) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · Supreme Council of the Republic of Tajikistan (2) · U.S. Energy Information Administration (1) · United Nations Economic Commission for Europe (UNECE) (1) · OECD (1) · SIPRI (1) · Social Insurance and Pension Agency, Tajikistan (1) · Agency on Statistics, Tajikistan (1) · Agency on Statistics under President of the Republic of Tajikistan (1) · Ministry of Finance (1) · Institute for Economics and Peace (1)
- **Tanzania** (TZA, 86 indicateurs) : National Bureau of Statistics (NBS) - Tanzania (40) · Bank of Tanzania (20) · World Bank (5) · National Bureau of Statistics, Tanzania (4) · World Health Organization (3) · Tanzania Revenue Authority (3) · World Economic Forum (2) · Transparency International (2) · National Bureau of Statistics of Tanzania (2) · IMF (1) · Energy and Water Utilities Regulatory Authority (EWURA) (1) · SIPRI (1) · Prime Minister’s Office – Labour, Youth, Employment and Persons with Disabilities, Tanzania (1) · Institute for Economics and Peace (1)
- **Thailand** (THA, 123 indicateurs) : Bank of Thailand (44) · Nesdb, Thailand (18) · Bureau of Trade and Economic Indices, Ministry of Commerce, Thailand (7) · World Bank (6) · Ministry of Commerce, Thailand (5) · The Federation of Thai Industries (4) · Bureau of Trade and Economic Indices, Thailand (4) · World Health Organization (3) · The Revenue Department, Government of Thailand (3) · Trade Policy and Strategy Office (3) · National Statistical Office of Thailand (3) · Thailand Revenue Department (3) · World Economic Forum (2) · Transparency International (2) · Ministry of Finance, Thailand (2) · Office of Industrial Economics, Thailand (2) · AKAMAI (2) · University of the Thai Chamber of Commerce (1) · U.S. Energy Information Administration (1) · Bangchak Corporation Public Company Limited (1) · World Gold Council (1) · Bank for International Settlements (1) · Department of Employment, Ministry of Labour (1) · S&P Global (1) · SIPRI (1) · Institute for Economics and Peace (1) · Ministry of Tourism & Sports, Thailand (1)
- **Timor-Leste** (TLS, 37 indicateurs) : National Statistics Directorate, Timor (19) · World Bank (7) · World Health Organization (3) · World Economic Forum (2) · Transparency International (2) · Central Bank of Timor-Leste (2) · SIPRI (1) · Institute for Economics and Peace (1)
- **Togo** (TGO, 37 indicateurs) : Central Bank of West African States (BCEAO) (10) · World Bank (7) · DGSCN, Togo (4) · World Health Organization (3) · Stat Togo (2) · Transparency International (2) · African Development Bank Group (2) · AKAMAI (2) · BCEAO (1) · SIPRI (1) · General Direction of Taxes (1) · Institute for Economics and Peace (1) · International Labour Organization (ILO) (1)
- **Tonga** (TON, 11 indicateurs) : World Bank (5) · Transparency International (2) · AKAMAI (2) · Ministry of Revenue & Customs (1) · Ministry of Finance and National Planning (1)
- **Trinidad and Tobago** (TTO, 67 indicateurs) : Central Bank of Trinidad and Tobago (35) · World Bank (7) · Central Statistical Office, Trinidad and Tobago (5) · World Health Organization (3) · Inland Revenue Division (3) · World Economic Forum (2) · Transparency International (2) · Central Statistical Office (2) · AKAMAI (2) · U.S. Energy Information Administration (1) · Inter-American Development Bank (1) · World Gold Council (1) · SIPRI (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **Tunisia** (TUN, 84 indicateurs) : National Institute of Statistics - Tunisia (40) · Central Bank of Tunisia (18) · World Bank (7) · World Health Organization (3) · La Direction Générale des Impôts, Ministère des Finances (3) · National Social Security Fund (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · U.S. Energy Information Administration (1) · World Gold Council (1) · SIPRI (1) · Institute for Economics and Peace (1)
- **Turkmenistan** (TKM, 31 indicateurs) : SESRIC - The Statistical, Economic & Social Res. & Training Centre for Islamic Countries (8) · World Bank (6) · National Institute of State Statistics and Information, Turkmenistan (3) · Transparency International (2) · Asian Development Bank (ADB) (2) · AKAMAI (2) · Ministry of Labour and Social Protection of Population of Turkmenistan (2) · U.S. Energy Information Administration (1) · International Monetary Fund (1) · OECD (1) · SESRIC (1) · Ministry of Finance (1) · Institute for Economics and Peace (1)
- **Türkiye** (TUR, 163 indicateurs) : Turkish Statistical Institute (66) · Central Bank of the Republic of Turkey (28) · OECD (10) · EUROSTAT (9) · Undersecretariat of Treasury, Turkey (7) · World Bank (6) · Ministry of Labor and Social Security (6) · Turkish Revenue Administration (4) · World Health Organization (3) · Turkish Automotive Manufacturers Association (OSD) (2) · World Economic Forum (2) · Transparency International (2) · Bank for International Settlements (2) · AKAMAI (2) · SIPRI (2) · United Nations Refugee Agency (1) · TOBB - Union of Chambers and Commodity Exchanges of Turkey (1) · U.S. Energy Information Administration (1) · Türkiye Petrolleri (1) · World Gold Council (1) · Ministry of Treasury and Finance, Turkey (1) · United Nations Economic Commission for Europe (UNECE) (1) · Banks Association of Turkey (1) · S&P Global (1) · World Steel Association (1) · Institute for Economics and Peace (1) · Ministry of Culture and Tourism, Turkey (1)
- **Uganda** (UGA, 85 indicateurs) : Uganda Bureau of Statistics (36) · Bank of Uganda (22) · World Bank (7) · World Health Organization (3) · Uganda Revenue Authority (3) · National Social Security Fund, Uganda (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · S&P Global (1) · African Development Bank Group (1) · SIPRI (1) · Ministry of Gender, Labour and Social Development (1) · Institute for Economics and Peace (1)
- **Ukraine** (UKR, 114 indicateurs) : State Statistics Service of Ukraine (52) · National Bank of Ukraine (19) · World Bank (8) · Ministry of Revenue and Duties of Ukraine (6) · Ministry of Finance of Ukraine (4) · Ministry of Social Policy of Ukraine (4) · World Health Organization (3) · Ministry of Finance, Ukraine (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · SIPRI (2) · Info Sapiens (1) · IMF (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · OECD (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **United Arab Emirates** (ARE, 88 indicateurs) : Federal Competitiveness and Statistics Centre (27) · Central Bank of the United Arab Emirates (13) · World Bank (8) · National Bureau of Statistics, UAE (5) · Central Bank of United Arab Emirates (3) · World Health Organization (3) · Dubai Data and Statistics Establishment - Government of Dubai (3) · Ministry of Finance, UAE (3) · General Pension and Social Security Authority (GPSSA) (3) · World Economic Forum (2) · Ministry of Finance, United Arab Emirates (2) · Transparency International (2) · AKAMAI (2) · SIPRI (2) · Nielsen (1) · Organization of the Petroleum Exporting Countries (1) · Baker Hughes Company (1) · IMF (1) · Ministry of Energy & Infrastructure, UAE (1) · World Gold Council (1) · Central Bank of the UAE (1) · S&P Global (1) · Ministry of Finance (1) · Institute for Economics and Peace (1)
- **United Kingdom** (GBR, 235 indicateurs) : Office for National Statistics (130) · Bank of England (24) · HM Revenue & Customs (9) · OECD (7) · World Bank (6) · The Society of Motor Manufacturers and Traders Limited (SMMT) (5) · Halifax and Bank of Scotland (4) · Confederation of British Industry (4) · S&P Global (4) · World Health Organization (3) · Office for Budget Responsibility, UK (3) · EUROSTAT (3) · Nationwide Building Society, United Kingdom (3) · BRC - British Retail Consortium (2) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · SIPRI (2) · Home Office, UK Government (1) · The Insolvency Service, United Kingdom (1) · DG ECFIN - Directorate General for Economic and Financial Affairs (1) · GfK Group (1) · U.S. Energy Information Administration (1) · Baker Hughes Company (1) · Department for Business, Energy & Industrial Strategy, United Kingdom (1) · RAC Motoring Services, UK (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · Bank for International Settlements (1) · Ministry of Housing, Communities and Local Government, UK (1) · YouGov/Citigroup (1) · Department for Work and Pensions, UK (1) · UK Finance (1) · Royal Institution of Chartered Surveyors (RICS) (1) · Nationwide Building Society, UK (1) · World Steel Association (1) · Institute for Economics and Peace (1) · Centre for European Economic Research (ZEW) (1)
- **United States** (USA, 422 indicateurs) : U.S. Bureau of Labor Statistics (69) · U.S. Census Bureau (48) · U.S. Bureau of Economic Analysis (39) · Federal Reserve (21) · Federal Reserve Bank of New York (21) · Institute for Supply Management (15) · U.S. Energy Information Administration (14) · U.S. Department of Labor (12) · U.S. Department of the Treasury (11) · Federal Reserve Bank of Dallas (9) · American Petroleum Institute (API) (8) · OECD (8) · U.S. Bureau of Economic Analysis (BEA) (8) · Federal Reserve Bank of Chicago (7) · Federal Reserve Bank of Philadelphia (7) · US Department of Agriculture (7) · Mortgage Bankers Association of America (6) · World Bank (6) · National Association of Realtors (6) · Federal Reserve Bank of Kansas City (6) · Automatic Data Processing, Inc (5) · University of Michigan (5) · Logistics Managers' Index (5) · Social Security Administration (5) · Standard & Poor's (4) · Internal Revenue Service (4) · S&P Global (3) · World Health Organization (3) · Bureau of Labor Statistics (3) · Federal Housing Finance Agency (3) · Federal Reserve Bank of Richmond (3) · Freddie Mac (2) · Truckstop (2) · Challenger, Gray and Christmas, Inc (2) · World Economic Forum (2) · Federal Reserve Bank of Cleveland (2) · Baker Hughes Company (2) · Financial Management Service, United States (2) · U.S. Treasury (2) · Bank for International Settlements (2) · AKAMAI (2) · SIPRI (2) · Office of Travel and Tourism Industries (2) · Manheim Consulting Inc (2) · US Department of Justice (1) · Bureau of Economic Analysis (BEA) (1) · Administrative Office of the U.S. Courts (1) · Fitch Auto ABS Indices (1) · U.S. Bureau of Economics Analysis (1) · Transparency International (1) · Technometrica Market Intelligence/RealClearMarkets (1) · Federal Reserve Bank of St. Louis (1) · Federal Reserve Bank of Atlanta (1) · World Gold Council (1) · Financial Management Service, US Treasury (1) · Office of Management and Budget, The White House (1) · United Nations Economic Commission for Europe (UNECE) (1) · The Conference Board (1) · Department of Labor (1) · National Association of Home Builders (1) · National Federation of Independent Business (1) · Federal Reserve Bank of San Francisco (1) · Redbook Research Inc (1) · CNBC/National Retail Federation (1) · United States International Trade Commission (1) · Fit Small Business (1) · World Steel Association (1) · Institute for Economics and Peace (1) · Wards Intelligence (1)
- **Uruguay** (URY, 93 indicateurs) : Banco Central del Uruguay (31) · Instituto Nacional de Estadística, Uruguay (24) · World Bank (8) · World Health Organization (3) · Dirección General Impositiva (3) · Ministerio de Economía y Finanzas, Uruguay (3) · Ministerio de Trabajo y Seguridad Social (3) · National Institute of Statistics (2) · World Economic Forum (2) · Transparency International (2) · Ministerio de Economia y Finanzas, Uruguay (2) · AKAMAI (2) · Inter-American Development Bank (1) · Administración Nacional de Combustibles Alcohol y Pórtland (1) · World Gold Council (1) · SIPRI (1) · World Steel Association (1) · Institute for Economics and Peace (1) · Ministerio de Turismo, Uruguay (1) · National Institute of Statistics of Uruguay (1)
- **Uzbekistan** (UZB, 73 indicateurs) : State Committee on Statistics of Uzbekistan (25) · Central Bank of Uzbekistan (11) · World Bank (7) · Ministry of Economy and Finance of the Republic of Uzbekistan (4) · World Health Organization (3) · State Tax Committee (3) · Ministry of Labour and Employment Relations (3) · Transparency International (2) · Ministry of Finance of the Republic of Uzbekistan (2) · AKAMAI (2) · The national database of legislation of the Republic of Uzbekistan (2) · Center for Economic Research and Reforms (CERR) (1) · U.S. Energy Information Administration (1) · SESRIC (1) · World Gold Council (1) · United Nations Economic Commission for Europe (UNECE) (1) · OECD (1) · World Steel Association (1) · Institute for Economics and Peace (1) · Ministry of Employment and Labor Relations of the Republic of Uzbekistan (1)
- **VIR** (VIR, 2 indicateurs) : World Bank (2)
- **Vanuatu** (VUT, 23 indicateurs) : Vanuatu Bureau of Statistics (8) · World Bank (6) · Vanuatu National Statistics Office (3) · Customs and Inland Revenue Department, Vanuatu (2) · Transparency International (2) · AKAMAI (2)
- **Venezuela** (VEN, 65 indicateurs) : Banco Central De Venezuela (27) · World Bank (6) · National Institute of Statistics, Venezuela (5) · World Health Organization (3) · Ministerio del Poder Popular de Finanzas, SENIAT (3) · Instituto Venezolano de los Seguros Sociales (3) · World Economic Forum (2) · Transparency International (2) · IMF (2) · AKAMAI (2) · Venezuela Central Bank (2) · Nielsen (1) · Organization of the Petroleum Exporting Countries (1) · Baker Hughes Company (1) · Ministerio del Poder Popular de Petróleo y Mineria (1) · Ministry of Planning and Finance, Government of Venezuela (1) · SIPRI (1) · World Steel Association (1) · Institute for Economics and Peace (1)
- **Vietnam** (VNM, 95 indicateurs) : General Statistics Office of Vietnam (47) · World Bank (7) · Asian Development Bank (6) · The State Bank of Vietnam (5) · World Health Organization (3) · General Department Of Taxation - Ministry Of Finance (3) · General Department of Taxation (3) · World Economic Forum (2) · Transparency International (2) · IMF (2) · AKAMAI (2) · Ministry of Labour, War Invalids, and Social Affairs (2) · Vietnam Automobile Manufacturers Association (1) · Nielsen (1) · U.S. Energy Information Administration (1) · Vietnam National Petroleum Group (1) · Ministry of Finance (1) · ADB - Asian Development Bank (1) · S&P Global (1) · SIPRI (1) · Ministry of Labour, Vietnam (1) · National Statistics Office of Vietnam (1) · Institute for Economics and Peace (1)
- **World** (WLD, 12 indicateurs) : Food and Agriculture Organization of the United Nations (6) · World Health Organization (3) · World Bank (2) · Federal Reserve Bank of New York (1)
- **Yemen** (YEM, 38 indicateurs) : SESRIC (9) · World Bank (8) · Central Bank of Yemen (3) · Yemen Tax Authority (3) · Central Statistics Office, Yemen (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · U.S. Energy Information Administration (1) · FRED (1) · Ministry of Finance, Yemen (1) · International Monetary Fund (1) · SIPRI (1) · Institute for Economics and Peace (1)
- **Zambia** (ZMB, 71 indicateurs) : Central Statistical Office, Zambia (26) · Bank of Zambia (16) · World Bank (6) · Central Statistical Office of Zambia (5) · World Health Organization (3) · Zambia Revenue Authority (3) · National Pension Scheme Authority (3) · World Economic Forum (2) · Transparency International (2) · AKAMAI (2) · S&P Global (1) · SIPRI (1) · Institute for Economics and Peace (1)
- **Zimbabwe** (ZWE, 51 indicateurs) : Reserve Bank of Zimbabwe (13) · Zimbabwe National Statistics Agency (12) · World Bank (5) · World Health Organization (3) · Zimbabwe Revenue Authority (3) · World Economic Forum (2) · Transparency International (2) · African Development Bank Group (2) · ZIMSTAT, Zimbabwe (2) · AKAMAI (2) · Zimbabwe Energy Regulatory Authority (1) · SIPRI (1) · Institute for Economics and Peace (1) · Zimbabwe Tourism Authority (1) · International Labour Organization (ILO) (1)


## 4. Sources gratuites retenues

Critères : données **gratuites** (pas seulement le chiffre-titre), **lisibles par un programme** sans clé ou avec
une clé gratuite, et **couvrant beaucoup de pays** — ce qui désigne les compilations officielles dont TE se sert
lui-même (et qu'il nomme sur sa page databases) plutôt que les 150 instituts nationaux un par un, dont la plupart
ne publient que des tableaux. Les instituts nationaux restent dans le catalogue, avec leur mode d'accès, pour les
cas où l'on voudra une série particulière à la source.

| Source | Ce qu'elle apporte | État dans deepwater |
|---|---|---|
| **Eurostat** (API de diffusion, JSON-stat, sans clé) | IPCH, chômage, PIB, production, ventes au détail, confiance, prix à la production, dette et déficit trimestriels, UE + AELE + candidats | **Ajouté** (`EurostatIndicators`) |
| **OCDE** Key Economic Indicators (SDMX, sans clé) | Prix, chômage, production, ventes, confiance, indicateur avancé, taux 3 mois et 10 ans, salaires, PIB, balance courante, ~45 économies | **Ajouté** (`OecdIndicators`) |
| **FMI** base des prix à la consommation (SDMX 3.0, sans clé) | Inflation mensuelle de ~190 pays | **Ajouté** (`ImfConsumerPrices`) |
| **BLS** API publique v1 (sans clé) | IPC, IPC sous-jacent, chômage, emplois non agricoles, salaires horaires, à la source | **Ajouté** (`BlsIndicators`) |
| FMI WEO, BoP, IMTS, GDD, PCPS ; BRI ; Banque mondiale WDI ; OCDE logement ; BCE (via Frankfurter) ; CFTC ; Trésor US | Déjà dans deepwater avant l'audit | Déjà présents |
| **FRED** (clé gratuite) | Inflation PCE, mises en chantier, inscriptions au chômage, balance commerciale, sentiment Michigan | **Ajouté** (`FredIndicators`, actif dès qu'une clé est saisie dans Sources) |
| **EIA**, **BEA** (clés gratuites) | Stocks et production de brut, prix de l'essence ; PIB annualisé | **Ajoutés** (`EiaIndicators`, `BeaIndicators`, clé requise) |
| **UN Comtrade** (clé gratuite facultative) | Exportations et importations par produit (chapitres SH) | **Ajouté** (`ComtradeProducts`, fiche pays › Commerce par produit ; sans clé, point d'accès public à quota) |
| **Instituts nationaux sans clé** : BCB et IBGE (Brésil), INDEC via datos.gob.ar (Argentine), SARB (Afrique du Sud), SingStat (Singapour) | Inflation, chômage, production, ventes, activité, prix à la production, à la source | **Ajoutés** (`src/NationalSources.*`), prioritaires sur les compilations |
| **Instituts nationaux à clé** : Banxico (Mexique), Banque de Corée (ECOS), banque centrale turque (EVDS) | Inflation, taux cible | **Ajoutés** (`src/KeyedSources.*`, clé requise) |
| NBS Chine, MOSPI Inde, DOSM Malaisie, Rosstat, StatsSA, TÜİK | — | Non exploitables sans navigateur ou sans paramètres documentés ; au catalogue |
| Les autres instituts statistiques et banques centrales (environ 150 pays) | Les séries à la source | **Catalogue** : nom, site, type, accès, API et état du site, dans Sources et dans la fiche économie |
| S&P Global, ISM, Ifo, ZEW, GfK, Michigan, Conference Board, NAB, Westpac, ANZ, CBI, NFIB, Sentix, ADP… | Enquêtes (chiffre-titre gratuit) | **Ajoutés** (`SurveyHeadlines`) : les derniers chiffres-titres lus dans le calendrier économique, une ligne par éditeur dans Sources |
| Notations, marchés, prévisions TE | — | Hors périmètre (payant ou propriétaire) |

## 5. Ce qui a été ajouté à deepwater

- **Fiche économie › Derniers chiffres** : 18 indicateurs (croissance t/t et sur un an, inflation, inflation
  sous-jacente, chômage, emplois, production industrielle, ventes au détail, confiance des entreprises et des
  ménages, indicateur avancé, prix à la production, salaires, taux 3 mois, 10 ans, balance courante, dette et solde
  publics) avec la valeur précédente et la période ; pour chaque ligne la source la plus directe disponible (BLS,
  puis Eurostat, puis OCDE, puis FMI), sauf si une autre est nettement plus récente. L'info-bulle de chaque ligne
  nomme l'organisme crédité par TE pour ce pays et cet indicateur.
- **Fiche économie › Sources officielles** : les organismes derrière les statistiques du pays d'après le catalogue,
  le nombre d'indicateurs de chacun, le type, l'accès, l'API ; un clic ouvre le site.
- **Carte** : deux nouvelles cartes thermiques, « Inflation, dernier mois » (~170 pays) et « Chômage, dernier
  mois » (~45 pays), dans le groupe « Derniers chiffres officiels » de la barre latérale.
- **Onglet Sources** : une section « Carte · statistiques officielles » (OCDE, Eurostat, FMI CPI, BLS : état,
  test, rafraîchissement, interrupteur) et une ligne pour le catalogue.
- **Onglet Sources** (suite) : sections « Carte · instituts nationaux, lus directement » (Brésil, Argentine, Afrique
  du Sud, Singapour, et avec clé Mexique, Corée du Sud, Turquie), « Carte · bases à clé gratuite » (FRED, EIA, BEA,
  UN Comtrade) avec champ de clé et bouton Enregistrer, « Carte · enquêtes privées » (le jeu de données et une
  carte par éditeur, avec interrupteur), et le catalogue complet en cinq groupes repliés (recherche, filtres pays et
  accès communs à tout l'onglet). Aucune mention de Trading Economics dans l'interface : il n'a servi qu'à bâtir
  le catalogue.
- **Fiche économie › Commerce par produit** : exportations et importations par chapitre SH (UN Comtrade), dernière
  année complète déclarée, en part du total.
- Code : `src/KeyIndicators.*`, `src/OfficialSources.*`, `src/NationalSources.*`, `src/KeyedSources.*`,
  `src/SurveyHeadlines.*`, `resources/data_sources.json` ; détails dans le README (section « Latest official figures
  and the sources catalogue »).

Particularités techniques relevées en chemin : l'API du FMI ignore `startPeriod` mais accepte le filtre SDMX 3.0
`c[TIME_PERIOD]=ge:2024-M01` (300 Ko au lieu de 3,7 Mo) ; le jeu Eurostat `prc_hicp_manr` est figé fin 2025, l'IPCH
courant est dans `prc_hicp_minr` (ECOICOP v2, `coicop18=TOTAL`) ; le serveur SDMX de l'OCDE veut un
`Accept-Language: en` ; l'API BLS v1 autorise 25 requêtes par jour et par adresse, une série par requête.

## 6. Inventaire de disponibilité des sources gratuites

Test du 2026-11-01 : une requête GET sur le site de chaque source avec le client de l'application (`User-Agent: Deepwater/0.1`), redirections suivies, 15 s d'attente. Le résultat est inscrit dans le catalogue (`test`) et affiché dans la colonne Test de l'onglet Sources tant qu'un test en session ne l'a pas remplacé.

| Résultat | Sources gratuites | Toutes les sources |
|---|---:|---:|
| répond | 805 | 923 |
| pas de site | 201 | 216 |
| refuse le client (401/403) | 107 | 117 |
| pas de réponse (15 s) | 73 | 76 |
| erreur TLS | 51 | 53 |
| hôte inconnu | 28 | 29 |
| erreur réseau | 11 | 11 |
| erreur HTTP 503 | 9 | 9 |
| connexion refusée | 8 | 8 |
| erreur HTTP 404 | 5 | 6 |
| erreur HTTP 307 | 5 | 5 |
| erreur HTTP 302 | 4 | 4 |
| erreur HTTP 412 | 2 | 2 |
| erreur HTTP 500 | 2 | 2 |
| erreur HTTP 507 | 2 | 2 |
| erreur HTTP 502 | 2 | 2 |
| erreur HTTP 400 | 1 | 1 |
| erreur HTTP 521 | 1 | 1 |
| erreur HTTP 429 | 0 | 1 |

Parmi les sources gratuites, les sites qui répondent portent 12045 des 16626 pages d'indicateur (72.4 %).
Délai médian des sites qui répondent : 845 ms ; 90e centile : 2897 ms.


#### Sources gratuites dont le site ne répond pas

| Source | Type | Pays | Pages | Résultat | Site |
|---|---|---:|---:|---|---|
| Transparency International | Organisation internationale | 188 | 375 | refuse le client (401/403) | https://www.transparency.org/en |
| OECD | Organisation internationale | 66 | 335 | refuse le client (401/403) | https://www.oecd.org/ |
| World Economic Forum | Organisation internationale | 152 | 304 | refuse le client (401/403) | https://www.weforum.org |
| Central Bank of West African States (BCEAO) | Organisation internationale | 10 | 89 | hôte inconnu | http://www.bceao.int |
| Statistics Portugal | Office statistique | 1 | 71 | pas de réponse (15 s) | http://www.ine.pt |
| Statistical Office of the Slovak Republic | Office statistique | 1 | 69 | refuse le client (401/403) | https://slovak.statistics.sk/ |
| U.S. Bureau of Labor Statistics | Office statistique | 1 | 69 | refuse le client (401/403) | http://www.bls.gov/ |
| European Commission | Organisation internationale | 25 | 64 | refuse le client (401/403) | http://ec.europa.eu |
| Statistical Office of the Republic of Slovenia | Office statistique | 1 | 60 | erreur réseau | https://www.stat.si |
| General Authority for Statistics, Saudi Arabia | Office statistique | 1 | 56 | pas de réponse (15 s) | https://www.stats.gov.sa |
| Federal State Statistics Service | Office statistique | 1 | 55 | pas de réponse (15 s) | https://rosstat.gov.ru/ |
| Philippine Statistics Authority | Office statistique | 1 | 55 | refuse le client (401/403) | https://psa.gov.ph |
| Statistics Lithuania | Office statistique | 1 | 53 | refuse le client (401/403) | https://osp.stat.gov.lt/ |
| Statistical Service of the Republic of Cyprus | Office statistique | 1 | 52 | erreur HTTP 302 | https://www.cystat.gov.cy/en/ |
| Banco Central del Paraguay | Banque centrale | 1 | 49 | refuse le client (401/403) | https://www.bcp.gov.py/ |
| Eastern Caribbean Central Bank | Organisation internationale | 4 | 47 | refuse le client (401/403) | https://www.eccb-centralbank.org/ |
| General Statistics Office of Vietnam | Office statistique | 1 | 47 | hôte inconnu | https://www.gso.gov.vn |
| National Statistical Office of Mongolia | Office statistique | 1 | 46 | erreur TLS | http://www.1212.mn |
| Instituto Brasileiro de Geografia e Estatística (IBGE) | Office statistique | 1 | 45 | refuse le client (401/403) | https://www.ibge.gov.br |
| National Statistics Office, Malta | Office statistique | 1 | 45 | refuse le client (401/403) | http://www.nso.gov.mt |
| The State Statistical Committee of the Republic of Azerbaijan | Office statistique | 1 | 45 | erreur HTTP 302 | http://www.stat.gov.az |
| IMF | Organisation internationale | 37 | 44 | refuse le client (401/403) | https://www.imf.org |
| National Planning Council, State of Qatar | Ministère / agence | 1 | 44 | erreur TLS | https://www.npc.qa/en/Pages/default.aspx |
| Palestinian Central Bureau of Statistics | Office statistique | 1 | 43 | erreur TLS | http://www.pcbs.gov.ps |
| Statistics Indonesia | Office statistique | 1 | 43 | refuse le client (401/403) | https://www.bps.go.id/ |
| Banco Central del Ecuador | Banque centrale | 1 | 42 | erreur réseau | https://www.bce.fin.ec |
| Institutul National de Statistica | Office statistique | 1 | 42 | erreur HTTP 503 | http://www.insse.ro |
| ANSD, Senegal | Office statistique | 1 | 39 | erreur TLS | http://www.ansd.sn |
| Instituto Nacional De Estatistica, Mozambique | Office statistique | 1 | 36 | erreur TLS | https://www.ine.gov.mz/ |
| Uganda Bureau of Statistics | Office statistique | 1 | 36 | erreur TLS | https://www.ubos.org/ |
| International Monetary Fund | Organisation internationale | 26 | 35 | refuse le client (401/403) | https://www.imf.org/en/Home |
| Kenya National Bureau of Statistics | Office statistique | 1 | 35 | erreur TLS | https://www.knbs.or.ke/ |
| South African Reserve Bank | Banque centrale | 1 | 34 | hôte inconnu | http://www.resbank.co.za |
| Office National des Statistiques (ONS) | Office statistique | 1 | 33 | erreur TLS | http://www.ons.dz/ |
| Banco Central del Uruguay | Banque centrale | 1 | 31 | erreur TLS | https://www.bcu.gub.uy/ |
| Saudi Central Bank | Banque centrale | 1 | 30 | erreur TLS | http://www.sama.gov.sa |
| National Statistics and Information Authority (NSIA) | Office statistique | 1 | 28 | erreur TLS | https://nsia.gov.af/home |
| Banco Central De Venezuela | Banque centrale | 1 | 27 | erreur TLS | http://www.bcv.org.ve/ |
| Department of Economic Planning and Development, Brunei | Ministère / agence | 1 | 27 | erreur HTTP 404 | https://deps.mofe.gov.bn/Theme/Home.aspx |
| Federal Competitiveness and Statistics Centre | Office statistique | 1 | 27 | refuse le client (401/403) | https://fcsc.gov.ae/ |
| Instituto Nacional de Estadística y Censo - Panamá | Office statistique | 1 | 25 | pas de réponse (15 s) | http://www.contraloria.gob.pa |
| Bangko Sentral ng Pilipinas | Banque centrale | 1 | 24 | refuse le client (401/403) | http://www.bsp.gov.ph |
| National Statistical Office of Malawi | Office statistique | 1 | 24 | hôte inconnu | http://www.nsomalawi.mw/ |
| Bank of Burundi | Banque centrale | 1 | 23 | hôte inconnu | http://www.brb.bi |
| Bank of the Lao PDR | Banque centrale | 1 | 23 | pas de réponse (15 s) | http://www.bol.gov.la |
| Institut National de la Statistique du Cameroun | Office statistique | 1 | 23 | refuse le client (401/403) | http://www.statistics-cameroon.org |
| National Bank of Poland | Banque centrale | 1 | 23 | refuse le client (401/403) | https://www.nbp.pl/ |
| Banco de Portugal | Banque centrale | 1 | 22 | refuse le client (401/403) | https://www.bportugal.pt |
| Bangladesh Bureau of Statistics | Office statistique | 1 | 22 | erreur TLS | http://www.bbs.gov.bd/ |
| Instituto Nacional de Estadística y Censos de Costa Rica | Office statistique | 1 | 22 | refuse le client (401/403) | https://inec.cr/ |
| National Bank of Cambodia | Banque centrale | 1 | 22 | refuse le client (401/403) | https://www.nbc.gov.kh/english/ |
| Bank of Israel | Banque centrale | 1 | 21 | connexion refusée | http://www.boi.org.il |
| Statistical Center of Iran | Office statistique | 1 | 21 | erreur TLS | https://www.amar.org.ir |
| Banco de Cabo Verde | Banque centrale | 1 | 20 | erreur HTTP 503 | http://www.bcv.cv |
| Bank of Greece | Banque centrale | 1 | 20 | refuse le client (401/403) | https://www.bankofgreece.gr |
| Central Bank of Malaysia | Banque centrale | 1 | 19 | refuse le client (401/403) | http://www.bnm.gov.my |
| National Bank of the Kyrgyz Republic | Banque centrale | 1 | 19 | refuse le client (401/403) | http://www.nbkr.kg |
| National Bank of the Republic of Belarus | Banque centrale | 1 | 19 | pas de réponse (15 s) | http://www.nbrb.by |
| Reserve Bank of Australia | Banque centrale | 1 | 19 | refuse le client (401/403) | http://www.rba.gov.au/ |
| Bank of Lithuania | Banque centrale | 1 | 18 | refuse le client (401/403) | https://www.lb.lt/en/ |
| Banque Du Liban | Banque centrale | 1 | 18 | refuse le client (401/403) | http://www.bdl.gov.lb |
| Central Bank of Iran | Banque centrale | 1 | 18 | erreur réseau | http://www.cbi.ir |
| Central Bank of Tunisia | Banque centrale | 1 | 18 | connexion refusée | http://www.bct.gov.tn |
| National Bank of Romania | Banque centrale | 1 | 18 | connexion refusée | http://www.bnr.ro |
| Nesdb, Thailand | Office statistique | 1 | 18 | hôte inconnu | https://www.nesdb.go.th |
| Qatar Central Bank | Banque centrale | 1 | 18 | erreur TLS | http://www.qcb.gov.qa |
| Central Bank of Suriname | Banque centrale | 1 | 17 | refuse le client (401/403) | https://www.cbvs.sr |
| De Nederlandsche Bank | Banque centrale | 1 | 17 | refuse le client (401/403) | https://www.dnb.nl/ |
| Central Bank of the Republic of China (Taiwan) | Banque centrale | 1 | 16 | erreur TLS | http://www.cbc.gov.tw |
| National Bank of the Republic of Macedonia | Banque centrale | 1 | 16 | refuse le client (401/403) | http://www.nbrm.mk |
| Reserve Bank of New Zealand | Banque centrale | 1 | 16 | refuse le client (401/403) | https://www.rbnz.govt.nz |
| Bank of Mauritius | Banque centrale | 1 | 15 | erreur TLS | https://www.bom.mu |
| Central Bank of Oman | Banque centrale | 1 | 15 | erreur TLS | https://cbo.gov.om/ |
| General Direction of Taxes | Ministère / agence | 7 | 15 | pas de réponse (15 s) | https://www.impots.gov.bf/ |
| Ministry of Labor and Social Security | Ministère / agence | 4 | 15 | hôte inconnu | https://www.ailevecalisma.gov.tr/ |
| Royal Monetary Authority of Bhutan | Banque centrale | 1 | 15 | erreur TLS | https://www.rma.org.bt |
| African Development Bank Group | Organisation internationale | 11 | 14 | refuse le client (401/403) | https://www.afdb.org/en |
| Aruba Central Bureau of Statistics | Office statistique | 1 | 14 | erreur HTTP 307 | http://www.cbs.aw |
| General Administration of Customs | Ministère / agence | 1 | 14 | erreur HTTP 412 | http://www.customs.gov.cn |
| Bank of Guyana | Banque centrale | 1 | 13 | pas de réponse (15 s) | http://www.bankofguyana.org.gy |
| Bank of Papua New Guinea | Banque centrale | 1 | 13 | refuse le client (401/403) | https://www.bankpng.gov.pg |
| Central Bank of the United Arab Emirates | Banque centrale | 1 | 13 | refuse le client (401/403) | https://www.centralbank.ae |
| Institut National de la Statistique du Niger | Office statistique | 1 | 13 | erreur HTTP 500 | http://www.stat-niger.org |
| Organization of the Petroleum Exporting Countries | Organisation internationale | 13 | 13 | refuse le client (401/403) | https://www.opec.org/ |
| National Social Security Fund | Ministère / agence | 4 | 12 | pas de site |  |
| Office of the Economic Advisor, India | Ministère / agence | 1 | 12 | erreur TLS | https://eaindustry.nic.in/ |
| U.S. Department of Labor | Ministère / agence | 1 | 12 | refuse le client (401/403) | https://www.bls.gov |
| Central Bank of Uzbekistan | Banque centrale | 1 | 11 | pas de réponse (15 s) | http://www.cbu.uz |
| Bank of Algeria | Banque centrale | 1 | 10 | erreur réseau | http://www.bank-of-algeria.dz |
| Ministry of Economy Trade & Industry (METI) | Ministère / agence | 1 | 10 | refuse le client (401/403) | https://www.meti.go.jp |
| Ministry of Finance, Government of India | Ministère / agence | 1 | 10 | hôte inconnu | https://finmin.nic.in/ |
| State statistical committee of the Republic of Tajikistan | Office statistique | 1 | 10 | erreur HTTP 404 | https://www.stat.tj/tj |
| Danish Central Tax Administration | Ministère / agence | 1 | 9 | refuse le client (401/403) | http://ec.europa.eu |
| Ministry of Finance, R.O.C | Ministère / agence | 1 | 9 | erreur TLS | https://www.mof.gov.tw |
| Ministry of Planning and Economic Development | Ministère / agence | 1 | 9 | erreur réseau | https://mped.gov.eg/ |
| Asian Development Bank | Organisation internationale | 3 | 8 | refuse le client (401/403) | http://www.adb.org |
| Australian Taxation Office | Ministère / agence | 1 | 8 | pas de réponse (15 s) | http://www.humanservices.gov.au |
| Chilean Tax Administration | Ministère / agence | 1 | 8 | pas de site |  |
| Departamento del Trabajo y Recursos Humanos, Puerto Rico | Ministère / agence | 1 | 8 | pas de réponse (15 s) | http://www.mercadolaboral.pr.gov |
| Institut National de la Statistique et de la Démographie (INStaD) | Office statistique | 1 | 8 | erreur HTTP 503 | https://instad.bj/ |
| International Labor Organization (ILO) | Organisation internationale | 8 | 8 | refuse le client (401/403) | https://ilostat.ilo.org/data/ |
| National Administration of the Principality of Liechtenstein | Ministère / agence | 1 | 8 | refuse le client (401/403) | http://www.llv.li/ |
| Albanian Taxation Office | Ministère / agence | 1 | 7 | pas de réponse (15 s) | https://www.tatime.gov.al/eng/ |
| Bureau of Trade and Economic Indices, Ministry of Commerce, Thailand | Ministère / agence | 1 | 7 | refuse le client (401/403) | http://www.price.moc.go.th |
| Canada Revenue Agency | Ministère / agence | 1 | 7 | pas de réponse (15 s) | https://www.canada.ca |
| Central Bank of Madagascar | Banque centrale | 1 | 7 | erreur TLS | https://www.banky-foibe.mg/ |
| General Authority of Zakat and Tax (GAZT) | Ministère / agence | 1 | 7 | hôte inconnu | https://gazt.gov.sa |
| Institut National de la Statistique et de l’Analyse Economique, Benin | Office statistique | 1 | 7 | erreur HTTP 503 | http://www.insae-bj.org |
| US Department of Agriculture | Ministère / agence | 1 | 7 | refuse le client (401/403) | https://www.usda.gov/ |
| Bureau of Internal Revenue | Ministère / agence | 1 | 6 | pas de site |  |
| Central Statistical Organization Iraq (COSIT) | Office statistique | 1 | 6 | erreur HTTP 507 | http://www.cosit.gov.iq/en |
| Federal Reserve Bank of Kansas City | Banque centrale | 1 | 6 | pas de réponse (15 s) | https://kansascityfed.org |
| Government of Malta - Inland Revenue | Ministère / agence | 1 | 6 | refuse le client (401/403) | https://socialsecurity.gov.mt |
| Inland Revenue Department of the Republic of Cyprus | Ministère / agence | 1 | 6 | refuse le client (401/403) | http://ec.europa.eu |
| International Trade Centre | Organisation internationale | 2 | 6 | refuse le client (401/403) | https://intracen.org/ |
| Ministry of Labour and Social Security | Ministère / agence | 2 | 6 | pas de site |  |
| New Zealand Treasury | Ministère / agence | 1 | 6 | refuse le client (401/403) | https://treasury.govt.nz/ |
| Qatar Statistics Authority | Office statistique | 1 | 6 | pas de réponse (15 s) | https://www.psa.gov.qa/en/Pages/default.aspx |
| AFDB | Organisation internationale | 2 | 5 | refuse le client (401/403) | http://www.afdb.org |
| Agency of Statistics of Tajikistan | Office statistique | 1 | 5 | pas de site |  |
| Autoridad de Impugnación Tributaria (AIT), Bolivia | Ministère / agence | 1 | 5 | erreur TLS | https://www.ait.gob.bo/ |
| Banque Nationale de Belgique | Banque centrale | 1 | 5 | erreur réseau | http://stat.nbb.be |
| Central Bank of Mauritania | Banque centrale | 1 | 5 | pas de réponse (15 s) | http://www.bcm.mr |
| Department Of Finance Canada | Ministère / agence | 1 | 5 | hôte inconnu | https://www.fin.gc.ca |
| Hellenic Ministry of Labour, Social Insurance and Social Solidarity | Ministère / agence | 1 | 5 | pas de site |  |
| Inland Revenue Board of Malaysia | Ministère / agence | 1 | 5 | pas de réponse (15 s) | http://www.hasil.gov.my |
| Korean Worker's Compensation & Welfare Service | Ministère / agence | 1 | 5 | erreur HTTP 400 | https://www.kcomwel.or.kr |
| Ministerio de Hacienda y Crédito Público | Ministère / agence | 1 | 5 | pas de réponse (15 s) | http://www.hacienda.gob.ni/ |
| Ministry Of Public Finance, Romania | Ministère / agence | 1 | 5 | pas de site |  |
| Ministry of Economic Affairs, R.O.C | Ministère / agence | 1 | 5 | refuse le client (401/403) | http://www.moea.gov.tw |
| Ministry of Finance of the Republic of Bulgaria | Ministère / agence | 1 | 5 | refuse le client (401/403) | http://www.minfin.bg |
| Ministry of Planning, Egypt | Ministère / agence | 1 | 5 | pas de réponse (15 s) | http://mpmar.gov.eg/ |
| National Institute of Statistics, Venezuela | Office statistique | 1 | 5 | hôte inconnu | http://www.ine.gov.ve/ |
| Republic of Azerbaijan Ministry of Labour and Social Protection of Population | Ministère / agence | 1 | 5 | erreur réseau | https://www.sosial.gov.az/ |
| Social Security Administration | Ministère / agence | 1 | 5 | refuse le client (401/403) | https://www.ssa.gov/ |
| Statistics Guyana | Office statistique | 1 | 5 | pas de réponse (15 s) | http://www.bankofguyana.org.gy |
| Agencia Tributaria, Spain | Ministère / agence | 1 | 4 | erreur TLS | https://www.agenciatributaria.es/ |
| Autoriti Monetari Brunei Darussalam | Banque centrale | 1 | 4 | erreur HTTP 404 | https://www.bdcb.gov.bn/home.aspx |
| Bangladesh Bureau of Statistics (BBS) | Office statistique | 1 | 4 | erreur TLS | https://bbs.gov.bd/ |
| Bureau of Trade and Economic Indices, Thailand | Ministère / agence | 1 | 4 | refuse le client (401/403) | http://www.price.moc.go.th |
| Central Administration for Statistics, Lebanon | Office statistique | 1 | 4 | pas de site |  |
| DRC Tax Authority | Ministère / agence | 1 | 4 | hôte inconnu | http://portal.drc.gov.bt/drc/ |
| Directorate General of Taxation of Costa Rica | Ministère / agence | 1 | 4 | pas de site |  |
| Federal Inland Revenue Service, Nigeria | Ministère / agence | 1 | 4 | pas de réponse (15 s) | https://www.firs.gov.ng/ |
| Federal Treasury Of Russia | Ministère / agence | 1 | 4 | erreur HTTP 503 | https://minfin.gov.ru |
| GSIS, Greece | Ministère / agence | 1 | 4 | refuse le client (401/403) | https://www.gsis.gr/ |
| Indirect Taxation Authority of Bosnia and Herzegovina | Ministère / agence | 1 | 4 | pas de site |  |
| Internal Revenue Commission, Papua New Guinea | Ministère / agence | 1 | 4 | refuse le client (401/403) | http://irc.gov.pg |
| Internal Revenue Service | Ministère / agence | 1 | 4 | refuse le client (401/403) | https://www.taxpolicycenter.org/ |
| Ministere de l'Économie, des Finances, de l'Action et des Comptes publics, France | Ministère / agence | 1 | 4 | refuse le client (401/403) | https://www.economie.gouv.fr/ |
| Ministerio de Economía y Finanzas Públicas | Ministère / agence | 1 | 4 | erreur TLS | https://www.economiayfinanzas.gob.bo/ |
| Ministry Of Finance, Republic of Lebanon | Ministère / agence | 1 | 4 | refuse le client (401/403) | https://www.bdl.gov.lb/index.php |
| Ministry of Economic Affairs (MOEA), Taiwan | Ministère / agence | 1 | 4 | refuse le client (401/403) | http://www.moea.gov.tw |
| Ministry of Economy and Finance | Ministère / agence | 4 | 4 | pas de réponse (15 s) | https://finances.ml/ |
| Ministry of Finance of Ukraine | Ministère / agence | 1 | 4 | refuse le client (401/403) | https://mof.gov.ua/uk |
| Ministry of Finance, Bahamas | Ministère / agence | 1 | 4 | refuse le client (401/403) | https://www.bahamas.gov.bs |
| Ministry of Finance, Qatar | Ministère / agence | 1 | 4 | refuse le client (401/403) | https://www.mof.gov.qa/en/pages/default.aspx |
| Ministry of Labor, Employment, and Social Security | Ministère / agence | 2 | 4 | pas de site |  |
| National Center of Legislation under the President of the Republic of Tajikistan | Ministère / agence | 1 | 4 | pas de réponse (15 s) | http://mmk.tj |
| State tax Service | Ministère / agence | 2 | 4 | pas de site |  |
| Sudan Central Bureau of Statistics | Office statistique | 1 | 4 | hôte inconnu | http://cbs.gov.sd |
| Superintendencia Nacional de Aduanas y de Administración Tributaria | Ministère / agence | 1 | 4 | pas de site |  |
| Afghanistan Revenue Department | Ministère / agence | 1 | 3 | pas de site |  |
| Autoridade Tributária de Moçambique | Ministère / agence | 1 | 3 | pas de site |  |
| Bureau of Labor Statistics | Office statistique | 1 | 3 | refuse le client (401/403) | https://www.bls.gov/home.htm |
| Central Bank of Syria | Banque centrale | 1 | 3 | pas de site |  |
| Central Bank of United Arab Emirates | Banque centrale | 1 | 3 | refuse le client (401/403) | https://www.centralbank.ae |
| Central Bank of Yemen | Banque centrale | 1 | 3 | pas de réponse (15 s) | http://www.centralbank.gov.ye |
| Central Statistics Organization of Afghanistan | Office statistique | 1 | 3 | hôte inconnu | http://cso.gov.af |
| Commisioner of Inland Revenue | Ministère / agence | 1 | 3 | pas de site |  |
| DGO - Direccao Geral do Orcamento, Portugal | Ministère / agence | 1 | 3 | erreur TLS | https://www.dgo.gov.pt/Paginas/default.aspx |
| Department of Income Tax | Ministère / agence | 1 | 3 | pas de site |  |
| Department of Statistics of The Bahamas | Office statistique | 1 | 3 | refuse le client (401/403) | http://www.bahamas.gov.bs/ |
| Direccion General de Impuestos Internos | Ministère / agence | 1 | 3 | pas de site |  |
| Dirección Ejecutiva de Ingresos | Ministère / agence | 1 | 3 | pas de site |  |
| Dirección General Impositiva | Ministère / agence | 1 | 3 | pas de site |  |
| Dirección General de Administración y Finanzas | Ministère / agence | 1 | 3 | pas de site |  |
| Directorate of National Taxes and Customs (DIAN) | Ministère / agence | 1 | 3 | pas de site |  |
| Ethiopian Revenues and Customs Authority | Ministère / agence | 1 | 3 | pas de site |  |
| Federal Administration of Public Revenue | Ministère / agence | 1 | 3 | pas de site |  |
| Federal Administration of Public Revenue (AFIP) | Ministère / agence | 1 | 3 | pas de site |  |
| Federal Board of Revenue, Government of Pakistan | Ministère / agence | 1 | 3 | pas de site |  |
| Fiji National Provident Fund | Ministère / agence | 1 | 3 | pas de site |  |
| General Department of National Taxation | Ministère / agence | 1 | 3 | pas de site |  |
| General Directorate of Revenues | Ministère / agence | 1 | 3 | pas de site |  |
| General Directorate of Revenues, Panama | Ministère / agence | 1 | 3 | pas de site |  |
| INSAE, Benin | Office statistique | 1 | 3 | erreur HTTP 503 | http://www.insae-bj.org |
| Inland Revenue Division | Ministère / agence | 1 | 3 | pas de site |  |
| Institute for Social Insurance | Ministère / agence | 1 | 3 | pas de site |  |
| Instituto Ecuatoriano de Seguridad Social | Ministère / agence | 1 | 3 | refuse le client (401/403) | http://www.igpinfo.com |
| Instituto Guatemalteco de Seguridad Social | Ministère / agence | 1 | 3 | pas de site |  |
| Instituto Venezolano de los Seguros Sociales | Ministère / agence | 1 | 3 | pas de site |  |
| Instituto de Seguros Sociales | Ministère / agence | 1 | 3 | pas de site |  |
| Internal Revenue Commission | Ministère / agence | 1 | 3 | pas de site |  |
| Internal Revenue Department | Ministère / agence | 1 | 3 | pas de site |  |
| International Monetary Fund (IMF) | Organisation internationale | 3 | 3 | refuse le client (401/403) | http://www.imf.org |
| Israel Ministry Of Finance | Ministère / agence | 1 | 3 | refuse le client (401/403) | https://www.gov.il/en/departments/ministry_of_finance/govil-landing-page |
| Israel Ministry of Finance Tax Authority | Ministère / agence | 1 | 3 | pas de site |  |
| La Direction Générale des Impôts, Ministère des Finances | Ministère / agence | 1 | 3 | pas de site |  |
| Labor Office of the Czech Republic | Ministère / agence | 1 | 3 | erreur TLS | https://www.uradprace.cz/uvod |
| Lesotho Revenue Authority | Ministère / agence | 1 | 3 | pas de réponse (15 s) | http://lra.org.ls/ |
| Ministerio de Economía y Finanzas, Uruguay | Ministère / agence | 1 | 3 | connexion refusée | http://deuda.mef.gub.uy |
| Ministerio de Economía, Comercio y Empresa | Ministère / agence | 1 | 3 | erreur TLS | http://www.comercio.gob.es |
| Ministerio de Trabajo y Seguridad Social | Ministère / agence | 1 | 3 | pas de site |  |
| Ministerio de Trabajo y Seguridad Social de Costa Rica | Ministère / agence | 1 | 3 | pas de site |  |
| Ministerio del Poder Popular de Finanzas, SENIAT | Ministère / agence | 1 | 3 | pas de site |  |
| Ministerio di Finansas, Comunicacion, Utilidad y Energia | Ministère / agence | 1 | 3 | pas de site |  |
| Ministry Of Finance, Montenegro | Ministère / agence | 1 | 3 | pas de site |  |
| Ministry for Taxes and Levies of the Republic of Belarus | Ministère / agence | 1 | 3 | pas de site |  |
| Ministry of Commerce | Ministère / agence | 1 | 3 | pas de site |  |
| Ministry of Customs and Revenue, Samoa | Ministère / agence | 1 | 3 | pas de site |  |
| Ministry of Employment and Labor | Ministère / agence | 1 | 3 | pas de site |  |
| Ministry of Employment, Social Affairs and Professional Formation | Ministère / agence | 1 | 3 | pas de site |  |
| Ministry of Finance of the Republic of Belarus | Ministère / agence | 1 | 3 | connexion refusée | http://minfin.gov.by/ |
| Ministry of Finance of the Republic of Kazakhstan | Ministère / agence | 1 | 3 | pas de site |  |
| Ministry of Finance of the Republic of Macedonia | Ministère / agence | 1 | 3 | pas de site |  |
| Ministry of Finance, Swaziland | Ministère / agence | 1 | 3 | pas de réponse (15 s) | https://www.gov.sz |
| Ministry of Labor | Ministère / agence | 1 | 3 | pas de site |  |
| Ministry of Labor and Social Insurance | Ministère / agence | 1 | 3 | pas de site |  |
| Ministry of Labor and Social Protection | Ministère / agence | 1 | 3 | pas de site |  |
| Ministry of Labour and Employment Relations | Ministère / agence | 1 | 3 | pas de site |  |
| Ministry of Labour and Immigration | Ministère / agence | 1 | 3 | pas de site |  |
| Ministry of Social Security, National Solidarity, and Reform Institutions | Ministère / agence | 1 | 3 | pas de réponse (15 s) | https://socialsecurity.govmu.org/socialsecurity/ |
| Ministério da Previdência Social | Ministère / agence | 1 | 3 | pas de réponse (15 s) | http://www.previdencia.gov.br/ |
| National Board of Revenue (NBR), Bangladesh | Ministère / agence | 1 | 3 | pas de site |  |
| National Debt Management Center, Ministry of Finance | Ministère / agence | 1 | 3 | erreur HTTP 404 | https://www.ndmc.gov.sa/en/Pages/default.aspx |
| National Institute of Social Security | Ministère / agence | 1 | 3 | pas de site |  |
| National Institute of State Statistics and Information, Turkmenistan | Office statistique | 1 | 3 | pas de site |  |
| National Insurance Corporation | Ministère / agence | 1 | 3 | pas de site |  |
| National Office of Social Insurance | Ministère / agence | 1 | 3 | pas de site |  |
| National Social Insurance Fund | Ministère / agence | 1 | 3 | pas de site |  |
| National Social Security Office | Ministère / agence | 1 | 3 | pas de site |  |
| National Tax Administration, Ministry of Finance | Ministère / agence | 1 | 3 | pas de site |  |
| Office for Budget Responsibility, UK | Ministère / agence | 1 | 3 | refuse le client (401/403) | https://obr.uk/ |
| Public Revenues and Taxes Department, Ministry of Finance | Ministère / agence | 1 | 3 | pas de site |  |
| Servicio de Administración Tributaria, SHCP | Ministère / agence | 1 | 3 | pas de site |  |
| Servicio de Rentas Internas del Ecuador | Ministère / agence | 1 | 3 | pas de site |  |
| Social Insurance General Office | Ministère / agence | 1 | 3 | pas de site |  |
| Social Security Corporation | Ministère / agence | 1 | 3 | pas de site |  |
| Social Security Fund | Ministère / agence | 1 | 3 | pas de site |  |
| Social Security Organisation (SOCSO) | Ministère / agence | 1 | 3 | pas de réponse (15 s) | https://www.perkeso.gov.my |
| South African Social Security Agency (SASSA) | Ministère / agence | 1 | 3 | pas de site |  |
| Sri Lanka Inland Revenue Department | Ministère / agence | 1 | 3 | connexion refusée | http://www.ird.gov.lk |
| State Revenue Service - Ministry of Finance | Ministère / agence | 1 | 3 | pas de site |  |
| State Tax Committee | Ministère / agence | 1 | 3 | pas de site |  |
| State Tax Inspectorate | Ministère / agence | 1 | 3 | refuse le client (401/403) | http://ec.europa.eu |
| State Tax Inspectorate (STI) | Ministère / agence | 1 | 3 | pas de site |  |
| Sudan Chamber of Taxation | Ministère / agence | 1 | 3 | pas de site |  |
| Superintendence of the Tax Administration | Ministère / agence | 1 | 3 | pas de site |  |
| Tax Administration - Ministry of Finance | Ministère / agence | 1 | 3 | pas de site |  |
| Tax Admministration of Jamaica | Ministère / agence | 1 | 3 | pas de site |  |
| Tax Committee of the Ministry of Finance | Ministère / agence | 1 | 3 | pas de site |  |
| Tax Committee of the Ministry of Finance of the Republic of Kazakhstan | Ministère / agence | 1 | 3 | pas de site |  |
| Tax Service of Republic of Armenia | Ministère / agence | 1 | 3 | pas de site |  |
| Taxation Administration, Ministry of Finance, R.O.C | Ministère / agence | 1 | 3 | pas de site |  |
| Thailand Revenue Department | Ministère / agence | 1 | 3 | pas de site |  |
| The Revenue Department, Government of Thailand | Ministère / agence | 1 | 3 | pas de site |  |
| Yemen Tax Authority | Ministère / agence | 1 | 3 | pas de site |  |
| Zambia Revenue Authority | Ministère / agence | 1 | 3 | erreur TLS | https://www.zra.org.zm/ |
| African Development Bank | Organisation internationale | 1 | 2 | pas de site |  |
| Central Bank of Timor-Leste | Banque centrale | 1 | 2 | pas de réponse (15 s) | http://www.bancocentral.tl |
| Central Statistical Organization Iraq | Office statistique | 1 | 2 | erreur HTTP 507 | https://cosit.gov.iq/ar/ |
| Customs Finland | Ministère / agence | 1 | 2 | refuse le client (401/403) | https://tulli.fi/en/frontpage |
| Customs and Inland Revenue Department, Vanuatu | Ministère / agence | 1 | 2 | hôte inconnu | https://customsinlandrevenue.gov.vu |
| Department of Finance and Administration, Australia | Ministère / agence | 1 | 2 | pas de réponse (15 s) | http://www.financeminister.gov.au |
| Department of Tax Services, Monaco | Ministère / agence | 1 | 2 | pas de site |  |
| Department of Taxes | Ministère / agence | 1 | 2 | pas de site |  |
| Dirección General de Crédito Público, Ministerio de Hacienda | Ministère / agence | 1 | 2 | refuse le client (401/403) | https://www.creditopublico.gob.do/ |
| Employment Agency of Montenegro | Ministère / agence | 1 | 2 | pas de réponse (15 s) | https://www.zzzcg.me/me/ |
| Guam Department of Revenue and Taxation | Ministère / agence | 1 | 2 | pas de site |  |
| INEC, Panama | Office statistique | 1 | 2 | pas de réponse (15 s) | http://www.inec.gob.pa/ |
| Income Tax Services, Isle of Man Government | Ministère / agence | 1 | 2 | pas de site |  |
| Jordan Tax Service | Ministère / agence | 1 | 2 | pas de site |  |
| Junta de Planificación de Puerto Rico | Ministère / agence | 1 | 2 | pas de site |  |
| Lao Statistics Bureau | Office statistique | 1 | 2 | pas de site |  |
| Ministerio de Economia y Finanzas, Uruguay | Ministère / agence | 1 | 2 | pas de site |  |
| Ministerio de Hacienda, Paraguay | Ministère / agence | 1 | 2 | hôte inconnu | http://www.economia.gov.py |
| Ministerul Finantelor Publice | Ministère / agence | 1 | 2 | pas de site |  |
| Ministry of Development Planning and Statistics | Office statistique | 1 | 2 | pas de site |  |
| Ministry of Economic Development | Ministère / agence | 1 | 2 | pas de réponse (15 s) | http://economy.gov.ru |
| Ministry of Finance of Moldova | Ministère / agence | 1 | 2 | pas de site |  |
| Ministry of Finance of Republic of Moldova | Ministère / agence | 1 | 2 | pas de site |  |
| Ministry of Finance of the Republic of Belarus Belarus | Ministère / agence | 1 | 2 | pas de réponse (15 s) | https://www.minfin.gov.by/en/ |
| Ministry of Finance of the Republic of Uzbekistan | Ministère / agence | 1 | 2 | hôte inconnu | https://www.mf.uz/uz/ |
| Ministry of Finance of the Russian Federation | Ministère / agence | 1 | 2 | erreur HTTP 503 | https://www.minfin.ru |
| Ministry of Finance, Cyprus | Ministère / agence | 1 | 2 | refuse le client (401/403) | http://www.mof.gov.cy |
| Ministry of Finance, Government of Chile | Ministère / agence | 1 | 2 | pas de réponse (15 s) | https://hacienda.cl/ |
| Ministry of Finance, Iceland | Ministère / agence | 1 | 2 | pas de site |  |
| Ministry of Finance, Kuwait | Ministère / agence | 1 | 2 | pas de site |  |
| Ministry of Finance, Panama | Ministère / agence | 1 | 2 | erreur réseau | http://www.mef.gob.pa |
| Ministry of Finance, Revenue Division, Brunei | Ministère / agence | 1 | 2 | pas de site |  |
| Ministry of Finance, Syria | Ministère / agence | 1 | 2 | pas de site |  |
| Ministry of Finance, Thailand | Ministère / agence | 1 | 2 | pas de réponse (15 s) | http://www2.mof.go.th/ |
| Ministry of Finance, Tourism and Development | Ministère / agence | 1 | 2 | pas de site |  |
| Ministry of Health and Social Affairs, Sweden | Ministère / agence | 1 | 2 | refuse le client (401/403) | https://www.government.se |
| Ministry of Labour | Ministère / agence | 1 | 2 | hôte inconnu | https://mlsd.gov.sa |
| Ministry of Labour, War Invalids, and Social Affairs | Ministère / agence | 1 | 2 | pas de site |  |
| Ministry of Social Affairs and Health | Ministère / agence | 1 | 2 | pas de site |  |
| Ministry of Taxes | Ministère / agence | 1 | 2 | pas de site |  |
| Ministère du Budget | Ministère / agence | 1 | 2 | refuse le client (401/403) | https://mbudget.gov.gn/ |
| Ministère du Travail, de L'emploi et de la Sécurité Sociale | Ministère / agence | 1 | 2 | refuse le client (401/403) | https://www.pwc.com |
| National Development Council (NDC), Taiwan | Ministère / agence | 1 | 2 | erreur TLS | http://www.ndc.gov.tw |
| National Housing Bank, India | Banque centrale | 1 | 2 | pas de réponse (15 s) | https://residex.nhbonline.org.in/ |
| National Institute of Statistics | Office statistique | 1 | 2 | pas de site |  |
| Nepal Central Bureau of Statistics | Office statistique | 1 | 2 | pas de site |  |
| Office of Travel and Tourism Industries | Ministère / agence | 1 | 2 | erreur TLS | https://travel.trade.gov |
| Philippine Central Bank | Banque centrale | 1 | 2 | refuse le client (401/403) | http://www.bsp.gov.ph |
| Philippine National Statistical Coordination Board | Office statistique | 1 | 2 | pas de site |  |
| Public Revenues and Tax Department, Qatar | Ministère / agence | 1 | 2 | pas de site |  |
| Service Canada | Ministère / agence | 1 | 2 | pas de réponse (15 s) | https://www.canada.ca |
| Social Protection Fund of the Ministry of Labor and Social Protection of the Republic of Belarus | Ministère / agence | 1 | 2 | pas de réponse (15 s) | https://ssf.gov.by/ru |
| Social Service Agency | Ministère / agence | 1 | 2 | refuse le client (401/403) | http://ssa.gov.ge |
| South Africa Government Services | Ministère / agence | 1 | 2 | pas de site |  |
| State Social Insurance Fund Board | Ministère / agence | 1 | 2 | pas de site |  |
| State Social Security Service | Ministère / agence | 1 | 2 | pas de site |  |
| Supreme Council of the Republic of Tajikistan | Ministère / agence | 1 | 2 | hôte inconnu | http://www.mehnat.tj |
| The Central Reserve Bank of El Salvador | Banque centrale | 1 | 2 | pas de site |  |
| The Inland Revenue Department of the Republic of Cyprus | Ministère / agence | 1 | 2 | pas de site |  |
| The Republic of Croatia, Tax Administration | Organisation internationale | 1 | 2 | erreur HTTP 502 | http://siteresources.worldbank.org |
| United Nations Refugee Agency | Organisation internationale | 2 | 2 | pas de réponse (15 s) | http://popstats.unhcr.org |
| Venezuela Central Bank | Banque centrale | 1 | 2 | erreur TLS | http://www.bcv.org.ve/ |
| ADB - Asian Development Bank | Organisation internationale | 1 | 1 | pas de site |  |
| ANOFM, Romania | Ministère / agence | 1 | 1 | pas de site |  |
| Afghanistan Bank | Banque centrale | 1 | 1 | hôte inconnu | http://www.centralbank.gov.af/ |
| Agency for Natural Resources and Energy | Ministère / agence | 1 | 1 | refuse le client (401/403) | https://www.enecho.meti.go.jp |
| Associates for Development Services Limited | Ministère / agence | 1 | 1 | pas de site |  |
| Australian Government Department of Finance | Ministère / agence | 1 | 1 | pas de réponse (15 s) | https://www.finance.gov.au |
| Australian Office of Financial Management (AOFM) | Ministère / agence | 1 | 1 | pas de réponse (15 s) | http://www.aofm.gov.au |
| Autoridade Tributária | Ministère / agence | 1 | 1 | pas de site |  |
| Bahamas Department of Statistics | Office statistique | 1 | 1 | refuse le client (401/403) | http://www.bahamas.gov.bs/ |
| Banco Central de São Tomé e Príncipe | Banque centrale | 1 | 1 | pas de site |  |
| Banco Central de la Republica Argentina | Banque centrale | 1 | 1 | pas de site |  |
| Banco de la Republica | Banque centrale | 1 | 1 | pas de site |  |
| Bangladesh Petroleum Corporation | Ministère / agence | 1 | 1 | erreur TLS | https://bpc.gov.bd/ |
| Banque Centrale de Mauritanie | Banque centrale | 1 | 1 | pas de réponse (15 s) | http://www.bcm.mr |
| Brazilian Institute of Geography and Statistics (IBGE) | Office statistique | 1 | 1 | refuse le client (401/403) | https://www.ibge.gov.br |
| Caisse Nationale de Sécurité Sociale, Benin | Ministère / agence | 1 | 1 | erreur HTTP 500 | https://cnss.bj/ |
| Canada Mortgage And Housing Corporation | Ministère / agence | 1 | 1 | pas de site |  |
| Center for Economic Research and Reforms (CERR) | Ministère / agence | 1 | 1 | erreur HTTP 404 | https://www.cer.uz/uzc/ |
| Central Agency for Public Mobilization and Statistics (CAPMAS) | Office statistique | 1 | 1 | pas de site |  |
| Central Bank of Mauritanita | Banque centrale | 1 | 1 | pas de réponse (15 s) | http://www.bcm.mr |
| Central Bank of South Sudan | Banque centrale | 1 | 1 | pas de site |  |
| Central Bank of the UAE | Banque centrale | 1 | 1 | pas de site |  |
| Central Department of Statistics and Information | Office statistique | 1 | 1 | pas de réponse (15 s) | https://www.stats.gov.sa |
| Chamber of Commerce | Ministère / agence | 1 | 1 | pas de réponse (15 s) | http://portaldocomercio.org.br/ |
| China Real Estate Information Corp | Ministère / agence | 1 | 1 | pas de réponse (15 s) | https://www.cric.com/ |
| Council of Labour Affairs | Ministère / agence | 1 | 1 | erreur TLS | https://english.mol.gov.tw |
| DGEEC | Office statistique | 1 | 1 | pas de réponse (15 s) | http://www.dgeec.gov.py |
| Department of Census and Statistics - Sri Lanka | Office statistique | 1 | 1 | pas de site |  |
| Department of Employment Affairs and Social Protection | Ministère / agence | 1 | 1 | refuse le client (401/403) | http://www.lowpaycommission.ie/ |
| Department of Employment and Labour - South Africa | Ministère / agence | 1 | 1 | pas de réponse (15 s) | https://www.gov.za/ |
| Department of Employment, Ministry of Labour | Ministère / agence | 1 | 1 | pas de réponse (15 s) | https://www.doe.go.th/prd/lmia?page=home |
| Department of Finance & Administration (DOFA) | Ministère / agence | 1 | 1 | pas de site |  |
| Department of Finance and Administration | Ministère / agence | 1 | 1 | pas de site |  |
| Department of Investment Review, MOEA | Ministère / agence | 1 | 1 | refuse le client (401/403) | https://www.moea.gov.tw/Mns/dir_e/home/Home.aspx |
| Department of Labor | Ministère / agence | 1 | 1 | refuse le client (401/403) | http://www.dol.gov |
| Department of Labor and Employment | Ministère / agence | 1 | 1 | refuse le client (401/403) | https://www.dole.gov.ph |
| Department of Revenue and Customs | Ministère / agence | 1 | 1 | pas de site |  |
| Department of Tourism, Philippines | Ministère / agence | 1 | 1 | refuse le client (401/403) | https://www.tourism.gov.ph/dot/ |
| Digestyc, El Salvador | Office statistique | 1 | 1 | hôte inconnu | http://www.digestyc.gob.sv/index.php |
| Dirección General de Estadística, Encuestas y Censos | Office statistique | 1 | 1 | pas de réponse (15 s) | http://www.dgeec.gov.py |
| Dirección Reguladora de Hidrocarburos y Minas | Ministère / agence | 1 | 1 | erreur TLS | https://www.dgehm.gob.sv/ |
| Direction Générale des Impôts, Morocco | Ministère / agence | 1 | 1 | pas de site |  |
| Direcção-Geral de Energia e Geologia, Portugal | Ministère / agence | 1 | 1 | hôte inconnu | http://www.precoscombustiveis.dgeg.pt/ |
| EFKA, Greece | Ministère / agence | 1 | 1 | pas de site |  |
| Economic and Financial Committee | Ministère / agence | 1 | 1 | pas de site |  |
| Employment Ministry | Ministère / agence | 1 | 1 | pas de site |  |
| Energy Department of Republic of South Africa | Ministère / agence | 1 | 1 | erreur TLS | http://www.energy.gov.za |
| Energy and Petroleum Regulatory Authority | Ministère / agence | 1 | 1 | pas de réponse (15 s) | https://www.epra.go.ke/ |
| Fair Work Commission | Ministère / agence | 1 | 1 | refuse le client (401/403) | https://www.fwc.gov.au |
| Federal Reserve Bank of St. Louis | Banque centrale | 1 | 1 | pas de réponse (15 s) | https://fred.stlouisfed.org/ |
| Finance Department, San Marino | Ministère / agence | 1 | 1 | refuse le client (401/403) | https://sanmarinoca.gov/government/departments/finance/index.php |
| Finance Ministry of the Republic of Moldova | Ministère / agence | 1 | 1 | pas de site |  |
| Financial Service Bureau | Ministère / agence | 1 | 1 | pas de site |  |
| General Administration of Customs of the People’s Republic of China | Ministère / agence | 1 | 1 | erreur HTTP 412 | http://www.customs.gov.cn/ |
| General Commission for Taxes | Ministère / agence | 1 | 1 | pas de site |  |
| General Department of Taxation Cambodia | Ministère / agence | 1 | 1 | pas de site |  |
| General Organization for Social Insurance, Bahrain | Ministère / agence | 1 | 1 | erreur TLS | http://www.sio.bh |
| Georgia Department of Revenue | Ministère / agence | 1 | 1 | pas de site |  |
| Government Debt Management Agency Ltd, Hungary | Ministère / agence | 1 | 1 | pas de site |  |
| Government Development Bank for Puerto Rico | Banque centrale | 1 | 1 | pas de réponse (15 s) | http://www.gdb-pur.com |
| INEC | Office statistique | 1 | 1 | pas de site |  |
| INEGI - Instituto Nacional de Estadistica, Geografia e Informatica, Mexico | Office statistique | 1 | 1 | pas de réponse (15 s) | http://www.inegi.org.mx/ |
| Immigration, Refugees and Citizenship Canada | Ministère / agence | 1 | 1 | pas de réponse (15 s) | https://www.canada.ca/en.html |
| Indian Oil Corporation Ltd | Ministère / agence | 1 | 1 | erreur HTTP 307 | https://iocl.com/home.aspx |
| Inland Revenue Authority | Ministère / agence | 1 | 1 | pas de site |  |
| Institut Haïtien de Statistique et d'Informatique | Office statistique | 1 | 1 | pas de site |  |
| Institut National de la Statistique et de la Demographie (INStad) | Office statistique | 1 | 1 | erreur HTTP 503 | https://instad.bj/ |
| Instituto Chileno de Administración Racional de Empresas | Ministère / agence | 1 | 1 | erreur HTTP 307 | https://www.icare.cl |
| Ivey Business School | Ministère / agence | 1 | 1 | pas de site |  |
| Japan Real Estate Institute (JREI) | Ministère / agence | 1 | 1 | pas de réponse (15 s) | https://www.reinet.or.jp/ |
| Labour Bureau, Government of India | Ministère / agence | 1 | 1 | pas de site |  |
| Libyan Tax Authority | Ministère / agence | 1 | 1 | pas de site |  |
| Lithuanian Labour Exchange | Ministère / agence | 1 | 1 | erreur réseau | http://www.ldb.lt |
| Malta National Statistics Office | Office statistique | 1 | 1 | refuse le client (401/403) | http://www.nso.gov.mt |
| Minister of Economy, Finance, & Planning | Ministère / agence | 1 | 1 | pas de site |  |
| Ministerio das Financas de Angola | Ministère / agence | 1 | 1 | pas de site |  |
| Ministerio de Finanzas Públicas, Guatemala | Ministère / agence | 1 | 1 | refuse le client (401/403) | http://www.minfin.gob.gt |
| Ministerio de Fomento | Ministère / agence | 1 | 1 | refuse le client (401/403) | https://www.fomento.gob.es/ |
| Ministerio de Trabajo y Desarrollo Laboral | Ministère / agence | 1 | 1 | refuse le client (401/403) | https://www.mitradel.gob.pa/ |
| Ministerio de Transportes, Movilidad y Agenda Urbana | Ministère / agence | 1 | 1 | refuse le client (401/403) | https://www.mitma.gob.es/ |
| Ministerio de Turismo y Deportes Argentina | Ministère / agence | 1 | 1 | hôte inconnu | https://www.yvera.tur.ar/ |
| Ministerio del Poder Popular de Petróleo y Mineria | Ministère / agence | 1 | 1 | erreur TLS | http://www.minpet.gob.ve |
| Ministerio del Trabajo Nicaragua | Ministère / agence | 1 | 1 | pas de réponse (15 s) | http://www.mitrab.gob.ni/ |
| Ministry of Administrative Development, Labour and Social Affairs | Ministère / agence | 1 | 1 | pas de site |  |
| Ministry of Business, Innovation & Employment | Ministère / agence | 1 | 1 | pas de site |  |
| Ministry of Business, Innovation and Employment | Ministère / agence | 1 | 1 | pas de site |  |
| Ministry of Commerce, Cambodia | Ministère / agence | 1 | 1 | refuse le client (401/403) | http://www.moc.gov.kh |
| Ministry of Domestic Trade, Cooperatives and Consumerism | Ministère / agence | 1 | 1 | pas de site |  |
| Ministry of Economic Affairs and Finance | Ministère / agence | 1 | 1 | pas de site |  |
| Ministry of Economic Affairs, Taiwan | Ministère / agence | 1 | 1 | refuse le client (401/403) | https://www.moea.gov.tw/Mns/dos_e/home/Home.aspx |
| Ministry of Economic Inclusion, Small Business, Employment and Skills, Morocco | Ministère / agence | 1 | 1 | pas de réponse (15 s) | https://miepeec.gov.ma/ |
| Ministry of Economy of the Republic of Belarus | Ministère / agence | 1 | 1 | connexion refusée | http://www.minfin.gov.by |
| Ministry of Employment and Labor Relations of the Republic of Uzbekistan | Ministère / agence | 1 | 1 | pas de réponse (15 s) | https://mehnat.uz/uz |
| Ministry of Employment and the Economy, Finland | Ministère / agence | 1 | 1 | refuse le client (401/403) | https://tem.fi |
| Ministry of Employment, Labour Productivity and Skills Development | Ministère / agence | 1 | 1 | erreur TLS | https://www.gov.bw/ |
| Ministry of Energy & Infrastructure, UAE | Ministère / agence | 1 | 1 | pas de réponse (15 s) | https://www.moei.gov.ae/ |
| Ministry of Finance & Treasury, Solomon Islands | Ministère / agence | 1 | 1 | erreur HTTP 307 | https://solomons.gov.sb/ministry-of-finance-and-treasury/ |
| Ministry of Finance Russia | Ministère / agence | 1 | 1 | erreur HTTP 503 | https://minfin.gov.ru/ |
| Ministry of Finance and National Planning | Ministère / agence | 1 | 1 | pas de site |  |
| Ministry of Finance and Planning | Ministère / agence | 1 | 1 | pas de site |  |
| Ministry of Finance and Public Credit Republic of Colombia | Ministère / agence | 1 | 1 | pas de site |  |
| Ministry of Finance of Republic of Kazakhstan | Ministère / agence | 1 | 1 | pas de site |  |
| Ministry of Finance of the Kyrgyz Republic | Ministère / agence | 1 | 1 | erreur TLS | http://www.minfin.gov.kg/ |
| Ministry of Finance of the Republic of Armenia | Ministère / agence | 1 | 1 | erreur TLS | https://www.minfin.am/en/ |
| Ministry of Finance, Customs Administration (Taiwan Customs Administration) | Ministère / agence | 1 | 1 | erreur TLS | https://web.customs.gov.tw/en/ |
| Ministry of Finance, Lebanon | Ministère / agence | 1 | 1 | pas de site |  |
| Ministry of Human Resources Malaysia | Ministère / agence | 1 | 1 | pas de réponse (15 s) | https://www.mohr.gov.my |
| Ministry of Justice, South Korea | Ministère / agence | 1 | 1 | erreur HTTP 307 | https://www.moj.go.kr/moj/index.do |
| Ministry of Labor & Skills, Ethiopia | Ministère / agence | 1 | 1 | pas de réponse (15 s) | https://mols.gov.et/ |
| Ministry of Labor and Social Welfare, Laos | Ministère / agence | 1 | 1 | hôte inconnu | https://www.molsw.gov.la/ |
| Ministry of Labour and Employment, India | Ministère / agence | 1 | 1 | pas de site |  |
| Ministry of Labour and Social Development | Ministère / agence | 1 | 1 | pas de site |  |
| Ministry of Labour and Social Policy of the Republic of Bulgaria | Ministère / agence | 1 | 1 | pas de site |  |
| Ministry of Labour and Social Protection Republic of Belarus | Ministère / agence | 1 | 1 | pas de réponse (15 s) | http://www.mintrud.gov.by |
| Ministry of Labour, Jordan | Ministère / agence | 1 | 1 | erreur réseau | https://www.mol.gov.jo/Default/En |
| Ministry of Labour, Vietnam | Ministère / agence | 1 | 1 | erreur réseau | http://english.molisa.gov.vn |
| Ministry of Manpower and Transmigration | Ministère / agence | 1 | 1 | pas de site |  |
| Ministry of National Infrastructures, Energy and Water Resources | Ministère / agence | 1 | 1 | refuse le client (401/403) | https://www.gov.il/he/departments/ministry_of_energy |
| Ministry of National Planning and Economic Development, Myanmar | Ministère / agence | 1 | 1 | pas de site |  |
| Ministry of Petroleum | Ministère / agence | 1 | 1 | connexion refusée | http://www.petroleum.gov.eg |
| Ministry of Planning and Finance, Government of Venezuela | Ministère / agence | 1 | 1 | pas de site |  |
| Ministry of Production of Peru | Ministère / agence | 1 | 1 | pas de site |  |
| Ministry of Revenue & Customs | Ministère / agence | 1 | 1 | erreur TLS | https://www.revenue.gov.to/ |
| Ministry of Tourism, Brazil | Ministère / agence | 1 | 1 | erreur HTTP 502 | https://dados.turismo.gov.br/ |
| Ministry of Tourism, Morocco | Ministère / agence | 1 | 1 | pas de site |  |
| Ministry of Transportation, R.O.C | Ministère / agence | 1 | 1 | erreur TLS | https://www.motc.gov.tw/en/index |
| Ministry of taxes of the Republic of Azerbaijan | Ministère / agence | 1 | 1 | pas de site |  |
| Ministry of the Economy & Finance | Ministère / agence | 1 | 1 | pas de site |  |
| Ministère des Affaires Sociales et du Travail, Haiti | Ministère / agence | 1 | 1 | pas de site |  |
| Ministère du Travail de l'Emploi et de la Sécurité Sociale, Algeria | Ministère / agence | 1 | 1 | pas de réponse (15 s) | https://www.mtess.gov.dz/fr/ |
| Ministère du Travail, de l'Emploi, de la Fonction Publique et des Lois Sociales, Madagascar | Ministère / agence | 1 | 1 | hôte inconnu | http://www.mfptls.gov.mg/ |
| Ministério da Administração Publica Trabalho e Segurança Social, Angola | Ministère / agence | 1 | 1 | pas de site |  |
| Ministério do Trabalho e Emprego, MTE | Ministère / agence | 1 | 1 | pas de réponse (15 s) | http://trabalho.gov.br |
| Minstry of Labour and Social Policy, Bulgaria | Ministère / agence | 1 | 1 | pas de site |  |
| National Bank of the Republic of North Macedonia | Banque centrale | 1 | 1 | refuse le client (401/403) | http://www.nbrm.mk |
| National Bureau for Taxation, Bahrain | Office statistique | 1 | 1 | pas de site |  |
| National Federation of Independent Business | Ministère / agence | 1 | 1 | refuse le client (401/403) | http://www.nfib.com |
| National Information System | Ministère / agence | 1 | 1 | pas de site |  |
| National Institute of Statistics of Uruguay | Office statistique | 1 | 1 | pas de site |  |
| National Institute of Statistics, Economic and Demographic Studies of Chad (INSEED) | Office statistique | 1 | 1 | pas de site |  |
| National Iranian Oil Products Distribution Company | Ministère / agence | 1 | 1 | pas de réponse (15 s) | https://www.niopdc.ir/ |
| National Minimum Wage Committee of Myanmar | Ministère / agence | 1 | 1 | pas de site |  |
| National Oil and Gas Authority | Ministère / agence | 1 | 1 | pas de réponse (15 s) | http://noga.gov.bh/moo/default.aspx |
| National Revenue Authority | Ministère / agence | 1 | 1 | erreur TLS | https://www.nra.gov.sl/ |
| National Statistical Coordination Board (NSCB), Philippines | Office statistique | 1 | 1 | pas de site |  |
| National Statistics Office | Office statistique | 1 | 1 | refuse le client (401/403) | https://nso.gov.mt/ |
| National Statistics Office of Malawi | Office statistique | 1 | 1 | hôte inconnu | http://www.nsomalawi.mw |
| Nepal Central Bank | Banque centrale | 1 | 1 | pas de site |  |
| North Korea Statistics | Office statistique | 1 | 1 | pas de site |  |
| North Korean Economy Watch | Ministère / agence | 1 | 1 | refuse le client (401/403) | http://www.nkeconwatch.com/ |
| ONS, Algerie | Office statistique | 1 | 1 | pas de site |  |
| OPEC | Organisation internationale | 1 | 1 | refuse le client (401/403) | https://www.opec.org/opec_web/en/ |
| Office of the Economic Adviser, India | Ministère / agence | 1 | 1 | erreur TLS | https://eaindustry.nic.in/ |
| Oficina Nacional Estadística | Office statistique | 1 | 1 | pas de site |  |
| Oppotus Research | Ministère / agence | 1 | 1 | hôte inconnu | https://www.oppotus.com/ |
| Organisation for Economic Co-operation and Development | Organisation internationale | 1 | 1 | refuse le client (401/403) | https://www.oecd.org/ |
| Organismo Supervisor de la Inversión en Energía y Minería, Peru | Ministère / agence | 1 | 1 | pas de réponse (15 s) | https://www.osinergmin.gob.pe/empresas/hidrocarburos |
| Pakistan State Oil | Ministère / agence | 1 | 1 | pas de site |  |
| Papua New Guinea National Statistics Office | Office statistique | 1 | 1 | pas de site |  |
| Peking University Institute of Social Sciences | Ministère / agence | 1 | 1 | pas de site |  |
| Petroleum Products Pricing Regulatory Agency | Ministère / agence | 1 | 1 | pas de site |  |
| Puerto Rican Department of Treasury | Ministère / agence | 1 | 1 | pas de site |  |
| Qatar Planning and Statistics Authority | Office statistique | 1 | 1 | erreur TLS | https://www.npc.qa/en/Pages/default.aspx |
| Rating and Valuation Department Hong Kong Government | Ministère / agence | 1 | 1 | pas de site |  |
| Retail Fuel Prices Observatory, Cyprus Government | Ministère / agence | 1 | 1 | erreur TLS | https://mobile.eservices.cyprus.gov.cy |
| Secretariat for Economy and Finance | Ministère / agence | 1 | 1 | pas de site |  |
| Secretaría de Trabajo, Empleo y Seguridad Social, Ministerio de Capital Humano | Ministère / agence | 1 | 1 | pas de site |  |
| Servicio de Impuestos Nacionales | Ministère / agence | 1 | 1 | pas de site |  |
| Shanghai Port | Ministère / agence | 1 | 1 | erreur HTTP 521 | https://www.portshanghai.com.cn/ |
| Singapore Institute of Purchasing & Materials Management, SIPMM | Ministère / agence | 1 | 1 | pas de site |  |
| Social Insurance Organization | Ministère / agence | 1 | 1 | refuse le client (401/403) | https://www.sio.gov.bh/ |
| Social Insurance and Pension Agency, Tajikistan | Ministère / agence | 1 | 1 | pas de réponse (15 s) | https://nafaka.tj/en |
| State Statistical Committee, Azerbaijan | Office statistique | 1 | 1 | erreur HTTP 302 | http://www.stat.gov.az |
| Statistical Centre of Iran | Office statistique | 1 | 1 | erreur TLS | https://www.amar.org.ir/ |
| Statistical Service, Ministry of Finance of Republic of Cyprus | Office statistique | 1 | 1 | refuse le client (401/403) | http://www.mof.gov.cy |
| Swaziland Revenue Authority | Ministère / agence | 1 | 1 | erreur TLS | https://www.ers.org.sz/ |
| Tariff (price) Council of Azerbaijan Republic | Ministère / agence | 1 | 1 | erreur HTTP 302 | http://www.tariffcouncil.gov.az |
| Tax Committee Ministry of Finance of the Republic of Kazakhstan | Ministère / agence | 1 | 1 | pas de réponse (15 s) | http://kgd.gov.kz/ru |
| Taxation and Revenue | Ministère / agence | 1 | 1 | pas de site |  |
| The Federal Service of Labour and Employment | Ministère / agence | 1 | 1 | pas de site |  |
| The Government of Bahamas | Ministère / agence | 1 | 1 | refuse le client (401/403) | https://www.bahamas.gov.bs/ |
| The Government of Cape Verde | Ministère / agence | 1 | 1 | pas de site |  |
| The Hashemite Kingdom of Jordan Department of Statistics | Office statistique | 1 | 1 | pas de site |  |
| The Mauritius Chamber of Commerce and Industry | Ministère / agence | 1 | 1 | pas de site |  |
| The Ministry of Oil and Gas, Sudan | Ministère / agence | 1 | 1 | hôte inconnu | http://www.mop.gov.sd/ |
| The ministry of economy of the Kyrgyz Republic | Ministère / agence | 1 | 1 | pas de réponse (15 s) | http://mineconom.gov.kg/en |
| Treasury | Ministère / agence | 1 | 1 | pas de site |  |
| UK Finance | Ministère / agence | 1 | 1 | pas de site |  |
| Unified Federal Register of Information on Bankruptcy | Ministère / agence | 1 | 1 | pas de site |  |
| United States International Trade Commission | Ministère / agence | 1 | 1 | refuse le client (401/403) | https://www.usitc.gov/ |
| https://www.bundesbank.de/en | Banque centrale | 1 | 1 | pas de site |  |

#### Sources gratuites avec un accès programmatique documenté

| Source | API | Test |
|---|---|---|
| World Bank | Indicators API (JSON/CSV), no key | répond |
| EUROSTAT | REST API: JSON-stat and SDMX, no key | répond |
| OECD | OECD SDMX API, no key | refuse le client (401/403) |
| Office for National Statistics | ONS API, no key | répond |
| Statistics Canada | Web Data Service (JSON), no key | répond |
| Australian Bureau of Statistics | ABS Data API (SDMX), no key | répond |
| Statistics Norway | SSB PxWeb API, no key | répond |
| European Central Bank | ECB Data Portal API (SDMX), no key | répond |
| Central Statistics Office Ireland | PxStat API, no key | répond |
| Statistics Sweden | SCB PxWeb API, no key | répond |
| National Bureau of Statistics of China | data.stats.gov.cn easyquery (JSON), no key | répond |
| National Institute of Statistics (ISTAT) | SDMX API (esploradati), no key | répond |
| Statistics Denmark | StatBank API, no key | répond |
| Statistics New Zealand | Infoshare and Aotearoa Data Explorer (CSV), no key | répond |
| Statistics Finland | PxWeb API, no key | répond |
| Bank for International Settlements | BIS SDMX API, no key | répond |
| Statistics Portugal | INE API (JSON), no key | pas de réponse (15 s) |
| U.S. Energy Information Administration | EIA API v2, free key | répond |
| Statistics Netherlands | CBS OData API, no key | répond |
| Federal Statistical Office | GENESIS-Online API, free account | répond |
| U.S. Bureau of Labor Statistics | BLS API v1 (no key, 25 requests a day) and v2 (free key) | refuse le client (401/403) |
| INSEE, France | API Melodi (SDMX-JSON), no key | répond |
| Central Bureau of Statistics, Israel | CBS OData API, no key | répond |
| Instituto Nacional de Estadística y Geografía (INEGI) | INEGI API, free key | répond |
| Turkish Statistical Institute | web tables (XLS) | répond |
| Statistics South Africa | web tables (XLSX) | répond |
| Central Statistics Office, Mauritius | PxStat API, no key | répond |
| National Statistics Institute (INE) | INEbase JSON API, no key | répond |
| Statistics Singapore | SingStat Table Builder API, no key | répond |
| Census and Statistics Department, Hong Kong | C&SD web API, no key | répond |
| Statistics Estonia | PxWeb API, no key | répond |
| Federal State Statistics Service | web tables | pas de réponse (15 s) |
| Philippine Statistics Authority | OpenSTAT, no key | refuse le client (401/403) |
| The Bank of Korea | ECOS API, free key | répond |
| Ministry of Statistics and Programme Implementation (MOSPI) | MOSPI eSankhyiki (CSV), no key | répond |
| Statistics Austria | STATcube (CSV), no key | répond |
| United Nations Economic Commission for Europe (UNECE) | UN Comtrade API, free key (limited); UNdata, no key | répond |
| Central Statistical Office of Poland (GUS) | BDL API, free key | répond |
| U.S. Census Bureau | Census Data API, no key for light use | répond |
| Instituto Brasileiro de Geografia e Estatística (IBGE) | SIDRA API (JSON), no key | refuse le client (401/403) |
| Bank of Thailand | BOT API, free key | répond |
| IMF | IMF SDMX 3.0 API, no key | refuse le client (401/403) |
| National Bank of Belgium | NBB.Stat (CSV), no key | répond |
| Palestinian Central Bureau of Statistics | web tables | erreur TLS |
| Statistics Indonesia | BPS Web API, free key | refuse le client (401/403) |
| Central Bureau of Statistics, Namibia | web tables | répond |
| Swiss Federal Statistical Office | PxWeb API, no key | répond |
| U.S. Bureau of Economic Analysis | BEA API, free key | répond |
| INE, Cape Verde | INEbase JSON API, no key | répond |
| International Monetary Fund | IMF SDMX 3.0 API, no key | refuse le client (401/403) |
| South African Reserve Bank | web API (JSON), no key | hôte inconnu |
| Statistics Korea | KOSIS API, free key | répond |
| Banco Central do Brasil | SGS API (JSON), no key | répond |
| Bank of Japan | Time-Series Data Search (CSV), no key | répond |
| Statistics Belgium | open data (CSV), no key | répond |
| Central Bank of the Republic of Turkey | EVDS API, free key | répond |
| Central Statistics Office, Botswana | PxStat API, no key | répond |
| Reserve Bank of India | DBIE (CSV), no key | répond |
| Central Bank of Seychelles | CBS OData API, no key | répond |
| Deutsche Bundesbank | SDMX REST API, no key | répond |
| Statistics Bureau of Japan | e-Stat API, free key | répond |
| Bank of England | Statistical Interactive Database (CSV), no key | répond |
| People's Bank of China | web tables | répond |
| Banco de México | SIE API, free key | répond |
| Federal Reserve Bank of New York | Data Download Program (CSV, no key); FRED API, free key | répond |
| Bank of Israel | web API (JSON), no key | connexion refusée |
| Federal Reserve | Data Download Program (CSV, no key); FRED API, free key | répond |
| Reserve Bank of Australia | statistical tables (XLSX), no key | refuse le client (401/403) |
| Swiss National Bank | SNB data portal API (JSON/CSV), no key | répond |
| Banque de France | Webstat API, free key | répond |
| De Nederlandsche Bank | DNB statistics API, no key | refuse le client (401/403) |
| Reserve Bank of New Zealand | statistical tables (XLSX), no key | refuse le client (401/403) |
| Aruba Central Bureau of Statistics | CBS OData API, no key | erreur HTTP 307 |
| Central Bureau of Statistics, Nepal | web tables | répond |
| General Administration of Customs | web tables | erreur HTTP 412 |
| Bank of Korea | ECOS API, free key | répond |
| Ministry of Internal Affairs & Communications | e-Stat API, free key | répond |
| Monetary Authority of Singapore | MAS API, no key | répond |
| Central Bureau Of Statistics, Syrian Arab Republic | web tables | répond |
| Sveriges Riksbank | Riksbank API, no key | répond |
| U.S. Department of the Treasury | Fiscal Data API and daily yield curve CSV, no key | répond |
| Bank of Canada | Valet API (JSON), no key | répond |
| Federal Reserve Bank of Dallas | Data Download Program (CSV, no key); FRED API, free key | répond |
| U.S. Bureau of Economic Analysis (BEA) | BEA API, free key | répond |
| Federal Reserve Bank of Chicago | Data Download Program (CSV, no key); FRED API, free key | répond |
| Federal Reserve Bank of Philadelphia | Data Download Program (CSV, no key); FRED API, free key | répond |
| Norges Bank | Norges Bank API, no key | répond |
| Federal Reserve Bank of Kansas City | Data Download Program (CSV, no key); FRED API, free key | pas de réponse (15 s) |
| Food and Agriculture Organization of the United Nations | UN Comtrade API, free key (limited); UNdata, no key | répond |
| Central Statistical Office, Trinidad and Tobago | PxStat API, no key | répond |
| Institut National de la Statistique du Tchad (INSEED) | API Melodi (SDMX-JSON), no key | répond |
| Sudan Central Bureau of Statistics | CBS OData API, no key | hôte inconnu |
| Banco de Mexico | SIE API, free key | répond |
| Bureau of Labor Statistics | BLS API v1 (no key, 25 requests a day) and v2 (free key) | refuse le client (401/403) |
| Central Statistics Office, Yemen | PxStat API, no key | répond |
| Central Statistics Organization of Afghanistan | PxStat API, no key | hôte inconnu |
| Federal Reserve Bank of Richmond | Data Download Program (CSV, no key); FRED API, free key | répond |
| Instituto Nacional de Estadistica, Geografia e Informatica, Mexico | INEGI API, free key | répond |
| International Monetary Fund (IMF) | IMF SDMX 3.0 API, no key | refuse le client (401/403) |
| National Statistical Office of Thailand | web tables | répond |
| Statistics Netherlands - CBS | CBS OData API, no key | répond |
| Treasury Department, Ministry of Finance and Economy | Fiscal Data API and daily yield curve CSV, no key | répond |
| Bank of Mexico | SIE API, free key | répond |
| BelgoStat | open data (CSV), no key | répond |
| Central Statistical Office | PxStat API, no key | répond |
| Central Statistics Office | PxStat API, no key | répond |
| Federal Reserve Bank of Cleveland | Data Download Program (CSV, no key); FRED API, free key | répond |
| Institut National de la Statistique, des Etudes Economiques et Démographiques (INSEED), Tchad | API Melodi (SDMX-JSON), no key | répond |
| Nepal Central Bureau of Statistics | web tables | pas de site |
| U.S. Treasury | Fiscal Data API and daily yield curve CSV, no key | répond |
| United Nations Refugee Agency | UN Comtrade API, free key (limited); UNdata, no key | pas de réponse (15 s) |
| Brazilian Institute of Geography and Statistics (IBGE) | SIDRA API (JSON), no key | refuse le client (401/403) |
| Bureau of Economic Analysis (BEA) | BEA API, free key | répond |
| Central Bank of Brazil | SGS API (JSON), no key | répond |
| Department of the Treasury of Puerto Rico | Fiscal Data API and daily yield curve CSV, no key | répond |
| Federal Reserve Bank of Atlanta | Data Download Program (CSV, no key); FRED API, free key | répond |
| Federal Reserve Bank of San Francisco | Data Download Program (CSV, no key); FRED API, free key | répond |
| Federal Reserve Bank of St. Louis | FRED API, free key | pas de réponse (15 s) |
| General Administration of Customs of the People’s Republic of China | web tables | erreur HTTP 412 |
| INE - Instituto Nacional de Estatistica de Angola | INEbase JSON API, no key | répond |
| INE - Instituto Nicaragüense de Energía | INEbase JSON API, no key | répond |
| INE Honduras | INEbase JSON API, no key | répond |
| INE, Sao Tome and Principe | INEbase JSON API, no key | répond |
| INEGI - Instituto Nacional de Estadistica, Geografia e Informatica, Mexico | INEGI API, free key | pas de réponse (15 s) |
| INEGI, Mexico | INEGI API, free key | répond |
| INSEED Comoros | API Melodi (SDMX-JSON), no key | répond |
| Institut National de la Statistique, des Etudes Economiques et Démographiques | API Melodi (SDMX-JSON), no key | répond |
| KOSTAT - Statistics Korea | KOSIS API, free key | répond |
| Kostat | KOSIS API, free key | répond |
| Ministry of Finance, Yemen | PxStat API, no key | répond |
| National Institute of Statistics, Economic and Demographic Studies of Chad (INSEED) | API Melodi (SDMX-JSON), no key | pas de site |
| Organisation for Economic Co-operation and Development | OECD SDMX API, no key | refuse le client (401/403) |
| Singapore Statistics | SingStat Table Builder API, no key | répond |
| Statistics Korea (KOSTAT) | KOSIS API, free key | répond |
| The People's Bank of China | web tables | répond |
| https://www.bundesbank.de/en | SDMX REST API, no key | pas de site |

Lecture : « pas de site » sont les sources que TE nomme sans lien (le nom seul dans la balise) ; elles sont
identifiables mais leur site n'a pas pu être testé. « refuse le client » regroupe les sites qui répondent 401 ou
403 à un client qui n'est pas un navigateur (pare-feu applicatif) : ils fonctionnent dans un navigateur, et le test
n'a pas cherché à se faire passer pour l'un. Les erreurs TLS viennent de certificats expirés ou de chaînes
incomplètes, fréquentes chez les administrations de petits pays. Le bouton **Tester** de l'onglet Sources refait
ce test à la demande, depuis l'application.

Le test porte sur la page d'accueil du site, pas sur son API : le site du BLS refuse le client alors que son API
(`api.bls.gov`) répond, et c'est elle que l'application utilise. Les quatre compilations branchées (OCDE, Eurostat,
FMI, BLS) sont testées en direct, sur leur point d'accès réel, par les lignes « Carte · statistiques officielles » de
l'onglet Sources.
