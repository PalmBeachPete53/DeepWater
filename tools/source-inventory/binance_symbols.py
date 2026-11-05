#!/usr/bin/env python3
"""Binance instrument lists -> docs/sources/catalog/binance.tsv, and the counts printed for docs/sources/binance.md.

Reads exchangeInfo from the two hosts whose robots.txt lets a client in: data-api.binance.vision (spot, no robots.txt)
and fapi.binance.com (USD-M futures, its robots.txt only names search engines). api.binance.com and api.binance.us
answer `User-agent: * / Disallow: /`, so they are left alone. Two calls, no prices: filters are left out.

usage: python3 -I tools/source-inventory/binance_symbols.py [docs/sources/catalog/binance.tsv]
"""
import collections
import datetime
import json
import sys
import time
import urllib.request

UA = {"User-Agent": "Deepwater/1.0"}
SPOT = "https://data-api.binance.vision/api/v3/exchangeInfo"
FUTURES = "https://fapi.binance.com/fapi/v1/exchangeInfo"


def get(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=60) as r:
        body = r.read()
    print("GET", url, "->", r.status, len(body), "bytes")
    return json.loads(body)


def top(counter, n=12):
    return ", ".join("%s %d" % kv for kv in counter.most_common(n))


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "docs/sources/catalog/binance.tsv"
    spot = get(SPOT)
    time.sleep(2)
    fut = get(FUTURES)

    rows = []
    syms = spot["symbols"]
    print("\nspot keys", sorted(spot), "symbol keys", sorted(syms[0]))
    print("rateLimits", [(r["rateLimitType"], r["interval"], r["intervalNum"], r["limit"]) for r in spot["rateLimits"]])
    print("spot symbols", len(syms), "status:", top(collections.Counter(s["status"] for s in syms)))
    live = [s for s in syms if s["status"] == "TRADING"]
    print("spot TRADING quote assets (%d):" % len({s["quoteAsset"] for s in live}), top(collections.Counter(s["quoteAsset"] for s in live)))
    print("spot base assets", len({s["baseAsset"] for s in syms}), "TRADING", len({s["baseAsset"] for s in live}))
    print("filter types", sorted({f["filterType"] for s in syms for f in s["filters"]}))
    print("order types", sorted({o for s in syms for o in s["orderTypes"]}))
    print("permission labels", sorted({p for s in syms for ps in s.get("permissionSets", []) for p in ps}))
    for s in syms:
        rows.append(("spot", s["symbol"], s["status"], s["baseAsset"], s["quoteAsset"], "", ""))

    fs = fut["symbols"]
    print("\nfutures keys", sorted(fut), "symbol keys", sorted(fs[0]))
    print("rateLimits", [(r["rateLimitType"], r["interval"], r["intervalNum"], r["limit"]) for r in fut["rateLimits"]])
    print("futures symbols", len(fs), "status:", top(collections.Counter(s["status"] for s in fs)))
    print("contract types:", top(collections.Counter(s["contractType"] for s in fs)))
    print("underlying types:", top(collections.Counter(s.get("underlyingType", "") for s in fs)))
    print("underlying subtypes:", top(collections.Counter(u for s in fs for u in s.get("underlyingSubType", [])), 30))
    print("quote assets:", top(collections.Counter(s["quoteAsset"] for s in fs)))
    print("margin assets:", top(collections.Counter(s["marginAsset"] for s in fs)))
    print("assets block", len(fut.get("assets", [])), "field names", sorted(fut["assets"][0]) if fut.get("assets") else [])
    dates = [s["onboardDate"] for s in fs if s.get("onboardDate")]
    d = lambda ms: datetime.datetime.fromtimestamp(ms / 1000, datetime.timezone.utc).strftime("%Y-%m-%d")
    print("onboardDate", d(min(dates)), "..", d(max(dates)))
    for s in fs:
        rows.append(("usdm", s["symbol"], s["status"], s["baseAsset"], s["quoteAsset"], s["contractType"],
                     d(s["onboardDate"]) if s.get("onboardDate") else ""))

    rows.sort()
    with open(out, "w") as f:
        f.write("market\tsymbol\tstatus\tbase\tquote\tcontract\tonboard\n")
        for r in rows:
            f.write("\t".join(r) + "\n")
    print("\nwrote", out, len(rows))


main()
