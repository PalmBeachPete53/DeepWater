#!/usr/bin/env python3
"""BIS SDMX (stats.bis.org): the dataflows, then the structure, availability and time span of each.

Time spans come from firstNObservations=1 and lastNObservations=1 in CSV; only the TIME_PERIOD column is kept, the values
are thrown away as they arrive. A flow with many series is asked on one reporting area only, marked "sampled".

    python3 -I bis.py docs/sources/catalog

Writes bis.tsv, bis_structures.jsonl, bis_codes.tsv (flow, dimension, code, label; not the area dimensions).
"""
import csv
import io
import json
import os
import sys
import time
import urllib.error
import urllib.request

BASE = "https://stats.bis.org/api/v2"
HDR = {"User-Agent": "Deepwater/1.0"}
PAUSE = 1.5
BIG = 120000
AREA = ("REF_AREA", "L_REP_CTY", "ISSUER_RES", "REP_CTY", "DER_REP_CTY", "BORROWERS_CTY")


def get(url, timeout=180):
    time.sleep(PAUSE)
    with urllib.request.urlopen(urllib.request.Request(url, headers=HDR), timeout=timeout) as r:
        return r.read()


def span(flow, version, key, extra=""):
    """First and last period per frequency, from the two CSV answers; values are not kept."""
    res, url_tail = {}, f"{flow}/{version}/{key}?format=csv&detail=dataonly"
    for param, pick in (("firstNObservations", min), ("lastNObservations", max)):
        body = get(f"{BASE}/data/dataflow/BIS/{url_tail}&{param}=1", timeout=280)
        for row in csv.DictReader(io.StringIO(body.decode("utf-8", "replace"))):
            f, t = row.get("FREQ", "?"), row.get("TIME_PERIOD")
            if not t:
                continue
            cur = res.setdefault(f, {})
            name = "first" if pick is min else "last"
            cur[name] = pick(cur[name], t) if name in cur else t
    return res


def summarise(flow, version):
    st = json.loads(get(f"{BASE}/structure/dataflow/BIS/{flow}/+?references=descendants&detail=referencepartial&format=sdmx-json"))["data"]
    f = st["dataflows"][0]
    comp = st["dataStructures"][0]["dataStructureComponents"]
    lists = {c["id"]: c for c in st.get("codelists", [])}
    rec = {"id": flow, "version": version, "name": f.get("name"), "description": (f.get("description") or "")[:800],
           "dsd": st["dataStructures"][0]["id"]}
    avail = {}
    try:
        c = json.loads(get(f"{BASE}/availability/dataflow/BIS/{flow}/+/*?mode=exact&format=sdmx-json"))["data"]["contentConstraints"][0]
        for a in c.get("annotations", []):
            if a.get("id") == "series_count":
                rec["series"] = int(a.get("title"))
        for region in c.get("cubeRegions", []):
            for kv in region.get("keyValues", []):
                avail[kv["id"]] = kv.get("values", [])
    except Exception as e:  # noqa: BLE001
        rec["availability_error"] = repr(e)[:160]
    dims, rows = [], []
    order = []
    for x in comp["dimensionList"]["dimensions"]:
        ref = (x.get("localRepresentation") or {}).get("enumeration", "")
        cl = ref.split(":")[-1].split("(")[0] if ref else ""
        codes = {k["id"]: k.get("name", "") for k in lists.get(cl, {}).get("codes", [])}
        vals = avail.get(x["id"])
        sample = vals if vals is not None else list(codes)
        order.append((x["id"], sample))
        dims.append({"id": x["id"], "codelist": cl, "codes_available": len(sample), "codes_in_list": len(codes),
                     "codes": {v: codes.get(v, "") for v in sample[:60]}})
        rows += [(flow, x["id"], v, codes.get(v, "").replace("\t", " ").replace("\n", " ")) for v in sample]
    rec["dimensions"] = dims
    rec["attributes"] = [a["id"] for a in comp.get("attributeList", {}).get("attributes", [])]
    # Time span
    measure_span(rec, version)
    return rec, rows


def measure_span(rec, version):
    """A key of wildcards only (*.*.*) answers 404: the whole flow is the key "*", a one-area sample fixes one dimension."""
    rec.pop("span_error", None)
    key = "*"
    if (rec.get("series") or 0) > BIG:
        dims = [(d["id"], d["codes"]) for d in rec["dimensions"]]
        for i, (dim, codes) in enumerate(dims):
            if dim in AREA and codes:
                fixed = "US" if "US" in codes else next(iter(codes))
                parts = ["*"] * len(dims)
                parts[i] = fixed
                key = ".".join(parts)
                rec["span_sampled"] = True
                rec["span_key_fixed"] = {dim: fixed}
                break
    try:
        rec["span"] = span(rec["id"], version, key)
    except Exception as e:  # noqa: BLE001
        rec["span_error"] = repr(e)[:160]


def main():
    out = sys.argv[1]
    jsonl, codes, tsv = (os.path.join(out, n) for n in ("bis_structures.jsonl", "bis_codes.tsv", "bis.tsv"))
    flows = json.loads(get(f"{BASE}/structure/dataflow/BIS/*/+?detail=allstubs&format=sdmx-json"))["data"]["dataflows"]
    print(len(flows), "dataflows", flush=True)
    done = {}
    if os.path.exists(jsonl):
        with open(jsonl, encoding="utf-8") as f:
            for line in f:
                if line.strip():
                    done[json.loads(line)["id"]] = json.loads(line)
    with open(jsonl, "a", encoding="utf-8") as j, open(codes, "a", encoding="utf-8") as c:
        if not done:
            c.write("flow\tdimension\tcode\tlabel\n")
        for x in sorted(flows, key=lambda x: x["id"]):
            if x["id"] in done:
                continue
            try:
                rec, rows = summarise(x["id"], x["version"])
                c.writelines("\t".join(r) + "\n" for r in rows if r[1] not in ("REF_AREA", "L_REP_CTY", "L_CP_COUNTRY"))
            except urllib.error.HTTPError as e:
                rec = {"id": x["id"], "version": x["version"], "name": x.get("name"), "error": f"HTTP {e.code}"}
                if e.code in (403, 429):
                    j.write(json.dumps(rec) + "\n")
                    print("refused, stopping", flush=True)
                    return
            done[x["id"]] = rec
            j.write(json.dumps(rec, ensure_ascii=False) + "\n")
            j.flush()
            c.flush()
            print(x["id"], rec.get("series"), rec.get("span"), rec.get("span_error", ""), rec.get("error", ""), flush=True)
    # A second pass for the spans that failed; the jsonl is rewritten with the last state of each flow.
    for r in done.values():
        if "span" not in r and "dimensions" in r:
            measure_span(r, r["version"])
            print(r["id"], r.get("span"), r.get("span_error", ""), flush=True)
    with open(jsonl, "w", encoding="utf-8") as j:
        j.writelines(json.dumps(r, ensure_ascii=False) + "\n" for r in done.values())
    with open(tsv, "w", encoding="utf-8") as f:
        f.write("id\tversion\tseries\tspan\tdimensions\tname\n")
        for r in sorted(done.values(), key=lambda x: x["id"]):
            s = " ".join(f"{k}:{v.get('first')}..{v.get('last')}" for k, v in (r.get("span") or {}).items())
            dims = " ".join(f"{d['id']}:{d['codes_available']}" for d in r.get("dimensions", []))
            f.write(f"{r['id']}\t{r['version']}\t{r.get('series','')}\t{s}\t{dims}\t{r.get('name','')}\n")
    print("done", flush=True)


if __name__ == "__main__":
    main()
