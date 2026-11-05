# Données et limites

Ce qu'il faut savoir avant de se fier à un chiffre : d'où il vient, sa fraîcheur, et ce qui est estimé.

## Fraîcheur

| Données | Fraîcheur |
|---|---|
| Desk | Prix Yahoo Finance, environ quinze minutes de retard ; positions CFTC du mardi ; le fil a quelques minutes de retard au mieux |
| Journal | Vos relevés, à la date du dernier import ; positions ouvertes aux cours retardés |
| Contrats à terme, cotations, Vue d'ensemble, Research, Quant | Yahoo Finance, avec dix à quinze minutes de retard ; la Vue d'ensemble se rafraîchit chaque minute |
| Graphiques (paires Binance) | En direct, transaction par transaction ; les autres symboles sont interrogés toutes les quelques secondes |
| Navires | Toutes les une à deux minutes par navire ; retiré après 30 minutes de silence |
| Historique des points de passage, ports | PortWatch du FMI, publié chaque semaine avec quelques jours de retard |
| Forex | Le fixing de la BCE, une fois par jour ouvré, pas d'intrajournalier |
| Calendrier économique | Toutes les dix minutes tant qu'il est affiché |
| Chiffres par pays, tableaux de bord | Du mensuel (prix, emploi) à l'annuel (comptes nationaux) ; une compilation peut avoir un trimestre de retard sur l'office national |
| Perspectives de l'économie mondiale du FMI | Deux fois par an ; l'année en cours est une estimation, les suivantes des projections |
| Balance des paiements, flux commerciaux | La dernière année que la plupart des pays ont déposée, environ un an en arrière |

## Estimations et chiffres expérimentaux

- Le cadran **Fear & Greed** du marché actions (Économie › Sentiment) est marqué ESTIMATION : il est reconstruit à partir de séries publiques, pas des chiffres de CNN.
- La **valeur intrinsèque**, les **notes de facteurs** et la **note quant** de Research, et l'**historique des recommandations** reconstruit, sont marqués EXPÉRIMENTAL : hypothèses fixes, non validées contre les prix ultérieurs.
- Les **passages aux points de passage** sont des minimums (seuls les navires reçus par aisstream.io), et certains détroits ne reçoivent aucun rapport.
- Le **graphe de chaîne d'approvisionnement** (Research › SPLC) compte des produits Wikidata, pas du chiffre d'affaires ; la couverture est inégale.
- Le **filtre de stabilité** de la carte de chaleur de liquidité est une approximation : le carnet de Binance est par niveau de prix, pas par ordre.
- Les côtés du **Footprint** sont exacts sur Binance (qui indique l'agresseur) ; le badge « APPROXIMATE SIDES » apparaît là où ils sont estimés.
- Les **Derniers chiffres** d'un pays prennent la source la plus directe ; une série figée est remplacée par une compilation plus fraîche.
- Les **enregistrements dorés** d'Économie fusionnent les sources qui concordent ; l'infobulle dit lesquelles ont été rebasées ou écartées et pourquoi.

## Clés et vie privée

Les requêtes vont directement de l'application à chaque source ; il n'y a ni serveur intermédiaire ni télémétrie. Les clés sont rangées dans les préférences de l'application (celles de Graphiques dans un fichier que vous seul pouvez lire) ; elles ne sont envoyées qu'au service auquel elles appartiennent. Le lecteur de News se présente comme `Deepwater` aux sites qu'il charge. Un modèle local tourne dans un bac à sable sans réseau ni accès aux préférences ; ce qu'une IA lit passe par les outils consignés de son emplacement, et les intégrations Claude et opencode envoient ce que leurs outils lisent à leurs fournisseurs.

## Limites de débit à connaître

FRED autorise 120 requêtes par minute (l'application espace les siennes) ; le Bureau of Labor Statistics américain 25 par jour et par adresse ; l'OCDE environ 20 par minute ; l'aperçu public d'UN Comtrade quelques requêtes par jour sans clé ; PortWatch expire sur les requêtes lourdes, les ports se chargent donc en une douzaine de requêtes. Une source qui échoue le montre dans sa ligne et dans le graphique ou le tableau qu'elle alimente.

## Licences

Deepwater embarque Quant Terminal (MIT), le comportement de Lightweight Charts réimplémenté, PineTS (AGPL-3.0), llama.cpp (MIT), libvterm (MIT), adblock-rust de Brave (MPL-2.0), le réseau maritime marnet d'Eurostat (EUPL-1.2) et UN/LOCODE. Leurs notices sont livrées avec l'application.
