# Developer tools

The `</>` icon in the header shows a small **DEV** flap on the right edge of the page area; both the icon and the open panel are remembered. Clicking the flap slides a full-width tabbed panel over the pages.

## Codebase

The project's code graph, as built by Graphify from the sources (`graphify-out/graph.json`, found next to the app or in the working directory), in 3D: the dots are functions, classes and files, coloured by community, with dashed, arrowed links across communities. Names show on hover or selection; the sidebar has a node search, the *Node info* panel (type, community, source or members, degree, clickable neighbours), the *Communities* legend with a checkbox per community, and the counts. Click a dot to inspect it, double-click to open its community; drag to orbit, wheel to zoom, `Esc` clears the selection. The files are reloaded on their own when the graph is rebuilt.

## Command-line flags

For screenshots and checks, the app can open on a page and render it to a file:

```
deepwater [--theme <id>] --page <id> [--devtools] [--country FRA] [--indicator <n>]
          [--economy-country USA [--economy-tab 2]] [--economy-indicator "Inflation Rate"]
          [--news-ticker NVDA] [--news-article 0] [--news-url https://…]
          [--sources-tab 1] [--sources-disc [floor:1|ai:0]] [--settings [0|1]] [--help-page [chapter]]
          [--ai-search <text>] [--ai-local <prompt>] [--ai-dock [prompt]]
          [--scroll <0..1> | --scroll-end] --delay <ms> --screenshot out.png
```

`--theme` is for that run only. `--settings` and `--help-page` shoot their floating window. `deepwater --pine-js file.pine` prints the JavaScript PineTS runs for a Pine Script and exits.

The page ids are `map`, `overview`, `economy`, `forex`, `charts`, `calendar`, `news`, `research`, `quant`, `ai` and `sources`; the help chapters are the same, plus `start`, `devtools` and `data`.
