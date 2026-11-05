#!/usr/bin/env python3
"""Census Data API: writes the whole data.json catalogue to census.tsv, then for every time series data set its
variables and geography levels to census_timeseries.jsonl (names, labels, predicate types; never data rows).

    census_catalog.py <data.json path> <catalog dir> <call log>
"""
import json, os, sys, time, urllib.error, urllib.request

UA = "Deepwater/1.0"


def get(url, log):
    time.sleep(1)
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    try:
        with urllib.request.urlopen(req, timeout=60) as r:
            body = r.read()
            log.write(f"GET {url} -> {r.status} {len(body)}\n"); log.flush()
            return json.loads(body)
    except Exception as e:
        log.write(f"GET {url} -> {getattr(e, 'code', repr(e)[:80])}\n"); log.flush()
        return None


def main():
    path, out, log_path = sys.argv[1:4]
    log = open(log_path, "a")
    ds = json.load(open(path))["dataset"]
    with open(os.path.join(out, "census.tsv"), "w") as f:
        f.write("path\tvintage\ttitle\tkind\ttemporal\tspatial\tmodified\n")
        for d in sorted(ds, key=lambda x: ("/".join(x.get("c_dataset", [])), str(x.get("c_vintage", "")))):
            kind = "timeseries" if d.get("c_isTimeseries") else "microdata" if d.get("c_isMicrodata") else "aggregate" if d.get("c_isAggregate") else "other"
            f.write("\t".join(str(x).replace("\t", " ") for x in ["/".join(d.get("c_dataset", [])), d.get("c_vintage", ""), d.get("title", ""), kind, d.get("temporal", ""), d.get("spatial", ""), d.get("modified", "")]) + "\n")
    with open(os.path.join(out, "census_timeseries.jsonl"), "w") as f:
        for d in ds:
            if not d.get("c_isTimeseries"):
                continue
            rec = {"path": "/".join(d["c_dataset"]), "title": d.get("title"), "description": (d.get("description") or "")[:300], "temporal": d.get("temporal")}
            v = get(d["c_variablesLink"], log)
            if v:
                rec["variables"] = {k: {"label": x.get("label"), "predicate": x.get("predicateType"), "required": x.get("required")} for k, x in v.get("variables", {}).items() if k not in ("for", "in", "ucgid")}
            g = get(d["c_geographyLink"], log)
            if g:
                rec["geography"] = sorted({x.get("name") for x in g.get("fips", [])})
            f.write(json.dumps(rec) + "\n"); f.flush()
    print("done")


main()
