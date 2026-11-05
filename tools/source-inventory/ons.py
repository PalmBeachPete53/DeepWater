#!/usr/bin/env python3
"""ONS: the CMD catalogue with each data set's dimensions, then every time series and dataset page from the search API.
Structure only. Paced at 0.6 s. The search API stops at offset 10,000: see the card for how the list was completed.

    python3 -I ons.py docs/sources/catalog calls.log
"""
import json, re, sys, time, urllib.request, urllib.error, collections
cat, logp = sys.argv[1:3]
log = open(logp, "a")
H = {"User-Agent": "Deepwater/1.0"}
def fetch(url):
    try:
        with urllib.request.urlopen(urllib.request.Request(url, headers=H), timeout=60) as r:
            b = r.read(); st = r.status
    except urllib.error.HTTPError as e:
        b = e.read(); st = e.code
    except Exception as e:
        b = b""; st = type(e).__name__
    log.write(f"GET {url} -> {st}, {len(b)} B\n"); log.flush()
    time.sleep(0.6)
    return st, b
def clean(s): return re.sub(r"[\t\r\n]+", " ", str(s if s is not None else "")).strip()
B = "https://api.beta.ons.gov.uk/v1"
# CMD
st, b = fetch(B + "/datasets?limit=1000")
items = json.loads(b)["items"]
with open(cat + "/ons.tsv", "w") as T:
    T.write("id\ttype\ttitle\tfrequency\tnational_statistic\tlast_updated\tedition\tversion\tdimensions\talert\n")
    for it in items:
        lv = it.get("links", {}).get("latest_version", {}).get("href")
        dims, ed, ver, alert = "", "", "", ""
        if lv:
            st, b = fetch(lv)
            if st == 200:
                v = json.loads(b)
                dims = " | ".join(f"{d.get('name')}[{(d.get('id') or d.get('href','').split('/')[-1])}]" for d in v.get("dimensions", []))
                ed, ver = v.get("edition"), v.get("version")
                alert = " ".join(a.get("description", "") for a in v.get("alerts", []) or [])[:200]
        T.write("\t".join(clean(x) for x in [it["id"], it.get("type"), it.get("title"), it.get("release_frequency"), it.get("national_statistic"), (it.get("last_updated") or "")[:10], ed, ver, dims, alert]) + "\n")
# search: every time series, every dataset page
def walk(ct, out, cols):
    n = 0; seen = set(); offset = 0; total = None
    with open(out, "w") as T:
        T.write("\t".join(cols) + "\n")
        while True:
            st, b = fetch(f"{B}/search?content_type={ct}&limit=1000&offset={offset}&sort=title")
            if st != 200:
                print(ct, "stop at", offset, st, b[:150]); break
            d = json.loads(b); total = d["count"]
            if not d["items"]: break
            for i in d["items"]:
                if i["uri"] in seen: continue
                seen.add(i["uri"])
                T.write("\t".join(clean(i.get(c)) for c in cols) + "\n"); n += 1
            offset += len(d["items"])
            if offset >= total: break
    print(ct, "total", total, "written", n, flush=True)
walk("timeseries", cat + "/ons_timeseries.tsv", ["cdid", "dataset_id", "title", "uri", "release_date"])
walk("dataset_landing_page", cat + "/ons_datasets.tsv", ["title", "uri", "release_date", "summary"])
print("done")
