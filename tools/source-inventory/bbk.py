#!/usr/bin/env python3
"""Deutsche Bundesbank: dataflows, their DSDs, and per flow the series count, frequencies, first and last period.

The data endpoint ignores detail= and lastNObservations= when no key is given and sends the whole flow, so the flow is
streamed and only the key columns are counted: no value is kept.

    python3 -I bbk.py docs/sources/catalog/bbk.tsv docs/sources/catalog/bbk_structures.jsonl calls.log
"""
import csv, json, re, sys, time, urllib.request, xml.etree.ElementTree as ET
BASE = "https://api.statistiken.bundesbank.de/rest"
UA = {"User-Agent": "Deepwater/1.0"}
NS = {"s": "http://www.sdmx.org/resources/sdmxml/schemas/v2_1/structure", "c": "http://www.sdmx.org/resources/sdmxml/schemas/v2_1/common"}
XL = "{http://www.w3.org/XML/1998/namespace}lang"
tsv, jsonl, logp = sys.argv[1:4]
log = open(logp, "a")

def fetch(url, timeout=120):
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=timeout) as r:
        b = r.read()
    log.write(f"GET {url} -> 200, {len(b)} B\n"); log.flush()
    time.sleep(0.8)
    return b

def name(e):
    n = {x.get(XL): x.text for x in e.findall("c:Name", NS)}
    return (n.get("en") or next(iter(n.values()), "") or "").strip()

root = ET.fromstring(fetch(BASE + "/metadata/dataflow/BBK"))
flows = [(f.get("id"), name(f), f.find("s:Structure/Ref", NS).get("id")) for f in root.iter("{%s}Dataflow" % NS["s"])]
dsds = {}
def dsd(d):
    if d in dsds: return dsds[d]
    r = ET.fromstring(fetch(f"{BASE}/metadata/datastructure/BBK/{d}?references=children"))
    cl = {c.get("id"): (name(c), len(c.findall("s:Code", NS))) for c in r.iter("{%s}Codelist" % NS["s"])}
    out = []
    for x in r.iter("{%s}Dimension" % NS["s"]):
        if x.get("id") is None:   # an attribute's relationship, not a dimension of the DSD
            continue
        ref = x.find("s:LocalRepresentation/s:Enumeration/Ref", NS)
        cid = ref.get("id") if ref is not None else ""
        out.append({"id": x.get("id"), "codelist": cid, "codes": cl.get(cid, ("", 0))[1], "label": cl.get(cid, ("", 0))[0]})
    attrs = [x.get("id") for x in r.iter("{%s}Attribute" % NS["s"])]
    dsds[d] = (out, attrs, name(next(r.iter("{%s}DataStructure" % NS["s"]))))
    return dsds[d]

done = set()
try:
    done = {json.loads(l)["id"] for l in open(jsonl)}
except FileNotFoundError:
    pass
J = open(jsonl, "a")
for fid, fname, d in flows:
    if fid in done: continue
    rec = {"id": fid, "name": fname, "dsd": d}
    dims, attrs, dn = dsd(d)
    rec["dimensions"] = dims; rec["attributes"] = attrs
    url = f"{BASE}/data/{fid}?format=sdmx_csv&lang=en"
    bytes_ = 0
    try:
        with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=300) as r:
            import io
            txt = io.TextIOWrapper(r, encoding="utf-8-sig", newline="")
            first_line = txt.readline()   # "," with lang=en, ";" without
            delim = "," if first_line.count(",") > first_line.count(";") else ";"
            head = next(csv.reader([first_line], delimiter=delim))
            rd = csv.reader(txt, delimiter=delim)
            ti, fi = head.index("TIME_PERIOD"), 1
            ids = head.index("BBK_ID") if "BBK_ID" in head else None
            ser, freq, first, last, obs = set(), {}, None, None, 0
            for row in rd:
                if len(row) <= ti: continue
                obs += 1
                k = row[ids] if ids is not None else ";".join(row[1:ti])
                if k not in ser:
                    ser.add(k); freq[row[fi]] = freq.get(row[fi], 0) + 1
                t = row[ti]
                if first is None or t < first: first = t
                if last is None or t > last: last = t
            rec.update(series=len(ser), observations=obs, frequencies=freq, first=first, last=last, columns=head[1:])
        log.write(f"GET {url} -> 200, {obs} rows streamed\n")
    except Exception as e:
        rec["error"] = f"{type(e).__name__}: {str(e)[:150]}"
        log.write(f"GET {url} -> {rec['error']}\n")
    log.flush()
    J.write(json.dumps(rec, ensure_ascii=False) + "\n"); J.flush()
    print(fid, rec.get("series"), rec.get("first"), rec.get("last"), rec.get("error", ""), flush=True)
    time.sleep(1)
J.close()
with open(tsv, "w") as T:
    T.write("flow\tname\tdsd\tdimensions\tseries\tobservations\tfrequencies\tfirst\tlast\n")
    for l in open(jsonl):
        r = json.loads(l)
        T.write("\t".join(str(x) for x in [r["id"], r["name"], r["dsd"], " ".join(f"{x['id'].replace('BBK_','')}({x['codes']})" for x in r["dimensions"]), r.get("series", ""), r.get("observations", ""), ",".join(f"{k}:{v}" for k, v in sorted(r.get("frequencies", {}).items())), r.get("first", ""), r.get("last", "")]) + "\n")
