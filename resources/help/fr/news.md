# News

Un fil de titres en cinq onglets : **General**, **Stocks**, **Crypto**, **Forex** et vos **Favoris**, avec un filtre par ticker (« NVDA », « BTCUSD », « EURUSD »). Il est rafraîchi toutes les cinq minutes tant qu'il est affiché, et sur **Rafraîchir**. Deux icônes à côté de Rafraîchir choisissent la **grille** (autant de colonnes que la largeur permet) ou la vue en **colonne** (une carte compacte par ligne) ; le choix est gardé.

## Cartes et lecteur

Chaque carte est un titre, son résumé, la date et l'ancienneté, le site de l'article et le ticker en badge. Un clic ouvre l'article dans le **lecteur**, un panneau flottant par-dessus la page : la page web elle-même, le titre, le site, la date et le ticker, un bouton de retour pour les liens suivis dedans, **Ouvrir dans le navigateur**, et × ou Échap ou un clic à côté pour fermer. Un clic du milieu ou ⌘-clic sur une carte ouvre directement le navigateur.

L'étoile au coin d'une carte, ou dans l'en-tête du lecteur, garde l'article dans les **Favoris**, tel qu'il était au moment de l'étoile.

Le lecteur a un **bloqueur de publicités**, le moteur de Brave avec les listes qu'il active par défaut (celles d'uBlock Origin, EasyList, EasyPrivacy, bandeaux de cookies…), téléchargées une fois par jour à la première ouverture de l'onglet. Le bouclier dans l'en-tête du lecteur montre son état. Les listes se rafraîchissent et le bloqueur se coupe dans [Sources](help:sources) › News. Sur macOS le lecteur affiche avec le WebKit du système (le moteur de Safari), que les protections anti-robots de certains sites acceptent là où d'autres moteurs sont refusés ; quand un site refuse quand même (401, 403, 429), le lecteur le dit sous son en-tête et **Ouvrir dans le navigateur** est la voie.

## Sources

Sans clé, des flux RSS gratuits fusionnés et triés du plus récent : CNBC, MarketWatch et BBC Business (general) ; Nasdaq, Seeking Alpha et Investing.com (stocks) ; Cointelegraph, Decrypt, CoinDesk et Investing.com (crypto) ; FXStreet et Investing.com (forex). Le badge de ticker vient du flux quand il en donne un, sinon du texte (« (NASDAQ: NFLX) », « $NVDA », la monnaie ou la devise nommée en premier). Avec le filtre par ticker, l'onglet Stocks lit les flux de Yahoo Finance et du Nasdaq sur ce symbole ; les autres onglets gardent les articles qui le nomment.

Avec une clé **Financial Modeling Prep** enregistrée dans Sources › News, ses points d'accès donnent un ticker à chaque article et le filtre les interroge directement. Une requête qui échoue revient aux flux et le dit dans le bandeau.

## Impact et thèmes

Chaque titre est lu d'après ses mots pour un impact (une étiquette sur la carte) et des thèmes : banques centrales, inflation, emploi, PIB, PMI, ventes au détail, commerce, résultats. Deux listes du sous-en-tête filtrent dessus. Cela lit des mots, pas le sens : un repère, pas un verdict.

Dans le lecteur, **Demander à l'IA** envoie le titre de l'article et son résumé de flux à votre modèle local dans le [panneau latéral](help:ai). Le modèle ne lit pas la page elle-même.

**Traduire** met le texte de la page dans votre langue, sur place et sur ce Mac, avec la traduction d'Apple sur macOS 26 ou plus récent : rien n'est envoyé nulle part. Seules les langues installées dans Réglages Système › Général › Langue et région › Langues de traduction sont utilisées ; pour une autre, le lecteur indique laquelle ajouter. Les liens et la mise en forme à l'intérieur des paragraphes sont perdus jusqu'à ce que **Original** rétablisse la page. Les [paramètres](help:start) › News la désactivent.
