# Outils de développement

L'icône `</>` du bandeau affiche une petite languette **DEV** au bord droit de la zone des pages ; l'icône et le panneau ouvert sont mémorisés. Cliquer sur la languette fait glisser un panneau à onglets pleine largeur par-dessus les pages.

## Codebase

Le graphe de code du projet, tel que Graphify le construit à partir des sources (`graphify-out/graph.json`, trouvé à côté de l'application ou dans le dossier de travail), en 3D : les points sont des fonctions, des classes et des fichiers, colorés par communauté, avec des liens fléchés en pointillé entre communautés. Les noms s'affichent au survol ou à la sélection ; la colonne a une recherche de nœud, le panneau *Node info* (type, communauté, source ou membres, degré, voisins cliquables), la légende *Communities* avec une case par communauté, et les décomptes. Cliquez sur un point pour l'inspecter, double-cliquez pour ouvrir sa communauté ; glissez pour orbiter, molette pour zoomer, `Échap` efface la sélection. Les fichiers sont rechargés d'eux-mêmes quand le graphe est reconstruit.

## Options de ligne de commande

Pour les captures et les vérifications, l'application peut s'ouvrir sur une page et la rendre dans un fichier :

```
deepwater [--theme <id>] --page <id> [--devtools] [--country FRA] [--indicator <n>]
          [--economy-country USA [--economy-tab 2]] [--economy-indicator "Inflation Rate"]
          [--news-ticker NVDA] [--news-article 0] [--news-url https://…]
          [--sources-tab 1] [--sources-disc [floor:1|ai:0]] [--settings [0|1]] [--help-page [chapitre]]
          [--ai-search <texte>] [--ai-local <prompt>] [--ai-dock [prompt]]
          [--scroll <0..1> | --scroll-end] --delay <ms> --screenshot sortie.png
```

`--theme` ne vaut que pour cette exécution. `--settings` et `--help-page` capturent leur fenêtre flottante. `deepwater --pine-js fichier.pine` imprime le JavaScript que PineTS exécute pour un script Pine et quitte.

Les identifiants de page sont `map`, `overview`, `economy`, `forex`, `charts`, `calendar`, `news`, `research`, `quant`, `ai` et `sources` ; les chapitres de l'aide sont les mêmes, plus `start`, `devtools` et `data`.
