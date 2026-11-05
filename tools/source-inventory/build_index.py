#!/usr/bin/env python3
"""Writes docs/sources/INDEX.md: the topic index below, then one line per card read from the cards themselves.

Edit TOPICS here when a card changes what a topic points to.

    build_index.py   (from the repository's root)
"""
import glob
import re

TOPICS = """\
## Who has what

Start from the topic, then open the card. `catalogue-bodies.md` covers the bodies of the catalogue by country, for what no card
holds (a country's tax office, a national labour ministry, a small central bank).

| Looking for | Cards |
|---|---|
| Inflation, CPI, PPI | [oecd](oecd.md) (54 areas, `CP`, `PP`), [imf](imf.md), [eurostat](eurostat.md), [ilo](ilo.md), [bls](bls.md), national offices |
| Unemployment, jobs, wages | [oecd](oecd.md), [ilo](ilo.md), [eurostat](eurostat.md), [bls](bls.md), [fred](fred.md), national offices |
| GDP, national accounts | [oecd](oecd.md), [imf](imf.md), [worldbank](worldbank.md), [eurostat](eurostat.md), [bea](bea.md), [european_commission](european_commission.md) (AMECO), national offices |
| Forecasts, projections | [imf](imf.md) (WEO), [oecd](oecd.md) (Economic Outlook), [european_commission](european_commission.md), [phillyfed](phillyfed.md) (SPF, Livingston), [boc](boc.md), [ing](ing.md), [bcb](bcb.md) (Focus) |
| Policy rates, money markets | [bis](bis.md), [nyfed](nyfed.md), [fedboard](fedboard.md), [ecb](ecb.md), [boe](boe.md), [boj](boj.md), [boc](boc.md), [snb](snb.md), [riksbank](riksbank.md), [norges](norges.md), [cbrt](cbrt.md) |
| Government yields, debt, auctions | [treasury](treasury.md), [boc](boc.md), [boe](boe.md), [mofjapan](mofjapan.md), [boi](boi.md), [oecd](oecd.md), [fred](fred.md) |
| Exchange rates | [frankfurter](frankfurter.md), [ecb](ecb.md), [bis](bis.md), [boc](boc.md), [riksbank](riksbank.md), [norges](norges.md), [bcb](bcb.md) |
| Credit, banks, balance sheets | [bis](bis.md), [fedboard](fedboard.md), [ecb](ecb.md), [boe](boe.md), [bbk](bbk.md), [bdf](bdf.md), [boc](boc.md), [boj](boj.md), [snb](snb.md) |
| House prices, housing | [oecd](oecd.md), [bis](bis.md), [freddiemac](freddiemac.md), [census](census.md), [fred](fred.md) |
| Surveys, confidence, PMI | [oecd](oecd.md) (`BCICP`, `CCICP`, BTS), [european_commission](european_commission.md), [boj](boj.md) (Tankan), [phillyfed](phillyfed.md), [fed-regional](fed-regional.md), [private-institutes](private-institutes.md) (S&P Global, ISM, ZEW, Ifo, Michigan: mostly refused or headline only) |
| Trade flows, balance of payments | [un](un.md) (Comtrade), [imf](imf.md), [oecd](oecd.md), [eurostat](eurostat.md), [census](census.md), [treasury](treasury.md) (TIC) |
| Positioning, sentiment | [cftc](cftc.md) (COT), [myfxbook](myfxbook.md), [alternative](alternative.md), [coinmarketcap](coinmarketcap.md), [binance](binance.md) (long-short, open interest) |
| Energy, oil stocks | [eia](eia.md), [jodi](jodi.md), [opec](opec.md) (refused), [eurostat](eurostat.md), [private-institutes](private-institutes.md) (Baker Hughes) |
| Commodities, agriculture | [imf](imf.md) (PCPS, WEO projections), [yahoo](yahoo.md), [usda](usda.md) (PSD), [fao](fao.md), [wgc](wgc.md), [worldsteel](worldsteel.md), [boc](boc.md) (BCPI) |
| Shipping, ports, freight | [aisstream](aisstream.md), [imf](imf.md) (PortWatch), [oecd](oecd.md) (freight by mode), [nyfed](nyfed.md) (GSCPI), [fred](fred.md), [bundled](bundled.md) |
| Stocks, company data, filings | [yahoo](yahoo.md), [nasdaq](nasdaq.md), [sec](sec.md), [polygon](polygon.md), [alpaca](alpaca.md), [fmp](fmp.md), [wikidata](wikidata.md), [wikipedia](wikipedia.md) (index members) |
| Crypto markets | [binance](binance.md), [coinbase](coinbase.md), [kraken](kraken.md), [coinmarketcap](coinmarketcap.md), [alternative](alternative.md), [cryptonews](cryptonews.md) |
| Order book, trades, futures | [binance](binance.md), [coinbase](coinbase.md), [kraken](kraken.md), [polygon](polygon.md) |
| Economic calendar | [tradingview](tradingview.md), [forexfactory](forexfactory.md), [nasdaq](nasdaq.md) (corporate) |
| News, research notes | [newsfeeds](newsfeeds.md), [investinglive](investinglive.md), [ing](ing.md), [cryptonews](cryptonews.md), [fmp](fmp.md), [nasdaq](nasdaq.md), [alpaca](alpaca.md) |
| Development, health, society | [worldbank](worldbank.md), [who](who.md), [unhcr](unhcr.md), [sesric](sesric.md), [unece](unece.md), [transparency](transparency.md), [iep](iep.md), [wef](wef.md), [sipri](sipri.md) |
| Regions: Africa | [afdb](afdb.md) (refused), [bceao](bceao.md), [beac](beac.md), [sarb](sarb.md), [statssa](statssa.md) (refused) |
| Regions: Latin America | [secmca](secmca.md), [iadb](iadb.md), [bcb](bcb.md), [ibge](ibge.md), [banxico](banxico.md), [inegi](inegi.md), [indec](indec.md) |
| Regions: Asia-Pacific | [adb](adb.md), [boj](boj.md), [bok](bok.md), [abs](abs.md), [singstat](singstat.md), [hkcsd](hkcsd.md), [psa](psa.md), [mospi](mospi.md), [statsnz](statsnz.md), [bps](bps.md), [bot](bot.md) |
| Local: bundled, computed, imported | [bundled](bundled.md), [deepwater](deepwater.md), [imports](imports.md) |
| Not data: services | [anthropic](anthropic.md), [huggingface](huggingface.md), [adblock](adblock.md) |
"""

HEAD = """\
# What the sources hold: index

See [README.md](README.md) for what this folder is and how far to trust it. Before deciding that a piece of data
doesn't exist, or adding a source: check the topic below, then grep `catalog/` (`zgrep` for the `.gz` lists).

```sh
zgrep -il "bank lending" docs/sources/catalog/*       # which source has a data set by that name
grep -i "port calls" docs/sources/*.md                # the cards
```

"""


def main():
    rows = []
    n = {"called": 0, "docs": 0, "refused": 0}
    for path in sorted(glob.glob("docs/sources/*.md")):
        name = path.split("/")[-1][:-3]
        text = open(path, encoding="utf-8").read()
        m = re.match(r"---\n(.*?)\n---\n", text, re.S)
        if not m:
            continue
        front = m.group(1)

        def field(key):
            f = re.search(rf"^{key}:\s*(.*?)(\s+#.*)?$", front, re.M)
            return f.group(1).strip() if f else ""

        para = re.search(r"^# [^\n]*\n+(.+?)\n\n", text[m.end():], re.S | re.M)
        summary = re.sub(r"\s+", " ", para.group(1)) if para else ""
        first = re.match(r"(.+?[.:])(\s|$)", summary)
        conf = field("confidence")
        n[conf] = n.get(conf, 0) + 1
        rows.append(f"| [{name}]({name}.md) | {(first.group(1) if first else summary)[:170].replace('|', '/')} "
                    f"| {field('access')} | {conf} |")
    with open("docs/sources/INDEX.md", "w", encoding="utf-8") as f:
        f.write(HEAD + TOPICS)
        f.write(f"\n## The cards\n\n{len(rows)} cards. **called**: its catalogue or a sample answered ({n.get('called', 0)}). "
                f"**docs**: public documentation and code only, usually for lack of a key ({n.get('docs', 0)}). "
                f"**refused**: it refused or failed, so the card says what could not be seen ({n.get('refused', 0)}).\n\n"
                "| Card | Holds | Access | Confidence |\n|---|---|---|---|\n")
        f.write("\n".join(rows) + "\n")
    print(len(rows), "cards", n)
    missing = sorted(set(re.findall(r"\]\(([a-z_\-]+)\.md\)", TOPICS)) - {r.split("[")[1].split("]")[0] for r in rows})
    print("topic links without a card:", missing)


if __name__ == "__main__":
    main()
