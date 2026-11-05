# IA

La page IA a trois sections : **Modèles**, **Local** et **Apprentissage automatique**. Une règle la traverse : un modèle n'atteint les données de Deepwater qu'à travers les **compétences** de l'emplacement où il est posé, chaque compétence étant un jeu d'outils que l'application exécute pour lui et consigne dans un journal. Rien de la page que vous regardez n'est envoyé à un modèle hors de ces outils.

## Modèles › Modèles locaux

Les modèles sur cet ordinateur (`~/Library/Application Support/Deepwater/Deepwater/models/` ; ouvrir le dossier, supprimer) et le **Hub Hugging Face** pour en obtenir d'autres : une recherche (30 résultats avec la tâche, les téléchargements, les likes et la date ; les dépôts à accès restreint signalés), les fichiers du dépôt choisi avec leurs tailles, et le téléchargement des fichiers cochés (deux à la fois ; progression, vitesse, Annuler ; un téléchargement interrompu reprend ; l'empreinte est vérifiée après). Les poids qui ne sont que des données (GGUF, safetensors, ONNX) sont cochés ; les fichiers qui peuvent exécuter du code à leur chargement (`.bin` pickle, `.pt`, `.ckpt`, Keras, Python, bibliothèques natives) sont signalés en rouge, jamais cochés, et en cocher un demande d'abord. Un jeton Hugging Face, saisi dans [Sources](help:sources) › IA, ouvre les modèles à accès restreint dont vous avez accepté les conditions.

**Emplacements.** Quatre carrés sous les modèles donnent à un modèle ses compétences. Glissez un modèle de la liste dans un emplacement (seulement les modèles avec un fichier GGUF, le format du moteur local), ou d'un emplacement à un autre, ou une intégration depuis Modèles › Intégration (Claude, opencode), et cochez les compétences que l'emplacement accorde :

- **Lire les données de Deepwater** : lecture seule, à travers l'application : les séries par pays, les enregistrements dorés, le WEO du FMI, les taux directeurs, les taux de change, les positions COT, les prix, et les tableaux et graphiques de n'importe quelle page. Ni fichier, ni réseau, ni écriture.
- **Trader** : le testeur de stratégie et le compte fictif en direct de la page Graphiques.

Les compétences appartiennent à l'emplacement : un modèle déposé reçoit celles de l'emplacement, ✕ l'en retire, et un nouvel emplacement n'accorde rien tant qu'aucune case n'est cochée. Un emplacement est lié aux fichiers du modèle (leurs empreintes) : si un fichier change, apparaît ou disparaît, les compétences de l'emplacement sont **suspendues** (cadre rouge en pointillé) jusqu'à ce que le modèle y soit glissé à nouveau. Seul un modèle dans un emplacement lié peut tourner.

## Modèles › Intégration

- **Claude** : les réglages de Claude par l'API d'Anthropic — le modèle et l'effort — avec l'état de la clé (la clé est dans Sources › IA, avec un Test). Claude peut être glissé dans un emplacement, mais rien dans l'application ne l'appelle encore : sa carte le dit et il ne lit rien.
- **opencode** : l'agent d'IA en terminal lit les outils de Deepwater par MCP. Avec **Servir les outils de Deepwater à opencode** coché, l'application écoute sur `127.0.0.1:47821` avec un jeton (**Nouveau jeton** le remplace) ; **Ajouter à opencode** enregistre Deepwater dans la configuration d'opencode, **Copier la configuration** donne le JSON pour une édition à la main. Les outils sont ceux de l'emplacement où opencode est glissé (aucun hors d'un emplacement), vérifiés à chaque appel et consignés ; au plus cinq ordres fictifs par session. Ce que les outils renvoient va au modèle qu'opencode utilise, le plus souvent chez un fournisseur.

## Local

Une conversation avec un modèle de cet ordinateur. Les modèles proposés sont ceux des emplacements liés ; choisissez la taille de contexte (4 096 à 32 768 jetons, plafonnée à celle du modèle ; 16 384 est la taille pour la compétence Trader, dont les outils prennent environ 10 000 jetons dans un tour) et un prompt système optionnel, puis **Charger** (refusé au-delà des trois quarts de la mémoire du Mac). Les réponses s'affichent au fur et à mesure ; une partie de réflexion (Qwen3, DeepSeek-R1) est montrée atténuée au-dessus de la réponse. La ligne d'état donne les jetons, la vitesse et les jetons du prompt déjà évalués.

Le moteur est llama.cpp avec Metal, exécuté comme un programme auxiliaire à part dans un **bac à sable** : il peut lire le code de l'application et les fichiers du modèle, utiliser le GPU, et rien d'autre — pas de réseau, pas d'écriture, aucun autre fichier ni programme. Les préférences de l'application, où sont les clés, sont hors de sa portée. Le modèle est déchargé quand son emplacement le lâche ou que ses fichiers changent.

**Outils.** Les compétences de l'emplacement du modèle sont les outils qu'on lui propose, listés dans son prompt système avec la date du jour et la règle de ne jamais inventer un chiffre. Le modèle écrit un appel, l'application l'exécute et renvoie le résultat, au plus six tours par question ; chaque appel est revérifié contre les compétences de l'emplacement à cet instant, et un outil d'une compétence non accordée est refusé. La transcription montre chaque appel (⚙ nom, arguments, taille des données ou erreur).

*Lire les données de Deepwater* : `list_data` (ce qui est chargé), `golden_series` (l'enregistrement doré d'un indicateur pour un pays, avec la source de chaque point), `country_series`, `imf_weo`, `policy_rate`, `fx_rate`, `cot_positions`, `prices` (le résumé d'un symbole et ses dernières barres), `read_page` (n'importe quelle page et section, telles que ses tableaux et graphiques les montrent, sans vous déplacer ; Sources et IA ne se lisent pas) et `open_page` (vous emmène sur une page, seulement quand vous le demandez).

*Trader* : `backtest_help` (le format des stratégies, les opérateurs, chaque indicateur, les exemples et vos stratégies enregistrées), `run_backtest` (charge une stratégie dans le testeur et l'exécute sur le graphique actif, en changeant de symbole ou d'unité de temps à la demande), `backtest_trades_csv` (exporte les transactions et les résume), `paper_account` (liquidités, valeur, positions, ordres en attente, dernières exécutions), `paper_order`, `paper_close` et `paper_cancel` (le code du ticket d'ordre lui-même, si bien que les exécutions, les ordres liés et les frais se comportent pareil). Limites : le compte fictif en direct seulement (jamais le compte d'entraînement du rejeu), aucun ordre au-dessus de la valeur du compte, au plus cinq ordres, clôtures ou annulations par question, et le prompt système dit au modèle de ne trader que sur demande. Le compte fictif est simulé ; l'exécution réelle n'a pas d'outil.

**Journal.** Chaque appel, exécuté, refusé ou échoué, est une ligne dans `ai/tool-journal.jsonl` (heure, modèle, emplacement, outil, arguments, issue, taille, durée) ; les 200 derniers sont montrés sous la conversation, avec **Montrer le fichier**.

## Panneau latéral

La languette **AI** au bord droit de chaque page ouvre un panneau pour interroger un modèle des emplacements depuis là où vous êtes (Échap le ferme, `⌘Entrée` envoie). Chaque question part avec une **ligne de contexte**, montrée en dessous : la page et ce qu'elle montre (le symbole et l'unité de temps du graphique actif, le pays et l'onglet ouverts, le symbole et la section, le pays sélectionné). Cette ligne ne porte aucune donnée : le modèle ne lit les tableaux et graphiques de la page qu'à travers `read_page`, avec la compétence. opencode, quand il est dans un emplacement, est un des choix, prêt sans chargement ; le modèle qu'il utilise se choisit dans une seconde liste.

## Apprentissage automatique

Entraîner des modèles classiques sur les données de Deepwater :

- **Environnement Python** : un environnement virtuel privé que l'application crée à partir d'un Python du système (3.10 ou plus récent) avec numpy, pandas, scikit-learn, LightGBM, XGBoost et PyTorch ; l'installation s'affiche dans le volet de journal.
- **Données** : **Exporter les données de Deepwater** écrit des fichiers CSV : chaque série par pays et par source, le WEO du FMI, les taux directeurs, les devises journalières, le COT, les enregistrements dorés, et dix ans de bougies journalières pour les symboles que vous listez (SPY, QQQ, GLD, USO, TLT, VIX, EUR/USD, BTC par défaut).
- **Entraînement** : le cadre (forêt aléatoire, gradient boosting, ridge ou logistique de scikit-learn ; LightGBM ; XGBoost ; un perceptron PyTorch), la tâche, le jeu de données et la colonne cible, une cible dérivée pour les séries temporelles (rendement, direction, valeur ou variation de la période suivante), des retards, des colonnes à écarter, la part de test et les hyperparamètres en JSON. Chaque exécution a un dossier avec le modèle et ses réglages, lignes, découpage, métriques (RMSE, MAE, R² et taux de bonne direction ; exactitude, F1 et AUC) et importances des variables ; le tableau des exécutions les liste, un double clic ouvre le dossier.
