#!/usr/bin/env python3
"""Federal Reserve Board Data Download Program: for every release, its Choose page and its full SDMX ZIP, kept as
structure only: dimensions, code lists with counts, and every series' name, description, frequency, unit, first and
last date. Observations are read past and dropped.

    fedboard_ddp.py <download dir> <catalog dir> <call log> [REL ...]
"""
import collections, json, os, re, sys, time, urllib.request, urllib.error, zipfile
import xml.etree.ElementTree as ET

UA = "Deepwater/1.0"
BASE = "https://www.federalreserve.gov/datadownload/"
RELS = ["CHGDEL", "CP", "DSR", "E2", "FOR", "G17", "G19", "G20", "H10", "H15", "H3", "H41", "H6", "H8", "PRATES", "SCOOS", "SLOOS", "Z1"]


def get(url, log, path=None, timeout=300):
    time.sleep(2)
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            body = r.read()
            status = r.status
    except urllib.error.HTTPError as e:
        log.write(f"GET {url} -> {e.code}\n"); log.flush()
        return e.code, b""
    log.write(f"GET {url} -> {status} {len(body)}\n"); log.flush()
    if path:
        open(path, "wb").write(body)
    return status, body


def local(tag):
    return tag.split("}")[-1]


def main():
    dl, out, log_path = sys.argv[1:4]
    rels = sys.argv[4:] or RELS
    log = open(log_path, "a")
    summ = open(os.path.join(out, "fedboard_ddp.jsonl"), "a")
    ser = open(os.path.join(out, "fedboard_ddp_series.tsv"), "a")
    for rel in rels:
        st, body = get(BASE + "Choose.aspx?rel=" + rel, log)
        page = body.decode("utf-8", "replace")
        text = re.sub(r"\s+", " ", re.sub(r"<[^>]+>", " ", page[page.find('id="content"'):]))
        packages = re.findall(r"([A-Z][^\[\]]{2,90}?) \[(csv|SDMX/ZIP)[^\]]*\]", text)
        rec = {"rel": rel, "packages": [p[0].strip() for p in packages if p[1] == "csv"]}
        m = re.search(r"last released ([A-Za-z]+, [A-Za-z]+ \d+, \d+)", text)
        rec["last_released"] = m.group(1) if m else None
        path = os.path.join(dl, rel + ".zip")
        st, _ = get(BASE + f"Output.aspx?rel={rel}&filetype=zip", log, path)
        rec["zip_status"] = st
        if st != 200 or not zipfile.is_zipfile(path):
            summ.write(json.dumps(rec) + "\n"); summ.flush()
            continue
        rec["zip_bytes"] = os.path.getsize(path)
        with zipfile.ZipFile(path) as z:
            rec["members"] = z.namelist()
            struct = [n for n in z.namelist() if "struct" in n.lower()]
            data = [n for n in z.namelist() if "data" in n.lower() and n not in struct]
            if struct:
                root = ET.fromstring(z.read(struct[0]))
                lists = {}
                for cl in root.iter():
                    if local(cl.tag) == "CodeList":
                        codes = [c for c in cl if local(c.tag) == "Code"]
                        name = next((c.text for c in cl if local(c.tag) == "Name"), "")
                        lists[cl.get("id")] = {"name": name, "n": len(codes), "codes": {c.get("value"): next((d.text for d in c if local(d.tag) == "Description"), "") for c in codes[:60]}}
                dims = []
                for kf in root.iter():
                    if local(kf.tag) in ("Dimension", "TimeDimension", "Attribute", "PrimaryMeasure"):
                        dims.append({"kind": local(kf.tag), "concept": kf.get("conceptRef"), "codelist": kf.get("codelist")})
                rec["dims"] = dims
                rec["codelists"] = lists
            nseries = 0
            freq = collections.Counter(); unit = collections.Counter()
            first_all, last_all = None, None
            dimvals = collections.defaultdict(collections.Counter)
            with z.open(data[0]) as f:
                cur = None
                for ev, el in ET.iterparse(f, events=("start", "end")):
                    t = local(el.tag)
                    if ev == "start" and t == "Series":
                        cur = {"attrs": dict(el.attrib), "first": None, "last": None, "desc": ""}
                    elif ev == "end" and t == "Obs" and cur is not None:
                        tp = el.get("TIME_PERIOD")
                        if tp:
                            cur["first"] = min(cur["first"], tp) if cur["first"] else tp
                            cur["last"] = max(cur["last"], tp) if cur["last"] else tp
                        el.clear()
                    elif ev == "end" and t == "TextType" and cur is not None and el.text and not cur["desc"]:
                        cur["desc"] = re.sub(r"\s+", " ", el.text)[:220]
                    elif ev == "end" and t == "Series" and cur is not None:
                        nseries += 1
                        a = cur["attrs"]
                        for k, v in a.items():
                            if k not in ("SERIES_NAME", "UNIT_MULT"):
                                dimvals[k][v] += 1
                        ser.write("\t".join([rel, a.get("SERIES_NAME", ""), a.get("FREQ", ""), a.get("UNIT", ""), str(cur["first"]), str(cur["last"]), cur["desc"]]) + "\n")
                        first_all = min(first_all, cur["first"]) if first_all and cur["first"] else (first_all or cur["first"])
                        last_all = max(last_all, cur["last"]) if last_all and cur["last"] else (last_all or cur["last"])
                        cur = None
                        el.clear()
            rec.update({"series": nseries, "first": first_all, "last": last_all,
                        "attrs": {k: {"distinct": len(v), "top": v.most_common(25)} for k, v in dimvals.items()}})
        summ.write(json.dumps(rec) + "\n"); summ.flush(); ser.flush()
        os.remove(path)
    print("done")


main()
