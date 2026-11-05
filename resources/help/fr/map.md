# Carte

La carte est un globe. Glissez pour le faire tourner, molette pour zoomer, cliquez sur un point, un groupe ou un pays. `Espace` lance et arrête la rotation ; `Échap` efface la sélection, et un second `Échap` éteint la carte de chaleur d'un indicateur. Les commandes du globe sont à son coin : **3D**, **⇄** et **⚓**, expliquées plus bas.

## Matières premières

Les points sont des sites de production, mines, gisements et exploitations d'une quarantaine de matières premières, colorés par catégorie (énergie, métaux précieux et industriels, céréales, softs). Ils se regroupent quand on s'éloigne ; cliquez sur un groupe pour l'ouvrir. La colonne de gauche porte la **liste de suivi** et les **plus fortes variations** ; cliquer sur une matière première ouvre sa fiche : prix, variation, graphique, et les sites qui la produisent.

Les prix sont réels, chacun avec sa source et son heure (l'infobulle de la ligne, la note sous le graphique) :

- **Contrats à terme** pour 20 matières premières (brut, Brent, gaz naturel, or, argent, cuivre, platine, palladium, aluminium, blé, maïs, soja, riz, café, cacao, sucre, coton, GNL, minerai de fer, huile de palme), de Yahoo Finance, avec une dizaine de minutes de retard sur la bourse. La variation est contre la clôture précédente du même contrat ; les plus fortes variations classent ces variations journalières.
- **Moyennes mensuelles du FMI** pour toutes les matières premières : le seul prix du charbon, de l'uranium, du nickel, du zinc, du lithium et du cobalt, qui n'ont pas de cotation gratuite de contrat à terme ; ailleurs une ligne de référence sous le prix du contrat. Le FMI publie la moyenne d'un mois au début du mois suivant.

## Navires en direct

Les navires demandent une clé gratuite **aisstream.io**, saisie dans [Sources](help:sources). Seuls les transporteurs de matières premières sont montrés, classés d'après le type qu'ils émettent : **pétroliers** (brut, produits, GNL, chimiquiers) et **cargos** (vrac sec, conteneurs) ; l'AIS ne dit pas ce qu'il y a à bord. Un navire silencieux depuis 30 minutes disparaît ; les sillages apparaissent en zoomant. Les navires n'émettent leur nom et leur type que toutes les quelques minutes, si bien que la première session se remplit lentement ; ce qu'ils ont dit est mémorisé et les sessions suivantes les classent dès leur apparition.

Cliquer sur un navire ouvre sa fiche. Sa destination AIS (un code de port, un nom de port, `FROM>TO`) est résolue en un port et la **route probable** est tracée sur un réseau maritime, avec la distance par mer et le temps à la vitesse actuelle.

## Points de passage

Des zones en pointillé marquent dix détroits et canaux. Cliquez sur l'une d'elles pour ses passages des dernières 48 heures, un graphique horaire et les transits journaliers. Un passage est un navire vu dans la zone après plus de six heures d'absence ; les comptes sont des minimums. Certaines zones ne reçoivent aucun rapport d'aisstream.io (Ormuz, Suez, Panama, le Bosphore au moment d'écrire) : la fiche le dit plutôt que d'afficher des zéros. L'historique journalier vient de PortWatch du FMI (AIS satellitaire, publié chaque semaine avec quelques jours de retard) : pétroliers, vrac sec, autres, capacité des pétroliers, tendance à 7 jours contre 90 jours.

## Indicateurs économiques

La colonne de droite liste 24 indicateurs en quatre groupes :

- **Économie** : croissance du PIB réel, inflation, chômage, taux directeur, taux directeur réel, balance courante, solde budgétaire, dette publique, PIB par habitant (PPA) et PIB.
- **Flux de capitaux et commerce** : entrées nettes de capitaux, IDE et flux de portefeuille, balance commerciale, ouverture commerciale, croissance des volumes d'exportation. Ils viennent de la balance des paiements, qui n'a que des chiffres déclarés : l'année par défaut est la dernière que la plupart des pays ont déposée, environ un an en arrière.
- **Derniers chiffres officiels** : l'inflation et le chômage du dernier mois, des offices nationaux et des compilations gratuites.
- **Chaînes d'approvisionnement** : rendements des cultures, fret routier, stocks de brut (plus bas).

Chaque carte montre la médiane mondiale et un histogramme des pays. Cliquer sur l'une colore le globe (légende en bas à gauche, sur une échelle du 5e au 95e centile) et affiche le classement dans la colonne de gauche, avec un curseur d'année de 2010 à la dernière projection du FMI. Le `?` cerclé près du titre du classement explique ce que l'indicateur mesure, comment le lire et ses pièges. Les chiffres de l'année en cours sont des estimations du FMI, les années suivantes des projections.

- **3D** élève chaque pays selon sa valeur, sur la même échelle que les couleurs, et incline la vue.
- **⇄** montre le commerce de marchandises comme une carte de flux : les 50 plus grands flux bilatéraux, ou, avec un pays sélectionné, ses dix principaux clients et fournisseurs, en arcs de l'exportateur à l'importateur avec des particules vers l'importateur. Survolez un arc pour le montant et sa part du commerce des deux pays. La source est le commerce par pays partenaire du FMI, dernière année complète ; le premier téléchargement est gros (toutes les années depuis 1948) et peut prendre jusqu'à une minute.

## Fiche pays

Cliquez sur un pays sur le globe ou dans un classement. Sa fiche économique donne :

- Les chiffres clés, l'an dernier / cette année / l'an prochain, les barres de croissance et d'inflation, l'historique du taux directeur.
- **Secteurs économiques** : agriculture, industrie et services en parts du PIB et de l'emploi, la valeur ajoutée en dollars, l'industrie manufacturière dans l'industrie, et l'évolution sur dix ans. Le `?` cerclé lit les chiffres du pays : son profil (services, agricole, extractif, industriel, mûr ou en transition), la production par emploi de chaque secteur, le mouvement des parts (désindustrialisation, rééquilibrage, tertiarisation), puis pour 56 grandes économies une histoire rédigée dont les événements sont numérotés sur le graphique.
- **Immobilier** : les prix réels des logements sur un an, dix ans et depuis leur sommet, et leur valorisation par rapport aux revenus et aux loyers. **Dette** : ménages, entreprises et État en % du PIB, service de la dette, crédit bancaire, créances douteuses. Les deux ont un `?` avec une lecture des chiffres (boom, retournement, boom du crédit, désendettement, ménages sous tension).
- **Derniers chiffres** : les chiffres mensuels et trimestriels que les marchés suivent (IPC, chômage, PIB, production, ventes au détail, confiance, PMI…), chacun avec sa valeur précédente et sa période, de la source gratuite la plus directe qui couvre le pays.
- **Commerce par produit** : exportations et importations par chapitre de produits pour la dernière année complète, avec les parts (UN Comtrade ; une clé lève les limites de l'aperçu public).
- **Chaînes d'approvisionnement** : rendements, fret routier et stocks de brut sur trois ans.
- Les sites de matières premières qui s'y trouvent, et les **sources officielles** derrière les statistiques du pays : office statistique, banque centrale, ministères, avec ce que chacun publie, si ses données sont gratuites et comment un programme les lit.

## Chaînes d'approvisionnement

Le bouton **⚓** affiche la couche chaînes d'approvisionnement et son aperçu dans la colonne de gauche :

- **Ports** (points) : environ 2 000 ports de PortWatch du FMI, colorés par leurs escales des sept derniers jours contre la même semaine un an plus tôt (gris sans chiffres), dimensionnés par leur trafic. Les plus actifs se voient de loin, les autres en zoomant. La fiche d'un port donne ses escales, les tonnes déchargées et chargées, son profil et les perturbations qui l'ont nommé.
- **Perturbations** (anneaux) : cyclones, séismes, inondations, incendies et événements comme le détroit d'Ormuz, en cours ou terminés depuis moins de 30 jours, colorés par niveau d'alerte, avec les ports touchés.
- **Stockage de pétrole** (losanges) : 44 hubs de négoce, réserves stratégiques et terminaux. L'EIA publie les stocks de Cushing, de la côte du Golfe et de la réserve stratégique (avec une clé EIA) ; chaque site montre aussi les stocks de brut de fin de mois de son pays (JODI).
- **Aperçu** : l'indice de pression sur les chaînes d'approvisionnement de la Fed de New York, la semaine des ports contre un an plus tôt avec les plus fortes baisses et hausses, les perturbations, le fret américain (tonnage routier, expéditions, prix du fret ; avec une clé FRED) et les stocks hebdomadaires de l'EIA.

Le groupe **Chaînes d'approvisionnement** de la colonne de droite colore les pays par les **rendements** du blé, du maïs, du riz et du soja contre l'année précédente (prévisions de l'USDA, Eurostat pour les membres de l'UE ; le curseur lit des campagnes), le **fret routier** en tonnes-km (environ 45 pays) et les **stocks de brut** du dernier mois contre un an plus tôt.

## Sources

Les Perspectives de l'économie mondiale et la balance des paiements du FMI, la BRI (taux directeurs, prix immobiliers, crédit, service de la dette), la Banque mondiale (secteurs, crédit, créances douteuses), l'OCDE (ratios de prix immobiliers, indicateurs clés), Eurostat, les offices nationaux (BLS, Brésil, Argentine, Afrique du Sud, Singapour et d'autres), les bases du commerce et des prix à la consommation du FMI, UN Comtrade, PortWatch, JODI, l'USDA, l'ITF, la Fed de New York, FRED et l'EIA. Chacune a une ligne et un interrupteur dans [Sources](help:sources) ; celles à clé le disent tant que vous n'en saisissez pas.
