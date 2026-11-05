#!/usr/bin/env python3
"""ADB's Key Indicators Database: its dataflows with their indicators, economies and periods, and the indicator list.

The flows' names come from the /api page; each flow's annual data is read for its axes and thrown away. One
call at a time, two seconds apart. Run with `python3 -I`.

    adb.py docs/sources/catalog/adb.tsv docs/sources/catalog/adb_indicators.tsv
"""
import csv
import html
import io
import json
import re
import sys
import time
import urllib.request

BASE = "https://kidb.adb.org/api/v5/sdmx"
HDR = {"User-Agent": "Deepwater/1.0"}


def fetch(url, timeout=180):
    time.sleep(2)
    with urllib.request.urlopen(urllib.request.Request(url, headers=HDR), timeout=timeout) as r:
        return r.read()


page = fetch("https://kidb.adb.org/api").decode("utf-8", "replace")
props = json.loads(html.unescape(re.search(r'data-page="([^"]*)"', page).group(1)))["props"]
codes = json.loads(fetch(f"{BASE}/structure/codelist/ADB/CL_KIDB_INDICATORS/+?format=sdmx-json"))["data"]["codelists"][0]["codes"]
with open(sys.argv[2], "w", encoding="utf-8") as f:
    f.write("code\tname\n")
    for c in codes:
        name = c.get("name") or (c.get("names") or {}).get("en", "")
        f.write(f"{c['id']}\t{' '.join(name.split())}\n")
print(len(codes), "indicators", flush=True)
with open(sys.argv[1], "w", encoding="utf-8") as out:
    out.write("flow\tname\trows\tindicators\teconomies\tfreqs\tfirst\tlast\tcolumns\n")
    for fl in props["dataflows"]:
        fid = fl["code"]
        cols, n, ind, eco, freq, lo, hi, err = [], 0, set(), set(), set(), "", "", ""
        try:
            key = "A...." if fid == "DF_KIDB_ITG" else "A.."
            text = fetch(f"{BASE}/data/ADB,{fid}/{key}?format=sdmx-csv").decode("utf-8", "replace")
            rd = csv.DictReader(io.StringIO(text))
            cols = rd.fieldnames or []
            for r in rd:
                n += 1
                ind.add(r.get("INDICATOR"))
                eco.add(r.get("ECONOMY_CODE"))
                freq.add(r.get("FREQ"))
                t = r.get("TIME_PERIOD") or ""
                if t:
                    lo, hi = min(lo or t, t), max(hi, t)
        except Exception as e:
            err = str(e)
        print(fid, n, err, flush=True)
        out.write("\t".join(str(x) for x in (fid, fl["name"], n if not err else err, len(ind), len(eco), ",".join(sorted(x for x in freq if x)),
                                             lo, hi, " ".join(cols))) + "\n")
        out.flush()
