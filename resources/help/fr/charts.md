# Graphiques

La page Graphiques est un terminal de trading : un portage de Quant Terminal (licence MIT) avec les ajouts de Deepwater, dont le flux d'ordres et la carte de chaleur de liquidité. Ses textes restent en anglais quelle que soit la langue. Elle prend le thème de l'application.

## Disposition

- **Les volets de graphique** : un ou plusieurs, chacun avec son symbole, son unité de temps et son type (chandeliers, barres, ligne, aire, Heikin Ashi, Renko, Kagi, point & figure, **Footprint**…). La barre d'outils porte le champ du symbole, l'unité de temps, le type de graphique, **Indicators**, **Compare** (un autre symbole sur le même graphique), **Replay** (rejeu barre par barre avec un curseur), l'export CSV et les réglages. Un fin **curseur de plage** sous chaque volet montre toutes les barres chargées et la fenêtre affichée (plus bas).
- **Le panneau latéral** : **Watchlist**, **Alerts**, **Calendar** et **News**.
- **Le panneau du bas** : **Strategy tester**, **Screener**, **Heatmap**, **Seasonals**, **Financials**, **Paper trading**, **Indicator editor**, **Terminal** et **PTQ**. Tirez son bord supérieur pour le redimensionner.

Le moteur tourne pendant toute la session : les alertes continuent de se déclencher et le compte fictif de s'exécuter pendant que vous êtes sur un autre onglet.

## Données

Les paires crypto viennent de Binance (binance.us là où binance.com est bloqué), les actions, indices, devises et contrats à terme de Yahoo Finance ; vos propres clés **Alpaca** et **Polygon**, saisies dans [Sources](help:sources), ajoutent leurs flux. Quand un fournisseur est coupé dans Sources, la page passe au suivant de sa chaîne et le graphique le dit (« Binance is turned off in Sources, so this chart uses Coinbase »). Les paires Binance reçoivent la bougie en formation d'un flux en direct ; ailleurs le volet interroge les dernières barres toutes les quelques secondes. Des fichiers CSV peuvent être importés comme symboles.

Le terminal sert aussi une API HTTP locale sur `127.0.0.1:3777` pour vos propres scripts, la même que celle de Quant Terminal.

## Indicateurs

87 indicateurs intégrés, séries et transformations de type de graphique, avec leurs réglages et couleurs. Deux sortes d'indicateurs à vous, gardés dans le dossier `custom/` des données Graphiques (`~/Library/Application Support/Deepwater/Deepwater/quant-terminal/`), édités dans l'**Indicator editor** et rechargés quand le fichier change :

- Des indicateurs **JavaScript**, au format de Quant Terminal.
- Des indicateurs **Pine Script** (`//@version=6`, `indicator(...)`, `plot(...)`) : un fichier `.pine` apparaît dans la liste comme un `.js`. Les `input.*` deviennent les réglages de l'indicateur, `plot()` des lignes ou des histogrammes, `plotshape()` et `plotchar()` des marqueurs, `hline()` des niveaux ; `bgcolor`, `fill`, les labels, les tables et les ordres `strategy.*` sont ignorés. Ils tournent sur PineTS, qui demande **Node.js 18 ou plus récent** sur la machine ; sans lui, le fichier est listé avec la raison.

## Curseur de plage

La bande sous chaque volet montre les clôtures de toutes les barres chargées et une fenêtre sur la partie affichée. Glissez dans la fenêtre pour faire défiler au même zoom, cliquez hors d'elle pour la centrer là, tirez un bord pour zoomer (jamais sous dix barres). Faire défiler ou zoomer le graphique déplace aussi la fenêtre. Quand la dernière barre est en vue, le graphique suit les nouvelles barres ; sinon il reste en place.

## Footprint

**Footprint** dans la liste des types de graphique montre, dans chaque bougie, le volume échangé à chaque niveau de prix, séparé selon l'agresseur : **bid** pour les ventes agressives, **ask** pour les achats agressifs. Paires Binance seulement, de 1m à 1h ; Binance indique l'agresseur, les côtés sont donc exacts (un badge le dit quand ils sont estimés pour un autre flux). L'historique est chargé jusqu'à la première barre du graphique ou jusqu'au budget (300 000 transactions par défaut) ; les bougies antérieures à la plus ancienne transaction chargée restent ordinaires.

Par bougie : le delta par niveau et au total, le **POC** (le niveau le plus échangé, encadré), les **déséquilibres diagonaux** (un côté au moins *ratio* fois l'autre côté un niveau en diagonale, marqués d'une barre et de chiffres en gras) et les **déséquilibres empilés** (N ou plus à la suite, tracés comme une zone qui court à droite jusqu'à ce qu'une barre ultérieure y échange). L'engrenage à côté du type de graphique choisit l'affichage — **Bid x Ask**, **Delta** ou **Volume profile** —, la taille des niveaux (**Auto** suit le zoom, ou un nombre fixe de ticks), le ratio de déséquilibre, le volume minimum, N, les couleurs et le budget d'historique. Les chiffres apparaissent quand les niveaux sont assez hauts, des cellules colorées en dessous, des bougies ordinaires quand elles sont trop étroites.

## Big Trades

**Big Trades** dans la liste des indicateurs (groupe Volume) pose une bulle sur le graphique pour chaque gros ordre agressif, à partir des mêmes transactions Binance. Les transactions consécutives du même côté en moins de 50 ms à un prix (ou, avec *Sweep*, à des prix qui vont dans un sens) forment un agrégat ; il est gros quand son volume atteint le seuil : **Manual** (un volume minimum) ou **Auto** (le 99e centile des agrégats des 30 dernières minutes, fois un facteur de volatilité entre 0,5 et 2). La légende montre le seuil et comment il est fait. Les bulles sont au prix moyen pondéré par le volume et à l'instant exact dans la bougie, colorées par côté, leur aire proportionnelle au volume ; survolez-en une pour son heure à la milliseconde, son côté, son volume, son prix moyen, sa plage et ses exécutions. Empilez plusieurs Big Trades avec des tranches min/max pour différentes tailles.

## Carte de chaleur de liquidité

**Liquidity Heatmap** dans la liste des indicateurs (groupe Volume) dessine, sous les bougies, les ordres au repos du carnet Binance de la paire au fil du temps : chaque colonne est le carnet à un instant, chaque cellule la quantité posée à un niveau de prix, plus claire quand elle est plus grande. Paires Binance seulement, au mieux sur des graphiques de 1s à 1m. Le carnet actuel est un **DOM** compact au bord droit (les bids dans la couleur des acheteurs, les asks dans celle des vendeurs). Binance n'a pas d'historique du carnet : la carte se remplit à partir de l'ouverture du graphique, sauf enregistrements (plus bas).

Réglages : l'intervalle d'échantillonnage (250 ms par défaut), l'historique gardé (120 minutes), le regroupement des niveaux de prix, ce qu'un pixel montre en zoom arrière (**Max** : un mur reste visible ; **Mean** : la liquidité stable pèse plus qu'un instant), les couleurs (**Deep**, **Thermal**, **Mono** ou personnalisées ; intensité, contraste et un seuil sous lequel rien n'est dessiné), le DOM, et :

- **Trade bubbles** (actif) : les ordres agressifs par-dessus la liquidité qu'ils ont touchée, les 10 % les plus gros des 30 dernières minutes par défaut.
- **Stability filter** (inactif) : seule la liquidité restée au moins ce temps (500 ms à 10 s). Une approximation, le carnet de Binance étant par niveau de prix, pas par ordre.
- **Pulled and absorbed liquidity** (actif) : une croix orange là où un gros niveau a disparu à l'approche du prix (annulé, pas exécuté), une barre violette là où le meilleur bid ou ask a tenu pendant que des ordres agressifs de sa taille s'y exécutaient, et un losange là où le prix est ensuite parti.
- **Record the order book to disk** : ce que Binance envoie est écrit au fil de l'eau, un fichier par paire et par heure (environ 6 Mo par heure sur BTCUSDT), gardé un nombre de jours. Quand une carte s'ouvre, les enregistrements qui tombent dans son historique sont rejoués d'abord, elle démarre donc avec ce qui a été enregistré ; dans le **Replay** du graphique, elle montre la profondeur enregistrée jusqu'au curseur.
- Une **surimpression de débogage** avec les chiffres du flux, du carnet et du dessin.

## Alertes

L'onglet **Alerts** du panneau latéral crée des alertes de prix et d'indicateur sur n'importe quel symbole ; elles sont vérifiées toutes les 15 secondes et se déclenchent depuis tous les onglets.

## Testeur de stratégie et compte fictif

Le **Strategy tester** backteste une stratégie écrite en règles sur des indicateurs (l'aide de l'éditeur donne le format ; quelques exemples sont intégrés) sur le graphique actif, avec les transactions marquées dessus, les statistiques et un export CSV. **Paper trading** est un compte simulé : ordres au marché, limites et stops avec stop loss et take profit optionnels, exécutions, frais, positions et historique ; le rejeu a un compte d'entraînement à part. Un modèle local avec la compétence *Trade* peut piloter les deux (voir [IA](help:ai)). **PTQ** envoie une stratégie au connecteur PTQ pour l'exécution réelle.

## Terminal

L'onglet **Terminal** du panneau du bas est votre shell dans un pseudo-terminal, comme le terminal intégré d'un éditeur. ``Ctrl+` `` l'ouvre et lui donne le focus, ou masque le panneau quand il l'a déjà ; `+` ouvre une autre session, la corbeille en ferme une ; les sessions continuent de tourner pendant que vous êtes sur un autre onglet. 256 couleurs et couleurs vraies, l'écran alternatif (vim, htop, less), les modes souris, 5 000 lignes de défilement (molette, `Maj+PageUp/PageDown`), sélection à la souris, `⌘C` / `⌘V`, `⌘K` efface, `Option+←/→` saute les mots.
