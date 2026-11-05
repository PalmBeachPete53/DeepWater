#!/usr/bin/env python3
"""INSEE BDM (SDMX): dataflows, then per flow the DSD (dimensions, codelist sizes), the series count and the first and
last period over its series (firstN/lastNObservations=1; only TIME_PERIOD is kept). Melodi catalogue: per data set its
concepts and periods. Paced at 2.3 s. Resumable.

    python3 -I insee.py docs/sources/catalog/insee_bdm.tsv docs/sources/catalog/insee_bdm_structures.jsonl docs/sources/catalog/insee_melodi.tsv calls.log
"""
import json, re, sys, time, urllib.request, urllib.error
tsv, jsonl, mel, logp = sys.argv[1:5]
UA = {"User-Agent": "Deepwater/1.0"}
log = open(logp, "a")
def fetch(url, timeout=180):
    try:
        with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=timeout) as r:
            b = r.read(); st = r.status
    except urllib.error.HTTPError as e:
        b = e.read(); st = e.code
    except Exception as e:
        b = b""; st = type(e).__name__
    log.write(f"GET {url} -> {st}, {len(b)} B\n"); log.flush()
    time.sleep(2.3)
    return st, b
BASE = "https://bdm.insee.fr/series/sdmx"
st, b = fetch(BASE + "/dataflow")
x = b.decode()
flows = []
for m in re.finditer(r'<str:Dataflow id="([^"]+)".*?</str:Dataflow>', x, re.S):
    bl = m.group(0)
    en = re.search(r'<com:Name xml:lang="en">([^<]*)', bl); fr = re.search(r'<com:Name xml:lang="fr">([^<]*)', bl)
    n = re.search(r'Nombre de séries : (\d+)', bl)
    flows.append((m.group(1), (en or fr).group(1).strip(), int(n.group(1)) if n else None))
done = set()
try:
    done = {json.loads(l)["id"] for l in open(jsonl)}
except FileNotFoundError:
    pass
J = open(jsonl, "a")
for fid, name, nser in flows:
    if fid in done: continue
    rec = {"id": fid, "name": name, "series": nser}
    st, b = fetch(f"{BASE}/datastructure/FR1/{fid}?references=children")
    t = b.decode("utf8", "replace")
    cl = {}
    for c in re.finditer(r'<str:Codelist [^>]*id="([^"]+)".*?</str:Codelist>', t, re.S):
        cl[c.group(1)] = len(re.findall(r'<str:Code ', c.group(0)))
    dims = []
    for d in re.finditer(r'<str:Dimension [^>]*id="([^"]+)".*?</str:Dimension>', t, re.S):
        ref = re.search(r'<Enumeration>\s*<Ref [^>]*id="([^"]+)"|<str:Enumeration>\s*<Ref [^>]*id="([^"]+)"', d.group(0))
        cid = (ref.group(1) or ref.group(2)) if ref else ""
        dims.append({"id": d.group(1), "codelist": cid, "codes": cl.get(cid)})
    rec["dimensions"] = dims
    per = []
    for q in ("firstNObservations", "lastNObservations"):
        st, b = fetch(f"{BASE}/data/{fid}?{q}=1&detail=dataonly")
        ps = re.findall(rb'TIME_PERIOD="([^"]+)"', b)
        rec[q + "_status"] = st
        per.append(sorted(set(p.decode() for p in ps)))
    if per[0]: rec["first"] = per[0][0]
    if per[1]: rec["last"] = per[1][-1]
    # Frequencies from the last-observation answer's FREQ attribute
    freqs = {}
    for f in re.findall(rb'<Series [^>]*?FREQ="([^"]+)"', b):
        freqs[f.decode()] = freqs.get(f.decode(), 0) + 1
    rec["frequencies"] = freqs
    J.write(json.dumps(rec, ensure_ascii=False) + "\n"); J.flush()
    print(fid, nser, rec.get("first"), rec.get("last"), flush=True)
J.close()
with open(tsv, "w") as T:
    T.write("flow\tname\tseries\tdimensions\tfrequencies\tfirst\tlast\n")
    for l in open(jsonl):
        r = json.loads(l)
        T.write("\t".join(str(v) for v in [r["id"], r["name"], r.get("series", ""), " ".join(f"{d['id']}({d['codes']})" for d in r["dimensions"]), ",".join(f"{k}:{v}" for k, v in sorted(r.get("frequencies", {}).items())), r.get("first", ""), r.get("last", "")]) + "\n")

# Melodi: one catalogue call holds everything per data set
st, b = fetch("https://api.insee.fr/melodi/catalog/all")
def en(a):
    a = a or []
    return next((i["content"] for i in a if i.get("lang") == "en" and i.get("content")), next((i["content"] for i in a if i.get("content")), ""))
with open(mel, "w") as T:
    T.write("dataset\ttitle\tresolution\tstart\tend\tseries\tobservations\tfrequency\tdsd\tmodified\tcomponents\n")
    for e in json.loads(b):
        tr = " ".join(en(x.get("label")) for x in e.get("temporalResolution") or [])
        sr = ",".join(x.get("id", "") for x in e.get("spatialResolution") or [])
        t = e.get("temporal") or {}
        T.write("\t".join(re.sub(r"[\t\n]", " ", str(x)) for x in [e["identifier"], en(e.get("title")), sr[:60], t.get("startPeriod", "")[:10], t.get("endPeriod", "")[:10], e.get("numSeries"), e.get("numObservations"), tr, (e.get("structure") or {}).get("dsd", ""), (e.get("modified") or "")[:10], " ".join(e.get("ordreComposants", []))[:400]]) + "\n")
