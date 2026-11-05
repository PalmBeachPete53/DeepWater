# Journal

Vos vrais trades et votre risque, gardés sur cet ordinateur dans un fichier des données de l'application. Rien n'est envoyé nulle part. Deepwater ne se connecte jamais à un courtier et ne demande jamais de mot de passe : les trades arrivent par fichier ou à la main.

La barre du haut choisit le compte (vous pouvez en garder plusieurs, chacun avec sa devise et son solde) et filtre par année, instrument, sens et stratégie.

## Importer

**Importer des trades** lit un relevé exporté de votre plateforme :

- **MetaTrader 4 et 5** : le relevé de compte ou le rapport HTML.
- **cTrader** : son historique CSV.
- **Tout autre CSV** : les colonnes sont reconnues par leur nom, et la fenêtre vous laisse dire laquelle est laquelle.

Un trade déjà présent n'est pas importé deux fois : un nouveau relevé peut chevaucher l'ancien. Les symboles avec un suffixe de courtier (`EURUSD.m`) sont rattachés au même marché. **+ Saisir un trade** en ajoute un à la main.

## Tableau de bord

Résultat net, taux de réussite, facteur de profit, gain moyen contre perte moyenne, espérance par trade, drawdown maximal, facteur de récupération, les plus longues séries et la durée moyenne de détention, avec le résultat cumulé, les derniers trades, le mois et le trimestre. Le score de performance est une moyenne propre à cette application de six mesures, chacune sur une échelle simple : ce n'est pas un standard. Les positions encore ouvertes sont valorisées aux cours retardés du terminal, avec les tailles de contrat d'un compte standard.

## Registre des trades

Chaque trade : dates, symbole, sens, volume, prix, durée, coûts, profit, net, stratégie et notes. Un trade peut être modifié, pour ajouter sa stratégie et une note, ou supprimé.

## Détails avancés

Le résultat par jour de la semaine, par heure de la journée et par durée de détention, les cinq meilleurs symboles, et vos cinq meilleurs et cinq pires trades.

## Calculateur de risque

Un calculateur de position. Le solde et la devise du compte, l'instrument, l'entrée (ou le cours en direct), le stop, un objectif facultatif et le risque par trade, en pourcentage ou en argent, donnent la taille de position en lots, l'argent en jeu, le gain visé et le rapport, dessinés sur une petite carte du trade. Les tailles de contrat sont celles d'un compte standard. **Vérifiez la taille de contrat et la marge de votre courtier** : elles diffèrent d'un courtier à l'autre, et le calculateur ne peut pas connaître les vôtres.
