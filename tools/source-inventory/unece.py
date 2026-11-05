#!/usr/bin/env python3
"""UNECE's PxWeb statistical database: every folder and table, with each table's variables and period range.

Structure only, no observation. One call at a time, a second apart. Run with `python3 -I`.

    unece.py docs/sources/catalog/unece.tsv
"""
import json
import sys
import time
import urllib.parse
import urllib.request

BASE = "https://w3.unece.org/PXWeb2015/api/v1/en/STAT"
HDR = {"User-Agent": "Deepwater/1.0"}


def fetch(path):
    time.sleep(1)
    url = BASE + "".join("/" + urllib.parse.quote(p) for p in path)
    with urllib.request.urlopen(urllib.request.Request(url, headers=HDR), timeout=60) as r:
        return json.load(r)


out = open(sys.argv[1], "w", encoding="utf-8")
out.write("path\ttable\ttitle\tvariables\tcountries\tfirst\tlast\n")
n = 0


def walk(path, trail):
    global n
    for e in fetch(path):
        text = " ".join(e["text"].split())
        if e["type"] == "l":
            walk(path + [e["id"]], trail + [text])
        elif e["type"] == "t":
            vars_, areas, first, last = [], "", "", ""
            try:
                meta = fetch(path + [e["id"]])
                for v in meta["variables"]:
                    vars_.append(f"{v['code']}({len(v['values'])})")
                    if v.get("time") or v["code"].lower() in ("year", "time", "period", "month", "quarter"):
                        first, last = v["valueTexts"][0], v["valueTexts"][-1]
                    if v["code"].lower() in ("country", "countries", "country/region", "region"):
                        areas = str(len(v["values"]))
            except Exception as err:
                vars_ = [f"error: {err}"]
            out.write("\t".join([" > ".join(trail), e["id"], text, " ".join(vars_), areas, first, last]) + "\n")
            out.flush()
            n += 1


walk([], [])
print(n, "tables")
