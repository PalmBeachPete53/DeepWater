#!/usr/bin/env python3
"""ECB Data Portal: every dataflow with its dimensions, labels, series count and first and last period.

Structure only: the observations of the first/last calls are read for their dates and thrown away. One call at a time,
two seconds apart. Over 8,000 series the range comes from five series spread through the flow. Resumable. Run with
`python3 -I`.

    ecb.py docs/sources/catalog/ecb.tsv docs/sources/catalog/ecb_structures.jsonl [--repair] [--retry-errors]
"""
import csv
import io
import json
import os
import sys
import time
import urllib.request
import xml.etree.ElementTree as ET

BASE = "https://data-api.ecb.europa.eu/service"
HDR = {"User-Agent": "Deepwater/1.0"}
S = "{http://www.sdmx.org/resources/sdmxml/schemas/v2_1/structure}"
C = "{http://www.sdmx.org/resources/sdmxml/schemas/v2_1/common}"
XL = "{http://www.w3.org/XML/1998/namespace}lang"
PAUSE = 2.0
BIG = 8000
SAMPLE = 5


def fetch(url, timeout=150):
    with urllib.request.urlopen(urllib.request.Request(url, headers=HDR), timeout=timeout) as r:
        return r.read()


def name_of(el):
    return next((n.text or "" for n in el.findall(C + "Name") if n.get(XL) == "en"), "")


def structure(flow):
    root = ET.fromstring(fetch(f"{BASE}/dataflow/ECB/{flow}?references=all"))
    lists = {cl.get("id"): {c.get("id"): name_of(c) for c in cl.findall(S + "Code")} for cl in root.iter(S + "Codelist")}
    concepts = {c.get("id"): name_of(c) for c in root.iter(S + "Concept")}
    dims = []
    for d in root.iter(S + "Dimension"):
        if not d.get("id"):  # the constraint's references to a dimension, not dimensions
            continue
        ref = d.find(f"{S}LocalRepresentation/{S}Enumeration/Ref")
        dims.append({"id": d.get("id"), "label": concepts.get(d.get("id"), ""), "codelist": ref.get("id") if ref is not None else ""})
    attrs = [a.get("id") for a in root.iter(S + "Attribute")]
    return dims, lists, attrs


def keys(flow, dims):
    """Series keys: how many series, and which codes each dimension really uses."""
    text = fetch(f"{BASE}/data/{flow}?detail=serieskeysonly&format=csvdata").decode("utf-8", "replace")
    n, used, sample = 0, {}, []
    for row in csv.DictReader(io.StringIO(text)):
        n += 1
        sample.append(row["KEY"].split(".", 1)[1])
        for d in dims:
            used.setdefault(d["id"], set()).add(row.get(d["id"], ""))
    return n, used, sample[:: max(1, n // SAMPLE)][:SAMPLE]


def periods(flow, which, span, key=None):
    """Earliest or latest period by frequency, from firstNObservations/lastNObservations; the values are not kept."""
    path = f"{flow}/{key}" if key else flow
    text = fetch(f"{BASE}/data/{path}?{which}=1&detail=dataonly&format=csvdata", 60).decode("utf-8", "replace")
    for row in csv.DictReader(io.StringIO(text)):
        freq, t = row.get("FREQ", "?"), row.get("TIME_PERIOD", "")
        if not t:
            continue
        lo, hi = span.get(freq, (t, t))
        span[freq] = (min(lo, t), max(hi, t))


def repair(jsonl):
    """A second pass over what failed: a 504 on the keys is retried frequency by frequency, a timed-out first or last
    call on a spread of series. Ranges and counts got this way are marked."""
    with open(jsonl, encoding="utf-8") as f:
        recs = [json.loads(line) for line in f]
    for rec in recs:
        if "dimensions" in rec:
            rec["dimensions"] = [d for d in rec["dimensions"] if d.get("id")]
    for rec in recs:
        fid = rec["id"]
        if "error" in rec or not (rec.get("series_error") or rec.get("period_errors")):
            continue
        dims = [d for d in rec["dimensions"] if d.get("id")]
        sample = []
        if rec.get("series_error"):
            n, used, parts = 0, {}, []
            first = dims[0]["id"] == "FREQ"
            for freq in ("M", "Q", "A", "D") if first else [None]:
                key = f"/{freq}" + "." * (len(dims) - 1) if first else ""
                time.sleep(PAUSE)
                try:
                    text = fetch(f"{BASE}/data/{fid}{key}?detail=serieskeysonly&format=csvdata").decode("utf-8", "replace")
                except Exception:
                    continue
                for row in csv.DictReader(io.StringIO(text)):
                    n += 1
                    parts.append(row["KEY"].split(".", 1)[1])
                    for d in dims:
                        used.setdefault(d["id"], set()).add(row.get(d["id"], ""))
            if n:
                rec["series"] = n
                rec["series_partial"] = "by frequency, M Q A D"
                rec.pop("series_error")
                for d in dims:
                    if d["id"] in used:
                        d["codes_used"] = len(used[d["id"]])
                        names = d.get("codes", {})
                        d["codes"] = {v: names.get(v, "") for v in sorted(used[d["id"]])[:60]}
                sample = parts[:: max(1, n // SAMPLE)][:SAMPLE]
        else:
            time.sleep(PAUSE)
            try:
                _, _, sample = keys(fid, dims)
            except Exception:
                pass
        if sample:
            span = {k: tuple(v) for k, v in (rec.get("periods_by_freq") or {}).items()} if not rec.get("series_partial") else {}
            rec.pop("period_errors", None)
            rec["periods_sampled_from"] = len(sample)
            for which in ("firstNObservations", "lastNObservations"):
                for key in sample:
                    try:
                        periods(fid, which, span, key)
                    except Exception as e:
                        rec.setdefault("period_errors", []).append(f"{which}: {e}")
                    time.sleep(PAUSE / 2)
            rec["periods_by_freq"] = {k: list(v) for k, v in sorted(span.items())}
        print(fid, rec.get("series"), rec.get("series_error") or "", flush=True)
        with open(jsonl + ".tmp", "w", encoding="utf-8") as out:
            for r in recs:
                out.write(json.dumps(r, ensure_ascii=False) + "\n")
        os.replace(jsonl + ".tmp", jsonl)
    with open(jsonl + ".tmp", "w", encoding="utf-8") as out:
        for r in recs:
            out.write(json.dumps(r, ensure_ascii=False) + "\n")
    os.replace(jsonl + ".tmp", jsonl)


def retry_errors(jsonl, budget=720):
    """Flows whose structure call gave a 504, the ones the app reads first, within a time budget in seconds."""
    with open(jsonl, encoding="utf-8") as f:
        recs = [json.loads(line) for line in f]
    for rec in recs:
        if "dimensions" in rec:
            rec["dimensions"] = [d for d in rec["dimensions"] if d.get("id")]
    end = time.time() + budget
    wanted = [r for r in recs if "error" in r or r.get("series_error")]
    wanted.sort(key=lambda r: (r["id"] not in ("MIR", "SPF", "STS", "TRD", "RPP", "SUP", "SAFE", "CPP"), r["id"]))
    for rec in wanted:
        if time.time() > end:
            break
        fid = rec["id"]
        try:
            if "error" in rec:
                dims, lists, attrs = structure(fid)
                for d in dims:
                    names = lists.get(d["codelist"], {})
                    d["codes_in_list"] = len(names)
                    d["codes_used"] = None
                    d["codes"] = {v: names.get(v, "") for v in list(names)[:60]}
                rec.update(attributes=attrs, dimensions=dims)
                rec.pop("error")
            dims = rec["dimensions"]
            time.sleep(PAUSE)
            n, used, sample = keys(fid, dims)
            rec.update(series=n)
            rec.pop("series_error", None)
            for d in dims:
                names = d.get("codes", {})
                d["codes_used"] = len(used[d["id"]])
                d["codes"] = {v: names.get(v, "") for v in sorted(used[d["id"]])[:60]}
            span = {}
            rec["periods_sampled_from"] = len(sample)
            for which in ("firstNObservations", "lastNObservations"):
                for key in sample:
                    time.sleep(PAUSE / 2)
                    try:
                        periods(fid, which, span, key)
                    except Exception as e:
                        rec.setdefault("period_errors", []).append(f"{which}: {e}")
            rec["periods_by_freq"] = {k: list(v) for k, v in sorted(span.items())}
        except Exception as e:
            if "dimensions" in rec:
                rec["series_error"] = str(e)
            else:
                rec["error"] = str(e)
        print(fid, rec.get("series"), rec.get("error") or rec.get("series_error") or "", flush=True)
        with open(jsonl + ".tmp", "w", encoding="utf-8") as out:
            for r in recs:
                out.write(json.dumps(r, ensure_ascii=False) + "\n")
        os.replace(jsonl + ".tmp", jsonl)
        time.sleep(PAUSE)
    with open(jsonl + ".tmp", "w", encoding="utf-8") as out:
        for r in recs:
            out.write(json.dumps(r, ensure_ascii=False) + "\n")
    os.replace(jsonl + ".tmp", jsonl)


def main():
    tsv, jsonl = sys.argv[1], sys.argv[2]
    if "--repair" in sys.argv:
        repair(jsonl)
    if "--retry-errors" in sys.argv:
        retry_errors(jsonl)
    root = ET.fromstring(fetch(f"{BASE}/dataflow/ECB"))
    flows = [(d.get("id"), name_of(d), d.find(f"{S}Structure/Ref").get("id")) for d in root.iter(S + "Dataflow")]
    print(len(flows), "flows", flush=True)
    done = {}
    if os.path.exists(jsonl):
        with open(jsonl, encoding="utf-8") as f:
            done = {r["id"]: r for r in map(json.loads, f)}
    with open(jsonl, "a", encoding="utf-8") as out:
        for fid, name, dsd in flows:
            if fid in done:
                continue
            rec = {"id": fid, "name": name, "dsd": dsd}
            try:
                dims, lists, attrs = structure(fid)
                rec["attributes"] = attrs
                time.sleep(PAUSE)
                used = None
                try:
                    rec["series"], used, sample = keys(fid, dims)
                except Exception as e:
                    rec["series_error"] = str(e)
                time.sleep(PAUSE)
                if rec.get("series"):
                    span = {}
                    # A big flow's first/last calls take minutes: its range comes from a spread of its series.
                    big = rec["series"] > BIG
                    if big:
                        rec["periods_sampled_from"] = len(sample)
                    for which in ("firstNObservations", "lastNObservations"):
                        for key in sample if big else [None]:
                            try:
                                periods(fid, which, span, key)
                            except Exception as e:
                                rec.setdefault("period_errors", []).append(f"{which}: {e}")
                            time.sleep(PAUSE / 2 if big else PAUSE)
                    rec["periods_by_freq"] = {k: list(v) for k, v in sorted(span.items())}
                for d in dims:
                    names = lists.get(d["codelist"], {})
                    vals = sorted(used[d["id"]]) if used and d["id"] in used else list(names)
                    d["codes_in_list"] = len(names)
                    d["codes_used"] = len(vals) if used else None
                    d["codes"] = {v: names.get(v, "") for v in vals[:60]}
                rec["dimensions"] = dims
            except Exception as e:
                rec["error"] = str(e)
            print(fid, rec.get("series"), rec.get("error") or rec.get("series_error") or "", flush=True)
            out.write(json.dumps(rec, ensure_ascii=False) + "\n")
            out.flush()
            done[fid] = rec
            time.sleep(PAUSE)
    with open(tsv, "w", encoding="utf-8") as f:
        f.write("id\tname\tdsd\tseries\tfreqs\tfirst\tlast\tdimensions\n")
        for fid, name, dsd in flows:
            r = done[fid]
            span = r.get("periods_by_freq") or {}
            firsts = [v[0] for v in span.values()]
            lasts = [v[1] for v in span.values()]
            dims = " ".join(f"{d['id']}({d.get('codes_used') if d.get('codes_used') is not None else d.get('codes_in_list')})"
                            for d in r.get("dimensions", []))
            f.write("\t".join(str(x) for x in (fid, name, dsd, r.get("series", ""), ",".join(span), min(firsts, default=""),
                                               max(lasts, default=""), dims)) + "\n")


if __name__ == "__main__":
    main()
