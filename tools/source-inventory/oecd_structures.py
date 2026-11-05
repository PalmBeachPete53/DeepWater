#!/usr/bin/env python3
"""OECD SDMX: the list of dataflows, then the structure of each (dimensions, countries, frequencies, years, size).

Structure only, no observation. The OECD allows about 20 requests a minute: paced at 13. Resumable.

    oecd_structures.py docs/sources/catalog/oecd.tsv docs/sources/catalog/oecd_structures.jsonl [--list-only]
"""
import json
import sys
import time
import urllib.request

BASE = "https://sdmx.oecd.org/public/rest/dataflow"
HDR = {"User-Agent": "Deepwater/1.0", "Accept": "application/vnd.sdmx.structure+json;version=1.0"}


def fetch(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers=HDR), timeout=90) as r:
        return json.load(r)["data"]


def summarise(d):
    flow = d["dataflows"][0]
    lists = {c["id"]: c for c in d.get("codelists", [])}
    dims = []
    for x in d["dataStructures"][0]["dataStructureComponents"]["dimensionList"]["dimensions"]:
        ref = x.get("localRepresentation", {}).get("enumeration", "")
        cl = ref.split(":")[-1].split("(")[0] if ref else ""
        dims.append({"id": x["id"], "codelist": cl})
    avail = {}
    cons = d.get("contentConstraints", [])
    rec = {"id": flow["id"], "agency": flow["agencyID"], "version": flow["version"], "name": flow.get("name", ""),
           "description": (flow.get("description") or "")[:600]}
    if cons:
        c = cons[0]
        for a in c.get("annotations", []):
            if a.get("id") == "obs_count":
                rec["observations"] = a.get("title")
        for region in c.get("cubeRegions", []):
            for kv in region.get("keyValues", []):
                if kv["id"] == "TIME_PERIOD" or "timeRange" in kv:
                    tr = kv.get("timeRange") or {}
                    rec["from"] = (tr.get("startPeriod") or {}).get("period")
                    rec["to"] = (tr.get("endPeriod") or {}).get("period")
                else:
                    avail[kv["id"]] = kv.get("values", [])
    for dim in dims:
        vals = avail.get(dim["id"])
        codes = {k["id"]: k.get("name", "") for k in lists.get(dim["codelist"], {}).get("codes", [])}
        dim["codes_available"] = len(vals) if vals is not None else len(codes)
        sample = vals if vals is not None else list(codes)
        # Every code of a short list with its label, a sample of a long one: what the axis holds, not the data.
        dim["codes"] = {v: codes.get(v, "") for v in sample[:60]}
    rec["dimensions"] = dims
    return rec


def main():
    tsv, jsonl = sys.argv[1], sys.argv[2]
    flows = fetch(f"{BASE}/all/all/latest?detail=allstubs")["dataflows"]
    with open(tsv, "w", encoding="utf-8") as f:
        f.write("agency\tid\tversion\tname\n")
        for x in sorted(flows, key=lambda x: (x["agencyID"], x["id"])):
            name = (x.get("name") or "").replace("\t", " ")
            f.write(f"{x['agencyID']}\t{x['id']}\t{x['version']}\t{name}\n")
    print(len(flows), "dataflows", flush=True)
    if "--list-only" in sys.argv:
        return
    done = set()
    try:
        with open(jsonl, encoding="utf-8") as f:
            done = {json.loads(l)["id"] for l in f if l.strip()}
    except FileNotFoundError:
        pass
    with open(jsonl, "a", encoding="utf-8") as out:
        for x in flows:
            if x["id"] in done:
                continue
            time.sleep(4.6)
            url = f"{BASE}/{x['agencyID']}/{x['id']}/latest?references=all&detail=referencepartial"
            try:
                rec = summarise(fetch(url))
            except Exception as e:
                rec = {"id": x["id"], "agency": x["agencyID"], "name": x.get("name", ""), "error": repr(e)[:160]}
                if "429" in repr(e):
                    time.sleep(120)
            out.write(json.dumps(rec, ensure_ascii=False) + "\n")
            out.flush()
    print("done", flush=True)


if __name__ == "__main__":
    main()
