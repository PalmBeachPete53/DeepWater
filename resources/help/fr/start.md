# Premiers pas

Deepwater est un terminal de bureau pour les marchés de matières premières et l'économie mondiale : un globe avec les navires et les prix en direct, l'économie de 190 pays, des graphiques de trading, un calendrier économique, les news, l'analyse de sociétés, des outils quantitatifs et des modèles d'IA locaux. Tout vient de sources publiques, la plupart sans compte ; quelques clés gratuites en débloquent davantage (voir plus bas).

## La fenêtre

Le bandeau porte la marque, les onglets, l'horloge UTC et trois icônes : `</>` (les outils de développement), `?` (cette aide) et l'engrenage (les paramètres). Chaque onglet est une page :

| Onglet | Ce qu'il montre |
|---|---|
| [Carte](help:map) | Le globe : sites de production et prix, navires en direct, détroits et canaux, cartes de chaleur par pays, flux commerciaux, chaînes d'approvisionnement, fiches pays |
| [Vue d'ensemble](help:overview) | Un instantané des marchés, rafraîchi chaque minute : indices, secteurs, obligations, crypto, devises, matières premières, plus fortes variations |
| [Économie](help:economy) | Les tableaux de bord de l'économie américaine, puis les chiffres de chaque pays et de chaque indicateur |
| [Forex](help:forex) | Les paires majeures, leurs corrélations, les taux directeurs et le positionnement spéculatif |
| [Graphiques](help:charts) | Le terminal de trading : graphiques, indicateurs, flux d'ordres, backtests, compte fictif, alertes |
| [Calendrier](help:calendar) | Publications économiques, événements des sociétés américaines, horaires des marchés |
| [News](help:news) | Les titres par catégorie, un lecteur avec bloqueur de publicités, les favoris |
| [Research](help:research) | Une société à la fois : ratios, états financiers, actionnariat, notations, pairs, secteurs, chaîne d'approvisionnement |
| [Quant](help:quant) | Paires, bêta, COT, signaux, régimes de Markov, prévisions de volatilité, exécution, risque de portefeuille |
| [Desk](help:desk) | Une lecture de trader : force des devises, sessions, récaps, fil, taux, positionnement, appétit pour le risque, saisonnalité, fondamentaux, technique, liquidité, notes, espaces |
| [Journal](help:journal) | Vos vrais trades depuis les relevés MetaTrader ou cTrader, leurs statistiques et un calculateur de risque |
| [IA](help:ai) | Les modèles de langage locaux, leurs compétences, les intégrations Claude et opencode, l'apprentissage automatique |
| [Sources](help:sources) | Chaque source de données, son état, son interrupteur, vos clés d'API, et un diagramme du chemin des données |

L'icône `?` ouvre cette aide sur le chapitre de la page où vous êtes. Le champ de recherche en haut de la colonne cherche dans tous les chapitres : la liste garde ceux qui contiennent les mots, le texte saute à la première occurrence, et Entrée passe à la suivante.

## Organiser les onglets

Le crayon après les onglets active le mode édition : glissez un onglet pour le déplacer, cliquez sur son × pour le masquer, et le + ramène un onglet masqué. Au moins un onglet reste toujours. L'ordre et les onglets masqués sont mémorisés.

## Paramètres

L'engrenage ouvre une fenêtre flottante à trois pages :

- **Langue** : anglais ou français. Toute l'interface bascule aussitôt, données de marché comprises (noms des matières premières et des pays, catégories). Les nombres suivent la langue : `1 234,5` en français, `1,234.5` en anglais ; le terminal Graphiques garde ses textes anglais.
- **Thème** : neuf palettes, appliquées aussitôt à toutes les pages, au globe et aux graphiques. Six sombres (Ember, Abyss, Jade, Oxide, Nova, Bloomberg) et trois claires (Glacier, Dune, Sakura).
- **News** : active ou désactive la traduction des articles. Désactivée, le lecteur d'articles n'a plus de bouton **Traduire**.

## Clés et sources de données

Deepwater fonctionne sans aucune clé. Celles-ci, gratuites, ajoutent des données ; toutes se saisissent dans [Sources](help:sources), à côté de la source qu'elles débloquent :

| Clé | Ce qu'elle ajoute |
|---|---|
| aisstream.io | Les navires en direct sur le globe (rien ne s'affiche sans elle) |
| FRED | L'essentiel des tableaux de bord Économie, environ 170 séries américaines dans Pays, le fret américain de la couche chaînes d'approvisionnement |
| EIA | Stocks et production pétroliers hebdomadaires américains, sites de stockage, onglets Énergie des pages pays |
| Financial Modeling Prep | Des news avec un ticker sur chaque article (sans elle, des flux RSS gratuits) |
| Hugging Face | Le téléchargement des modèles à accès restreint (les ouverts n'ont besoin de rien) |
| Anthropic | Les réglages de l'intégration Claude et sa liste de modèles |
| UN Comtrade | Le commerce par produit au-delà des limites de l'aperçu public |
| BEA, Banxico, Banque de Corée, banque centrale turque, INEGI, BPS, e-Stat | Les séries nationales de ces pays |
| Alpaca, Polygon | Des fournisseurs de données supplémentaires pour la page Graphiques |

Chaque source a aussi un **interrupteur** : coupée, elle ne fait plus aucune requête, et la dernière copie enregistrée sur le disque reste en usage.

## Panneaux latéraux

Deux languettes pendent au bord droit de chaque page :

- **DEV** (visible quand l'icône `</>` est active) : les [outils de développement](help:devtools).
- **AI** : le [panneau latéral d'IA](help:ai), pour interroger un modèle local sur la page que vous regardez.

Échap les ferme.

## Clavier

| Touche | Effet |
|---|---|
| `F1`, `⌘?` | Cette aide |
| `Espace` | Lance ou arrête la rotation du globe |
| `Échap` | Efface la sélection sur la carte ; une seconde pression éteint la carte de chaleur ; ferme les panneaux latéraux, le lecteur et l'aide |
| `Ctrl+\`` | Le terminal de la page Graphiques |
| `⌘Entrée` | Envoie la question dans le panneau d'IA |

## Où sont les données

Les caches, les modèles téléchargés, les données de la page Graphiques, les enregistrements dorés et le journal de l'IA sont dans `~/Library/Application Support/Deepwater/Deepwater/`. Les paramètres, clés comprises, sont dans les préférences de l'application. Supprimer le dossier de données repart de zéro sans perdre vos clés.
