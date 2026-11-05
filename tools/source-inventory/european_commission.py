#!/usr/bin/env python3
"""DG ECFIN's Redisstat (AMECO, business and consumer surveys, forecast tables): every dataflow and its structure.

Structure only, no observation. One call at a time, a second apart. Resumable. Run with `python3 -I`.

    european_commission.py docs/sources/catalog/european_commission.tsv docs/sources/catalog/european_commission_structures.jsonl
"""
import json
import os
import sys
import time
import urllib.request
import xml.etree.ElementTree as ET

BASE = "https://webgate.ec.europa.eu/ecfin/redisstat/api/dissemination/sdmx/2.1"
HDR = {"User-Agent": "Deepwater/1.0"}
S = "{http://www.sdmx.org/resources/sdmxml/schemas/v2_1/structure}"
C = "{http://www.sdmx.org/resources/sdmxml/schemas/v2_1/common}"
XL = "{http://www.w3.org/XML/1998/namespace}lang"


def fetch(url):
    time.sleep(1)
    with urllib.request.urlopen(urllib.request.Request(url, headers=HDR), timeout=120) as r:
        return ET.fromstring(r.read())


def name_of(el):
    return next((n.text or "" for n in el.findall(C + "Name") if n.get(XL) == "en"), "")


def annotations(flow):
    out = {}
    for a in flow.iter(C + "Annotation"):
        t, v = a.findtext(C + "AnnotationType"), a.findtext(C + "AnnotationTitle")
        if t in ("OBS_COUNT", "SERIES_COUNT", "OBS_PERIOD_OVERALL_OLDEST", "OBS_PERIOD_OVERALL_LATEST", "UPDATE_DATA",
                 "DISSEMINATION_OBJECT_TYPE"):
            out[t] = v
    return out


tsv, jsonl = sys.argv[1], sys.argv[2]
flows = [(d.get("id"), name_of(d)) for d in fetch(f"{BASE}/dataflow/ECFIN/all?detail=allstubs").iter(S + "Dataflow")]
flows.sort()
done = {}
if os.path.exists(jsonl):
    with open(jsonl, encoding="utf-8") as f:
        done = {r["id"]: r for r in map(json.loads, f)}
with open(jsonl, "a", encoding="utf-8") as out:
    for fid, name in flows:
        # "X$Y" are saved views of X, which has the whole structure: they are listed, not fetched.
        if fid in done or "$" in fid:
            continue
        rec = {"id": fid, "name": name}
        try:
            root = fetch(f"{BASE}/dataflow/ECFIN/{fid}?references=descendants&detail=referencepartial")
            rec.update(next((annotations(d) for d in root.iter(S + "Dataflow")), {}))
            lists = {cl.get("id"): {c.get("id"): name_of(c) for c in cl.findall(S + "Code")} for cl in root.iter(S + "Codelist")}
            dims = []
            for d in root.iter(S + "Dimension"):
                if not d.get("id"):  # the constraint's references to a dimension, not dimensions
                    continue
                ref = d.find(f"{S}LocalRepresentation/{S}Enumeration/Ref")
                dims.append({"id": d.get("id"), "codelist": ref.get("id") if ref is not None else ""})
            avail = {}
            for region in fetch(f"{BASE}/contentconstraint/ECFIN/{fid}").iter(S + "CubeRegion"):
                for kv in region.findall(C + "KeyValue"):
                    if kv.get("id") != "TIME_PERIOD":
                        avail[kv.get("id")] = [v.text for v in kv.findall(C + "Value")]
            for dim in dims:
                names = lists.get(dim["codelist"], {})
                vals = avail.get(dim["id"], list(names))
                dim["codes_available"] = len(vals)
                dim["codes"] = {v: names.get(v, "") for v in vals[:60]}
            rec["dimensions"] = dims
        except Exception as e:
            rec["error"] = str(e)
            print("FAIL", fid, e, flush=True)
        out.write(json.dumps(rec, ensure_ascii=False) + "\n")
        out.flush()
        done[fid] = rec
with open(tsv, "w", encoding="utf-8") as f:
    f.write("id\tname\tseries\tobservations\tfirst\tlast\tupdated\tdimensions\n")
    for fid, name in flows:
        r = done.get(fid, {})
        dims = " ".join(f"{d['id']}({d['codes_available']})" for d in r.get("dimensions", []))
        vals = (fid, name, r.get("SERIES_COUNT", ""), r.get("OBS_COUNT", ""), r.get("OBS_PERIOD_OVERALL_OLDEST", ""),
                r.get("OBS_PERIOD_OVERALL_LATEST", ""), (r.get("UPDATE_DATA") or "")[:10], dims if "$" not in fid else "view")
        f.write("\t".join(str(x) for x in vals) + "\n")
