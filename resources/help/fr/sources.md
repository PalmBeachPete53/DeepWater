# Sources

Chaque endroit d'où l'application tire ses données, en deux sous-onglets : la **Liste** et le diagramme **Données**.

## Liste

Les sources sont groupées par la page qu'elles alimentent : la carte et les navires, le forex, la macroéconomie, les statistiques officielles, les offices nationaux, les bases à clé gratuite, les enquêtes privées, les rendements du Trésor de la Vue d'ensemble, les flux de News et les listes de son bloqueur, les clés de l'IA, les fournisseurs de Graphiques et vos propres clés Graphiques, les données livrées avec l'application. Chaque ligne montre :

- Son **état** : à jour, en téléchargement ou en erreur, la dernière mise à jour et la fréquence de vérification.
- **Test** : une petite requête, chronométrée.
- **Rafraîchir**, pour les jeux de données téléchargés.
- Un **interrupteur** : coupée, la source ne fait plus aucune requête. Les jeux de données de la carte et du forex gardent la dernière copie enregistrée sur le disque ; la page Graphiques passe au fournisseur suivant de sa chaîne et le dit sur le graphique. Les lignes des éditeurs d'enquêtes n'ont qu'un interrupteur : coupé, leurs chiffres quittent les fiches pays.
- Sa **clé d'API**, quand il y en a une : aisstream, FRED, EIA, BEA, Banxico, la Banque de Corée, la banque centrale turque, UN Comtrade, Financial Modeling Prep, Hugging Face, Anthropic, Alpaca, Polygon et GitHub (les mises à jour de l'application). Une source à clé sans clé le dit et ne fait aucune requête.

Le champ de recherche filtre les lignes. Les états se rafraîchissent chaque seconde tant que l'onglet est affiché.

**Le catalogue officiel.** Sous les sources de l'application, cinq groupes repliés listent les organismes derrière les statistiques du monde — offices statistiques, banques centrales, ministères et agences, organisations internationales, instituts privés, environ 1 300 — en cartes : ce que chacun publie, son accès (gratuit, titres gratuits, payant) et son API, l'état de son site au dernier inventaire, **Test** et **Ouvrir le site**. Le champ de recherche et les filtres par pays et par accès s'appliquent aussi à eux.

## Données

Un diagramme du chemin des données. À gauche les organismes et sociétés que l'application lit (environ 64), groupés : organismes internationaux, agences américaines, offices nationaux et banques centrales, marchés et sociétés de données, web, news et IA, cet ordinateur ; à droite les parties de l'application (les navires de la carte, les chaînes d'approvisionnement, les matières premières, les cartes de chaleur et les fiches pays, les flux commerciaux, Vue d'ensemble, Forex, les vues du Calendrier, les tableaux de bord, Pays et Indicateurs d'Économie, Research, Quant, Graphiques, News, IA). Un lien par paire, aussi épais que les jeux de données qu'il porte ; points et liens prennent les états de la liste (fonctionne, échoue, pas encore vérifié, en pointillé quand coupé). Le bloc **Fusion**, entre les sources et Pays et Indicateurs d'Économie, est les enregistrements dorés.

Survolez une source, une partie ou un lien pour faire ressortir ses données et lister ses jeux de données dans la carte latérale ; cliquez pour l'épingler ; **Montrer dans la liste** ouvre la Liste avec seulement ces jeux de données. Sous les sources, **Listés, non lus par l'application** sont les organismes du catalogue que l'application ne lit pas, avec leur accès et leurs noms par pays.

**Zoom arrière : disque** pose tout le diagramme sur un disque incliné, les sources le long de son arc gauche et les parties le long du droit ; **Zoom arrière : plaques** le dresse sur de fines plaques l'une derrière l'autre. Le survol et le clic fonctionnent toujours.

Sous le disque, le **sous-sol** montre les IA et ce que chacune atteint : les emplacements qui tiennent un modèle, opencode, Claude, l'apprentissage automatique à droite ; à gauche les séries par pays, les tableaux et graphiques, les enregistrements dorés, le WEO du FMI, les taux directeurs, les taux de change, les positions COT, les prix (la compétence *Lire les données de Deepwater*), le testeur de stratégie et le compte fictif (*Trader*), et la page affichée (la ligne de contexte du panneau latéral). Un lien est en pointillé quand l'accès est accordé mais pas en vigueur (un emplacement suspendu, opencode non servi) ; des puits montent vers ce que chacune lit sur le disque au-dessus. Les **flèches haut et bas** passent d'un étage à l'autre ; un **clic** sur un élément le regarde de près, avec ce dont il vient à sa gauche et ce vers quoi il va à sa droite ; les **flèches gauche et droite** vont à l'élément précédent et suivant ; **Échap** revient.
