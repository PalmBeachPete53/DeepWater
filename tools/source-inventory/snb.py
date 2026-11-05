#!/usr/bin/env python3
"""SNB data portal: the cubes listed by its sitemap. Topic cubes (242): title, source, dimensions, first and last date,
series count; warehouse cubes (912): title and publishing title. The data file is streamed, only keys and dates are
looked at, no value is kept. Resumable.

    python3 -I snb.py sitemap.xml docs/sources/catalog/snb.tsv docs/sources/catalog/snb_structures.jsonl calls.log
"""
import json, re, sys, time, urllib.request, urllib.error, urllib.parse
sm, tsv, jsonl, logp = sys.argv[1:5]
H = {"User-Agent": "Deepwater/1.0"}
log = open(logp, "a")
def fetch(url, stream=False):
    try:
        r = urllib.request.urlopen(urllib.request.Request(url, headers=H), timeout=120)
        b = r.read(); st = r.status
    except urllib.error.HTTPError as e:
        b = e.read(); st = e.code
    except Exception as e:
        b = b""; st = type(e).__name__
    log.write(f"GET {url} -> {st}, {len(b)} B\n"); log.flush()
    time.sleep(0.6)
    return st, b
x = open(sm, encoding="utf8").read()
hrefs = sorted(set(re.findall(r'hreflang="en" href="([^"]+)"', x)))
cubes = []
for h in hrefs:
    m = re.search(r"/en/(topics|warehouse)/([^/]+)/cube/([^/]+)$", h)
    if m: cubes.append((m.group(1), m.group(2), m.group(3)))
done = set()
try:
    done = {json.loads(l)["id"] for l in open(jsonl)}
except FileNotFoundError:
    pass
J = open(jsonl, "a")
ts = "20261105_144141"
for kind, group, cid in cubes:
    if cid in done: continue
    wh = kind == "warehouse"
    rec = {"id": cid, "kind": kind, "group": group}
    st, b = fetch("https://data.snb.ch/json/table/getCubeInfo?" + urllib.parse.urlencode({"lang": "en", "isWarehouse": str(wh).lower(), "cubeId": cid, "pageViewTime": ts}))
    if st == 200:
        i = json.loads(b)
        rec.update(title=i.get("title"), publishing=i.get("publishingTitle"), source=i.get("source"), public_since=i.get("publicSinceDate"))
    else:
        rec["info_status"] = st
    if not wh:
        st, b = fetch(f"https://data.snb.ch/api/cube/{cid}/dimensions/en")
        if st == 200:
            d = json.loads(b)
            def cnt(items):
                n = 0
                for it in items:
                    if "dimensionItems" in it and it["dimensionItems"]: n += cnt(it["dimensionItems"])
                    else: n += 1
                return n
            rec["dimensions"] = [{"id": k["id"], "name": k.get("name"), "codes": cnt(k.get("dimensionItems", []))} for k in d.get("dimensions", [])]
        st, b = fetch(f"https://data.snb.ch/api/cube/{cid}/data/csv/en")
        if st == 200:
            lines = b.decode("utf-8-sig", "replace").splitlines()
            head = next((i for i, l in enumerate(lines) if l.startswith('"Date"')), None)
            if head is not None:
                dates, keys = set(), set()
                for l in lines[head + 1:]:
                    c = [p.strip('"') for p in l.split(";")]
                    if len(c) < 2: continue
                    dates.add(c[0]); keys.add(";".join(c[1:-1]))
                if dates:
                    rec.update(first=min(dates), last=max(dates), series=len(keys), periods=len(dates))
        else:
            rec["data_status"] = st
    J.write(json.dumps(rec, ensure_ascii=False) + "\n"); J.flush()
J.close()
with open(tsv, "w") as T:
    T.write("cube\tkind\tgroup\ttitle\tpublishing\tsource\tdimensions\tseries\tfirst\tlast\tpublic_since\n")
    for l in open(jsonl):
        r = json.loads(l)
        ds = " | ".join(f"{d.get('name')}({d.get('codes')})" for d in r.get("dimensions", []))
        T.write("\t".join(str(v if v is not None else "").replace("\t", " ") for v in [r["id"], r["kind"], r["group"], r.get("title"), r.get("publishing"), r.get("source"), ds, r.get("series"), r.get("first"), r.get("last"), r.get("public_since")]) + "\n")
print("done", len(cubes))
