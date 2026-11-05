# Desk

Une lecture des marchés de trader en quinze sections, dans le sous-en-tête ; la dernière affichée est retenue. Les prix viennent de Yahoo Finance, avec environ quinze minutes de retard. Rien ici ne demande de clé ni de compte, et chaque source figure dans [Sources](help:sources) › Desk.

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

Une seule liste de trois choses, les plus récentes d'abord : les titres du flux public d'InvestingLive, rafraîchis chaque minute ; les marchés qui franchissent un nouveau palier de 3 % sur la journée ; et les publications du calendrier. C'est un flux, pas un squawk professionnel : quelques minutes de retard au mieux. Les titres s'ouvrent dans le lecteur, et **TV en direct** y ouvre aussi le direct d'une chaîne sur YouTube.

## Taux

- **Le chemin de la Fed** : ce qu'impliquent les contrats à terme CME sur fed funds à 30 jours à chaque réunion du calendrier de la Fed : le taux après la réunion, la variation, le cumul, et les probabilités de −50, −25, statu quo, +25 et +50 points de base. Chaque mouvement est réparti entre les deux quarts de point les plus proches, la simplification habituelle. Cela lit ce qui est intégré dans les prix, pas ce qui arrivera.
- **Banques centrales** : neuf taux directeurs de la BRI, le dernier mouvement et sa date, la variation sur un an, le dernier vote quand la banque le publie dans son texte (les dissidences à la Fed, le décompte à la Banque d'Angleterre) et la prochaine décision d'après les pages des banques. La RBA et la RBNZ refusent les programmes : leurs dates n'apparaissent que si le calendrier économique les annonce.
- **Courbes des taux** : États-Unis, zone euro, Royaume-Uni, Japon, Canada et Suisse, dernière publication. Les méthodes diffèrent : les niveaux se comparent approximativement.

- **Royaume-Uni** : le taux implicite de la courbe OIS quotidienne de la Banque d'Angleterre et, réunion par réunion, le mouvement intégré réparti sur les quarts de point comme pour la Fed. Il contient une petite prime et ce n'est pas une enquête de probabilités.
- **Zone euro** : les contrats à terme CME sur l'€STR à trois mois, par trimestre, face à l'€STR du jour. Un trimestre contient environ deux réunions de la BCE : cela lit le chemin, pas une réunion ; les volumes sont faibles et affichés.
- **Australie et Nouvelle-Zélande** : les taux mensuels à 3 mois et à 10 ans de l'OCDE, car leurs banques centrales refusent les programmes et aucune courbe quotidienne gratuite n'a été trouvée. Les dates de leurs décisions sont gardées dans l'application et mises à jour avec elle.

**2 ans − taux**, dans le tableau des banques centrales, est le rendement d'État à 2 ans face au taux directeur : où le marché voit les taux, avec une prime de terme. C'est la seule lecture de marché pour la BoJ, la BNS et la BoC, car aucune source gratuite qui l'autorise ne publie de contrats à terme courts pour elles. Les prévisions d'ING sont dans Prévisions.

## Positionnement

Deux listes, pour deux foules. Pour les positions sur contrats à terme, voir [Quant](help:quant) › COT.

- **Forex et CFD** : la Community Outlook de Myfxbook, par son API officielle et votre compte gratuit : la part de lots long et short, le nombre de positions et les prix moyens par symbole, les plus traités d'abord. La communauté inclut des comptes de démo, et leur part s'affiche au-dessus de la liste. La moitié des symboles n'ont que quelques dizaines de positions et affichent 0 % ou 100 % : ils restent dans la liste, atténués, mais les tuiles ne comptent que les symboles d'au moins 500 positions. Connectez-vous ici avec votre e-mail et votre mot de passe. Le mot de passe part une seule fois vers Myfxbook, en HTTPS, pour ouvrir une session, et n'est pas conservé. La session se termine après un mois ou quand votre adresse internet change, et la section redemande alors. Les chiffres sont demandés au plus toutes les demi-heures, sans historique. C'est une communauté de particuliers, pas tout le marché.
- **Crypto** : le ratio public de Binance des comptes long et short sur douze perpétuels USDT, avec les 30 derniers jours : une mesure de la foule, pas de l'argent.

## Appétit pour le risque

Un composite de 0 à 100 de sept marchés en clôtures journalières : le VIX, la tendance du S&P 500, le haut rendement contre la qualité, AUD/JPY, le cuivre contre l'or, les actions contre l'or, et le bitcoin. Chacun est classé par rapport à ses 252 derniers jours et les rangs sont moyennés ; sous 40 on lit risk-off, au-dessus de 60 risk-on. C'est une estimation propre à cette application, pas un indice publié.

## Saisonnalité

Le comportement d'un marché au fil du calendrier : le chemin moyen de l'année contre l'année en cours, le taux de réussite et le rendement moyen de la période, le rendement moyen par mois et par jour de la semaine, et la période année par année. Choisissez 5, 10, 15 ou 20 ans, l'année entière, un semestre ou un trimestre, ou glissez sur les mois du graphique, et décidez d'écarter ou non les années extrêmes au-delà de 1,5 écart-type. L'année en cours est exclue des moyennes. La saisonnalité passée ne prédit pas une année donnée.

## Fondamentaux

Huit devises sur six thèmes : taux, inflation, croissance, emploi, activité et sentiment, d'après les dernières publications de chaque économie au calendrier économique et les taux directeurs de la BRI. Chaque thème se lit fort, neutre ou faible selon des règles fixes, indiquées sous le tableau, et le score est leur moyenne de −100 à +100. Le biais compare le score à la moyenne des huit devises : il dit lesquelles se détachent, pas lesquelles sont fortes en soi. Un thème sans publication récente est écarté. Le comparateur met deux devises côte à côte. C'est une lecture simple, pas une prévision, et **Demander à l'IA** la fait commenter par votre modèle local.

## Technique

Chaque marché sur six signaux : tendance (EMA 20 contre EMA 50), momentum (RSI), volatilité, volume, structure (plus hauts, plus bas) et liquidité. Filtrez par classe, tendance et nom ; un clic ouvre la fiche de signaux du marché avec son radar. **Maj+clic** sur un autre marché le superpose au premier, en pointillés, avec ses lectures sous celles du premier, et marque les deux lignes ; sur un troisième, il remplace le second, et sur l'un des deux, il le retire. Un simple clic revient à un seul marché. Le volume est celui du marché, ou celui d'un ETF comme approximation pour le forex et certaines matières premières ; un tiret signifie qu'il n'y en a pas.

## Liquidité

Des zones de stops lues sur le seul prix : plus hauts et plus bas égaux en bougies 1 heure ou journalières, là où les stops ont tendance à se placer. Le graphique trace les zones, le plus haut et le plus bas de la veille ; les tableaux donnent les touches de chaque zone, sa distance et son état, les balayages récents et les extrêmes des séances. **Me notifier d'un nouveau balayage** envoie une notification de bureau pour le marché et l'unité de temps affichés, vérifiés toutes les 15 minutes tant que Deepwater est ouvert et que la section a été ouverte. C'est une estimation à partir des bougies, pas un carnet d'ordres. Pour la vraie profondeur, voir la heatmap dans [Graphiques](help:charts).

## Notes

Les titres d'ING Think, de la Fed, de la BCE, de la Banque d'Angleterre et des discours de banquiers centraux de la BRI, par éditeur et avec une recherche. Un clic ouvre la note dans le lecteur, avec ses boutons **Demander à l'IA** et **Traduire**. Les sites des banques centrales sont exemptés des listes du bloqueur de publicité, dont l'une rendait les pages de la Fed blanches. **Les notes et les prévisions de change des grandes banques sont réservées à leurs clients**, sans flux public : elles ne sont pas ici.

## Prévisions

Les prévisions des autres, côte à côte. **ING Think** par trimestre, pour la croissance, l'inflation, les taux directeurs, les taux à trois mois, les obligations, le change et les matières premières, avec la date de sa dernière mise à jour ; dans l'onglet change, un clic sur une paire trace le chemin d'ING après un an de prix. Le **FMI** et l'**OCDE** par année pour la croissance, l'inflation et le chômage des huit grandes économies ; un clic sur une ligne trace ses chiffres passés et les prévisions. Les **projections de la Fed**, qui demandent la clé FRED gratuite dans [Sources](help:sources). **L'écart entre prévisionnistes** donne la réponse la plus basse, la médiane et la plus haute des dernières enquêtes de prévisionnistes professionnels, celles de la BCE et de la Fed de Philadelphie : des chiffres annuels. Aucune source gratuite ne publie cette fourchette pour une publication précise du calendrier. TD, KBC, MUFG et RBC publient aussi des tableaux de change, mais leurs conditions n'autorisent pas ou n'indiquent pas de droit de reprendre leurs chiffres : ce sont des liens vers leurs sites.

## Espaces

Vos propres écrans : jusqu'à six panneaux en direct de sections du Desk, dans autant d'espaces nommés que vous voulez. Les panneaux se chargent quand l'espace est affiché. Ils ont moins de place que la section : un tableau large peut être coupé au bord du panneau.
