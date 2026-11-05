#!/usr/bin/env python3
"""A .Stat Suite / NSI web service: the dataflows, then per flow its dimensions, codelist sizes, actual availability
(frequencies, first and last period, observation count). Structure only, no observation. Resumable.

    python3 -I nbb.py <rest base> <agency|all> out.tsv out.jsonl calls.log [pause]
e.g. nbb.py https://nsidisseminate-stat.nbb.be/rest all docs/sources/catalog/nbb.tsv docs/sources/catalog/nbb_structures.jsonl calls.log 1.2
"""
import json, re, sys, time, urllib.request, urllib.error
base, agency, tsv, jsonl, logp = sys.argv[1:6]
pause = float(sys.argv[6]) if len(sys.argv) > 6 else 1.2
H = {"User-Agent": "Deepwater/1.0", "Accept": "application/vnd.sdmx.structure+json;version=1.0"}
log = open(logp, "a")
def fetch(url, timeout=120):
    try:
        with urllib.request.urlopen(urllib.request.Request(url, headers=H), timeout=timeout) as r:
            b = r.read(); st = r.status
    except urllib.error.HTTPError as e:
        b = e.read(); st = e.code
    except Exception as e:
        b = b""; st = type(e).__name__
    log.write(f"GET {url} -> {st}, {len(b)} B\n"); log.flush()
    time.sleep(pause)
    return st, b
st, b = fetch(f"{base}/dataflow/{agency}/all/latest")
flows = json.loads(b)["data"]["dataflows"]
print(len(flows), flush=True)
def nm(o):
    return o.get("name") or next(iter((o.get("names") or {}).values()), "")
done = set()
try:
    done = {(json.loads(l)["agency"], json.loads(l)["id"]) for l in open(jsonl)}
except FileNotFoundError:
    pass
J = open(jsonl, "a")
for f in flows:
    if (f["agencyID"], f["id"]) in done: continue
    rec = {"agency": f["agencyID"], "id": f["id"], "version": f["version"], "name": nm(f), "description": re.sub(r"<[^>]+>", " ", f.get("description") or "")[:400]}
    st, b = fetch(f"{base}/dataflow/{f['agencyID']}/{f['id']}/{f['version']}?references=all")
    rec["structure_status"] = st
    if st == 200:
        d = json.loads(b)["data"]
        cl = {c["id"]: c for c in d.get("codelists", [])}
        cats = [c.get("name") for c in d.get("categorySchemes", [])]
        dims = []
        for dsd in d.get("dataStructures", [])[:1]:
            rec["dsd"] = dsd["id"]
            for x in dsd["dataStructureComponents"]["dimensionList"]["dimensions"]:
                ref = x.get("localRepresentation", {}).get("enumeration", "")
                cid = ref.split(":")[-1].split("(")[0] if ref else ""
                dims.append({"id": x["id"], "codelist": cid, "codes": len(cl.get(cid, {}).get("codes", []))})
            rec["attributes"] = [a["id"] for a in dsd["dataStructureComponents"].get("attributes", {}).get("attributeList", {}).get("attributes", dsd["dataStructureComponents"].get("attributes", {}).get("attributes", []))] if isinstance(dsd["dataStructureComponents"].get("attributes"), dict) else []
        rec["dimensions"] = dims
    st, b = fetch(f"{base}/availableconstraint/{f['agencyID']},{f['id']},{f['version']}/all/all/all")
    rec["avail_status"] = st
    if st == 200:
        try:
            c = json.loads(b)["data"]["contentConstraints"][0]
            for a in c.get("annotations", []):
                if a.get("id") == "obs_count": rec["observations"] = a.get("title")
            for r in c.get("cubeRegions", []):
                for k in r.get("keyValues", []):
                    if k["id"] == "TIME_PERIOD" and "timeRange" in k:
                        rec["first"] = k["timeRange"].get("startPeriod", {}).get("period", "")[:10]
                        rec["last"] = k["timeRange"].get("endPeriod", {}).get("period", "")[:10]
                    elif k["id"] in ("FREQ", "FREQUENCY", "SIKLIK"):
                        rec["frequencies"] = k.get("values")
                    else:
                        for dm in rec.get("dimensions", []):
                            if dm["id"] == k["id"]: dm["available"] = len(k.get("values", []))
        except Exception as e:
            rec["avail_error"] = repr(e)[:100]
    J.write(json.dumps(rec, ensure_ascii=False) + "\n"); J.flush()
    print(f["id"], rec.get("observations"), rec.get("first"), rec.get("last"), flush=True)
J.close()
with open(tsv, "w") as T:
    T.write("agency\tflow\tversion\tname\tdimensions\tfrequencies\tobservations\tfirst\tlast\n")
    for l in open(jsonl):
        r = json.loads(l)
        ds = " ".join(f"{x['id']}({x.get('available', x.get('codes'))})" for x in r.get("dimensions", []))
        T.write("\t".join(str(v).replace("\t", " ").replace("\n", " ") for v in [r["agency"], r["id"], r["version"], r["name"], ds, ",".join(r.get("frequencies") or []), r.get("observations", ""), r.get("first", ""), r.get("last", "")]) + "\n")
print("done")
