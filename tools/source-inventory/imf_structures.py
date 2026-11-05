#!/usr/bin/env python3
"""IMF SDMX (api.imf.org): the list of dataflows, then the structure and availability of each.

Structure only, no observation. Two calls per dataflow, paced. Resumable: flows already in the jsonl are skipped.

    python3 -I imf_structures.py docs/sources/catalog

Writes in that directory:
    imf.tsv               one line per dataflow: agency, id, version, series, first, last, dimensions, name
    imf_structures.jsonl  one line per dataflow: description, dimensions, codelists, available codes (labels up to 60)
    imf_codes.tsv         flow, dimension, code, label for every available code of every dimension but the area ones
"""
import json
import os
import sys
import time
import urllib.error
import urllib.request

BASE = "https://api.imf.org/external/sdmx"
HDR = {"User-Agent": "Deepwater/1.0", "Accept": "application/json"}
AREA_DIMS = {"COUNTRY", "REF_AREA", "JURISDICTION", "AREA", "COUNTERPART_COUNTRY", "COUNTERPART_AREA"}
PAUSE = 1.5


def fetch(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers=HDR), timeout=120) as r:
        body = r.read()
    return json.loads(body) if body else None


def urn_tail(urn):
    """urn...Concept=IMF:CS_MASTER_DATA(1.0+.0).COUNTRY -> (CS_MASTER_DATA, 1.0+.0, COUNTRY)"""
    s = urn.split("=")[-1].split(":")[-1]
    scheme, rest = s.split("(", 1)
    version, concept = rest.split(").", 1)
    return scheme, version, concept


def codelist_of(component, schemes):
    """The codelist id of a dimension: its local representation, else the core representation of its concept."""
    ref = (component.get("localRepresentation") or {}).get("enumeration")
    if not ref:
        scheme, version, concept = urn_tail(component["conceptIdentity"])
        # "4.0+.0" is a wildcard for 4.0 and later: the answer may hold 4.1.0. Same major first, any version after.
        major = version.split(".")[0] + "."
        for s in sorted(schemes, key=lambda s: not s.get("version", "").startswith(major)):
            if s["id"] == scheme and not ref:
                for c in s.get("concepts", []):
                    if c["id"] == concept:
                        ref = (c.get("coreRepresentation") or {}).get("enumeration")
    if not ref:
        return ""
    return ref.split(":")[-1].split("(")[0]


def annotation(item, key):
    for a in item.get("annotations", []):
        if a.get("id") == key:
            return a.get("title") or a.get("value")
    return None


def summarise(stub):
    ag, fid = stub["agencyID"], stub["id"]
    st = fetch(f"{BASE}/3.0/structure/dataflow/{ag}/{fid}/+?references=descendants&detail=referencepartial")["data"]
    time.sleep(PAUSE)
    flow = st["dataflows"][0]
    comp = st["dataStructures"][0]["dataStructureComponents"]
    lists = {c["id"]: c for c in st.get("codelists", [])}
    schemes = st.get("conceptSchemes", [])
    rec = {"id": fid, "agency": ag, "version": flow.get("version"), "name": flow.get("name", ""),
           "description": (flow.get("description") or "")[:800], "updated": annotation(flow, "lastUpdatedAt"),
           "dsd": st["dataStructures"][0]["id"]}
    avail, series = {}, None
    try:
        cons = fetch(f"{BASE}/2.1/availableconstraint/{ag},{fid}/all/all/all")["data"]["contentConstraints"][0]
        rec["series"] = annotation(cons, "series_count")
        rec["from"] = annotation(cons, "time_period_start")
        rec["to"] = annotation(cons, "time_period_end")
        for region in cons.get("cubeRegions", []):
            for kv in region.get("keyValues", []):
                avail[kv["id"]] = kv.get("values", [])
    except Exception as e:  # noqa: BLE001 - recorded in the line, the flow keeps its structure
        rec["availability_error"] = repr(e)[:160]
    time.sleep(PAUSE)
    dims, rows = [], []
    for x in comp["dimensionList"]["dimensions"]:
        cl = codelist_of(x, schemes)
        codes = {k["id"]: k.get("name", "") for k in lists.get(cl, {}).get("codes", [])}
        vals = avail.get(x["id"])
        sample = vals if vals is not None else list(codes)
        dims.append({"id": x["id"], "codelist": cl, "codes_available": len(sample), "codes_in_list": len(codes),
                     "codes": {v: codes.get(v, "") for v in sample[:60]}})
        if x["id"] not in AREA_DIMS:
            rows += [(fid, x["id"], v, codes.get(v, "").replace("\t", " ").replace("\n", " ")) for v in sample]
    rec["dimensions"] = dims
    rec["attributes"] = [a["id"] for a in comp.get("attributeList", {}).get("attributes", [])]
    return rec, rows


def main():
    out = sys.argv[1]
    jsonl, codes, tsv = (os.path.join(out, n) for n in ("imf_structures.jsonl", "imf_codes.tsv", "imf.tsv"))
    flows = fetch(f"{BASE}/3.0/structure/dataflow/*/*/+?detail=allstubs")["data"]["dataflows"]
    print(len(flows), "dataflows", flush=True)
    done = set()
    if os.path.exists(jsonl):
        with open(jsonl, encoding="utf-8") as f:
            done = {json.loads(line)["id"] for line in f if line.strip() and "error" not in json.loads(line)}
    refused = 0
    with open(jsonl, "a", encoding="utf-8") as j, open(codes, "a", encoding="utf-8") as c:
        if not done:
            c.write("flow\tdimension\tcode\tlabel\n")
        for stub in sorted(flows, key=lambda x: (x["agencyID"], x["id"])):
            if stub["id"] in done:
                continue
            try:
                rec, rows = summarise(stub)
                c.writelines("\t".join(r) + "\n" for r in rows)
                refused = 0
            except urllib.error.HTTPError as e:
                rec = {"id": stub["id"], "agency": stub["agencyID"], "name": stub.get("name", ""),
                       "error": f"HTTP {e.code}"}
                if e.code in (403, 429):
                    refused += 1
                    if refused >= 3:
                        j.write(json.dumps(rec, ensure_ascii=False) + "\n")
                        print("refused three times, stopping", flush=True)
                        return
                    time.sleep(90)
            except Exception as e:  # noqa: BLE001
                rec = {"id": stub["id"], "agency": stub["agencyID"], "name": stub.get("name", ""),
                       "error": repr(e)[:160]}
            j.write(json.dumps(rec, ensure_ascii=False) + "\n")
            j.flush()
            c.flush()
            print(stub["id"], rec.get("series"), rec.get("error", ""), flush=True)
    # The flat list, rebuilt from the jsonl.
    latest = {}
    with open(jsonl, encoding="utf-8") as f:
        for line in f:
            if line.strip():
                r = json.loads(line)
                latest[r["id"]] = r
    with open(tsv, "w", encoding="utf-8") as f:
        f.write("agency\tid\tversion\tseries\tfirst\tlast\tdimensions\tname\n")
        for r in sorted(latest.values(), key=lambda x: (x["agency"], x["id"])):
            dims = " ".join(f"{d['id']}:{d['codes_available']}" for d in r.get("dimensions", []))
            f.write("\t".join(str(v or "") for v in (r["agency"], r["id"], r.get("version"), r.get("series"),
                                                     r.get("from"), r.get("to"), dims,
                                                     r.get("name", "").replace("\t", " "))) + "\n")
    print("done", flush=True)


if __name__ == "__main__":
    main()
