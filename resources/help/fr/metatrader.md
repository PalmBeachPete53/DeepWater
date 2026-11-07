# Comptes

La page **Comptes** (MetaTrader jusqu'en novembre 2026). Un seul ordre pour plusieurs comptes de trading : MetaTrader 4 et 5, cTrader, DXtrade, TradeLocker et Match-Trader, dimensionné pour chacun selon son solde, depuis Deepwater. **Ce sont de vrais ordres.** Aucune IA ne peut lire cette page ni envoyer quoi que ce soit depuis elle.

Les comptes sont joints via **MetaCopier**, un service cloud payant qui maintient les connexions et détient vos identifiants MetaTrader ; Deepwater ne garde que sa clé d'API, dans le coffre du système (Trousseau sur macOS, Gestionnaire d'identifiants sur Windows). Vérifiez que votre courtier ou votre prop firm l'autorise avant de vous en servir. FTMO limite le capital à 400 000 $ par trader ou par stratégie et peut suspendre les comptes aux trades identiques au-delà ; savoir s'il accepte un service qui détient vos identifiants est une question à lui poser.

## Mise en place

1. Créez un compte MetaCopier, ajoutez-y vos comptes MetaTrader et créez une clé d'API.
2. Collez la clé ici puis **Enregistrer** : elle est d'abord essayée, et une clé refusée n'est pas conservée.
3. **Commencez par un compte démo.**

## Envoyer un ordre

1. Cochez **Armé** sur les comptes qui doivent le recevoir. Aucun n'est armé au démarrage de l'app.
2. Indiquez le symbole, le sens, le risque par compte en pourcentage de son solde (5 % au plus) et un stop, obligatoire. L'objectif est facultatif.
3. **Préparer** montre, par compte, le symbole sous son nom là-bas, le prix, les lots et ce qu'ils risquent, ou pourquoi ce compte refuse. Les lots sont arrondis à l'inférieur : un compte ne risque jamais plus que demandé ; sous le volume minimum, il refuse plutôt que d'arrondir au-dessus.
4. **Envoyer** demande confirmation, puis envoie une fois à chaque compte prêt. Un plan dure 30 secondes et l'envoi le consomme.
5. Chaque ligne dit ensuite ce qui s'est passé : **Ouvert** quand la position a été vue sur le compte, **Refusé** avec la raison, ou **Pas vu sur le compte**. Dans ce dernier cas, regardez dans MetaTrader avant de renvoyer : **Deepwater ne renvoie jamais un ordre de lui-même**, car un ordre dont la réponse s'est perdue a pu être exécuté.

## Argent réel

Un compte que le service n'indique pas comme démo, ou n'indique pas du tout, compte comme de l'argent réel et ne peut pas être armé tant que vous ne l'autorisez pas, en tapant un mot, pour cette session seulement. Un challenge de prop firm peut être indiqué comme démo : l'armer reste votre décision, et le nom du serveur est affiché pour vous aider.

## Positions

Les positions de tous les comptes, actualisées toutes les quelques secondes tant que la page est ouverte. **Fermer** en termine une, après une question. **Tout fermer sur les comptes armés** termine toutes leurs positions, y compris celles ouvertes ailleurs que dans Deepwater.

Les soldes viennent de MetaCopier, qui les synchronise une fois par minute. Une réponse lente ou les règles d'un symbole peuvent faire différer l'exécution du prix du plan.

## Plateformes et sécurité

MetaCopier relie aussi des plateformes d'échange (Binance, Bybit, OKX…), Tradovate, TradeStation et Alpaca. Leurs comptes s'affichent, avec leur plateforme, mais aucun ordre n'est encore dimensionné pour eux : la page les refuse plutôt que de deviner ce qu'est un lot chez eux.

Ce qui protège les comptes : la clé MetaCopier reste dans le coffre de secrets du système et ne part que vers MetaCopier, en HTTPS ; les mots de passe de vos plateformes se saisissent sur le site de MetaCopier, jamais dans Deepwater ; aucune IA n'atteint cette page ; l'argent réel reste bloqué tant que vous ne l'autorisez pas. À vous : un mot de passe solide et la double authentification sur MetaCopier, un compte démo d'abord, et **Oublier** la clé sur un ordinateur partagé.
