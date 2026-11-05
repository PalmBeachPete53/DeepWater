# Desk

Une lecture des marchés de trader en quatorze sections, dans le sous-en-tête ; la dernière affichée est retenue. Les prix viennent de Yahoo Finance, avec environ quinze minutes de retard. Rien ici ne demande de clé ni de compte, et chaque source figure dans [Sources](help:sources) › Desk.

Les textes sont écrits par des règles fixes, pas par un modèle. Quand une section a un bouton **Demander à l'IA**, il ouvre le [panneau IA](help:ai) et demande à votre modèle local de lire la section par son outil : la question ne contient aucun chiffre.

## Marchés

109 marchés en cinq colonnes : forex, indices, matières premières, actions et crypto. Cherchez par nom, changez la période (1 jour, 1 semaine, 1 mois) et l'ordre. Les tuiles donnent la largeur du marché, le mouvement moyen, le meneur et le marché à la traîne. Un clic sur une ligne ouvre la fiche du marché dans Technique.

## Force

Des indices de devises non pondérés : chacune des huit grandes devises contre les sept autres, chaque paire comptant autant. Un graphique depuis la première bougie à l'écran, en 15 minutes, 1 heure, 4 heures ou journalier et le classement. Cela lit des mouvements relatifs, pas l'économie. Les indices pondérés par le commerce sont une autre mesure.

## Sessions

Sydney, Tokyo, Londres et New York sur une frise à votre heure, avec ce qui est ouvert, les chevauchements et le temps avant chaque ouverture et fermeture. L'heure d'été est gérée. Ce sont les horaires habituels de trading, pas ceux d'une bourse.

## Récap

Ce qui s'est passé pendant une session ou une semaine : les mouvements des principaux marchés de l'ouverture à la clôture, en bougies de 15 minutes pour les sessions, le plus grand mouvement en graphique, les publications du calendrier et les titres du fil sur cette période. Le résumé est un texte fixe construit à partir de ces chiffres. **Demander à l'IA** le fait lire par votre modèle local.

## Fil

Une seule liste de trois choses, les plus récentes d'abord : les titres du flux public d'InvestingLive, rafraîchis chaque minute ; les marchés qui franchissent un nouveau palier de 3 % sur la journée ; et les publications du calendrier. C'est un flux, pas un squawk professionnel : quelques minutes de retard au mieux. **TV en direct** ouvre le direct d'une chaîne sur YouTube dans votre navigateur.

## Taux

- **Le chemin de la Fed** : ce qu'impliquent les contrats à terme CME sur fed funds à 30 jours à chaque réunion du calendrier de la Fed : le taux après la réunion, la variation, le cumul, et les probabilités de −50, −25, statu quo, +25 et +50 points de base. Chaque mouvement est réparti entre les deux quarts de point les plus proches, la simplification habituelle. Cela lit ce qui est intégré dans les prix, pas ce qui arrivera.
- **Banques centrales** : neuf taux directeurs de la BRI, le dernier mouvement et sa date, la variation sur un an et la prochaine décision quand elle est connue. Le calendrier économique n'annonce que certaines décisions à l'avance : la colonne est souvent vide hors de la Fed.
- **Courbes des taux** : les rendements au pair du Trésor américain et la courbe au comptant AAA de la zone euro de la BCE, dernière publication.

Seule la Fed a des probabilités : aucune source gratuite ne publie de contrats à terme de taux pour les autres banques.

## Positionnement

Deux listes, pour deux foules. Pour les positions sur contrats à terme, voir [Quant](help:quant) › COT.

- **Forex et CFD** : la Community Outlook de Myfxbook, par son API officielle et votre compte gratuit : la part long et short, les lots, les positions et les prix moyens par symbole. Connectez-vous ici avec votre e-mail et votre mot de passe. Le mot de passe part une seule fois vers Myfxbook, en HTTPS, pour ouvrir une session, et n'est pas conservé. La session se termine après un mois ou quand votre adresse internet change, et la section redemande alors. Les chiffres sont demandés au plus toutes les demi-heures, sans historique. C'est une communauté de particuliers, pas tout le marché.
- **Crypto** : le ratio public de Binance des comptes long et short sur douze perpétuels USDT, avec les 30 derniers jours : une mesure de la foule, pas de l'argent.

## Appétit pour le risque

Un composite de 0 à 100 de sept marchés en clôtures journalières : le VIX, la tendance du S&P 500, le haut rendement contre la qualité, AUD/JPY, le cuivre contre l'or, les actions contre l'or, et le bitcoin. Chacun est classé par rapport à ses 252 derniers jours et les rangs sont moyennés ; sous 40 on lit risk-off, au-dessus de 60 risk-on. C'est une estimation propre à cette application, pas un indice publié.

## Saisonnalité

Le comportement d'un marché au fil du calendrier : le chemin moyen de l'année contre l'année en cours, le taux de réussite et le rendement moyen de la période, le rendement moyen par mois et par jour de la semaine, et la période année par année. Choisissez 5, 10, 15 ou 20 ans, l'année entière, un semestre ou un trimestre, et d'écarter ou non les années extrêmes au-delà de 1,5 écart-type. L'année en cours est exclue des moyennes. La saisonnalité passée ne prédit pas une année donnée.

## Fondamentaux

Huit devises sur six thèmes : taux, inflation, croissance, emploi, activité et sentiment, d'après les dernières publications de chaque économie au calendrier économique et les taux directeurs de la BRI. Chaque thème se lit fort, neutre ou faible selon des règles fixes, indiquées sous le tableau, et le score est leur moyenne de −100 à +100. Un thème sans publication récente est écarté. Le comparateur met deux devises côte à côte. C'est une lecture simple, pas une prévision, et **Demander à l'IA** la fait commenter par votre modèle local.

## Technique

Chaque marché sur six signaux : tendance (EMA 20 contre EMA 50), momentum (RSI), volatilité, volume, structure (plus hauts, plus bas) et liquidité. Filtrez par classe, tendance et nom ; un clic ouvre la fiche de signaux du marché avec son radar. Le volume est celui du marché, ou celui d'un ETF comme approximation pour le forex et certaines matières premières ; un tiret signifie qu'il n'y en a pas.

## Liquidité

Des zones de stops lues sur le seul prix : plus hauts et plus bas égaux en bougies 1 heure ou journalières, là où les stops ont tendance à se placer. Le graphique trace les zones, le plus haut et le plus bas de la veille ; les tableaux donnent les touches de chaque zone, sa distance et son état, les balayages récents et les extrêmes des séances. C'est une estimation à partir des bougies, pas un carnet d'ordres. Pour la vraie profondeur, voir la heatmap dans [Graphiques](help:charts).

## Notes

Les titres d'ING Think, de la Fed, de la BCE, de la Banque d'Angleterre et des discours de banquiers centraux de la BRI, par éditeur et avec une recherche. Un clic ouvre la note dans votre navigateur. **Les notes et les prévisions de change des grandes banques sont réservées à leurs clients**, sans flux public : elles ne sont pas ici.

## Espaces

Vos propres écrans : jusqu'à six panneaux en direct de sections du Desk, dans autant d'espaces nommés que vous voulez. Les panneaux se chargent quand l'espace est affiché. Ils ont moins de place que la section : un tableau large peut être coupé au bord du panneau.
