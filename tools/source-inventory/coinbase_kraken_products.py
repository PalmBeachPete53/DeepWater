#!/usr/bin/env python3
"""Coinbase Exchange products and Kraken pairs -> docs/sources/catalog/coinbase.tsv, kraken.tsv, and the counts for the cards.
Three public metadata calls, no key, no prices: minimum sizes and increments are left out.

usage: python3 -I tools/source-inventory/coinbase_kraken_products.py [docs/sources/catalog]
"""
import collections
import json
import os
import sys
import time
import urllib.request

UA = {"User-Agent": "Deepwater/1.0"}


def get(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=60) as r:
        body = r.read()
    print("GET", url, "->", r.status, len(body), "bytes")
    return json.loads(body)


def top(counter, n=15):
    return ", ".join("%s %d" % kv for kv in counter.most_common(n))


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "docs/sources/catalog"
    products = get("https://api.exchange.coinbase.com/products")
    print("coinbase products", len(products), "status:", top(collections.Counter(p["status"] for p in products)))
    online = [p for p in products if p["status"] == "online"]
    print("quote (online):", top(collections.Counter(p["quote_currency"] for p in online)))
    print("bases", len({p["base_currency"] for p in products}), "online", len({p["base_currency"] for p in online}))
    print("flags true:", {k: sum(1 for p in products if p.get(k)) for k in
                          ("trading_disabled", "post_only", "limit_only", "cancel_only", "auction_mode", "fx_stablecoin", "margin_enabled")})
    with open(os.path.join(out, "coinbase.tsv"), "w") as f:
        f.write("product\tstatus\tbase\tquote\n")
        for p in sorted(products, key=lambda p: p["id"]):
            f.write("\t".join((p["id"], p["status"], p["base_currency"], p["quote_currency"])) + "\n")
    time.sleep(2)

    currencies = get("https://api.exchange.coinbase.com/currencies")
    print("coinbase currencies", len(currencies), "status:", top(collections.Counter(c["status"] for c in currencies)),
          "type:", top(collections.Counter(c["details"]["type"] for c in currencies)))
    time.sleep(2)

    pairs = get("https://api.kraken.com/0/public/AssetPairs")["result"]
    print("kraken pairs", len(pairs), "status:", top(collections.Counter(p.get("status", "") for p in pairs.values())))
    print("quote:", top(collections.Counter(p["wsname"].split("/")[1] for p in pairs.values() if "wsname" in p), 20))
    print("distinct quotes", len({p["wsname"].split("/")[1] for p in pairs.values() if "wsname" in p}),
          "distinct bases", len({p["wsname"].split("/")[0] for p in pairs.values() if "wsname" in p}))
    print("aclass_base:", top(collections.Counter(p.get("aclass_base", "") for p in pairs.values())))
    print("aclass_quote:", top(collections.Counter(p.get("aclass_quote", "") for p in pairs.values())))
    print("pair field names", sorted(next(iter(pairs.values()))))
    with open(os.path.join(out, "kraken.tsv"), "w") as f:
        f.write("pair\taltname\twsname\tbase\tquote\taclass_base\tstatus\n")
        for k in sorted(pairs):
            p = pairs[k]
            f.write("\t".join((k, p.get("altname", ""), p.get("wsname", ""), p.get("base", ""), p.get("quote", ""),
                               p.get("aclass_base", ""), p.get("status", ""))) + "\n")
    time.sleep(2)

    assets = get("https://api.kraken.com/0/public/Assets")["result"]
    print("kraken assets", len(assets), "aclass:", top(collections.Counter(a.get("aclass", "") for a in assets.values())),
          "status:", top(collections.Counter(a.get("status", "") for a in assets.values())))


main()
