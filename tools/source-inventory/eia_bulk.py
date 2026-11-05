#!/usr/bin/env python3
"""EIA's route tree needs a key; its bulk files do not. Walks api.eia.gov/bulk/manifest.txt, downloads each current
bulk ZIP, and keeps structure only (category tree, series counts by frequency and unit, first and last period, id
families); the observations are never kept.

    eia_bulk.py <download dir> <catalog dir> [dataset ...]      # default: the current data sets, not AEO/IEO vintages
"""
import collections, json, os, re, sys, time, urllib.request, zipfile

UA = "Deepwater/1.0"
MAX = 700 * 1024 * 1024


def fetch(url, path):
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=600) as r, open(path, "wb") as f:
        size = int(r.headers.get("Content-Length", 0))
        if size > MAX:
            return r.status, size, False
        while True:
            chunk = r.read(1 << 20)
            if not chunk:
                break
            f.write(chunk)
    return 200, os.path.getsize(path), True


def family(ds, sid):
    """A readable id family, so the catalogue lists kinds of series and not every series."""
    head = ".".join(sid.split(".")[:2])
    if ds == "INTL":
        return "-".join(head.split("-")[:2])
    tail = head.split(".", 1)[1] if "." in head else head
    if ds in ("PET", "NG"):
        p = tail.split("_")
        return p[0] + "_" + p[1] if len(p[0]) == 1 and len(p) > 1 else (p[0] if len(p) > 1 else re.sub(r"[0-9].*$", "", tail)[:3])
    if ds in ("SEDS", "TOTAL"):
        return tail[:2]
    if ds == "STEO":
        return tail.split("_")[0][:4]
    if ds.startswith(("AEO", "IEO")):
        return head.split(".")[0]
    return tail


def main():
    dl, out = sys.argv[1:3]
    wanted = sys.argv[3:] or ["PET", "PET_IMPORTS", "NG", "TOTAL", "STEO", "INTL", "SEDS", "COAL", "EMISS", "NUC_STATUS", "ELEC", "IEO", "AEO.2026"]
    req = urllib.request.Request("https://api.eia.gov/bulk/manifest.txt", headers={"User-Agent": UA})
    manifest = json.load(urllib.request.urlopen(req, timeout=60))["dataset"]
    cat = open(os.path.join(out, "eia_categories.tsv"), "a")
    fam = open(os.path.join(out, "eia_series_families.tsv"), "a")
    summary = open(os.path.join(out, "eia_bulk.jsonl"), "a")
    for name in wanted:
        url = manifest[name]["accessURL"]
        path = os.path.join(dl, name + ".zip")
        time.sleep(2)
        try:
            status, size, ok = fetch(url, path)
        except Exception as e:
            summary.write(json.dumps({"dataset": name, "url": url, "error": repr(e)[:200]}) + "\n")
            summary.flush()
            continue
        if not ok:
            summary.write(json.dumps({"dataset": name, "url": url, "skipped_bytes": size}) + "\n")
            summary.flush()
            continue
        series = cats = 0
        freqs, units, first, last = collections.Counter(), collections.Counter(), None, None
        families = collections.defaultdict(lambda: [0, set(), set(), None, None, ""])
        with zipfile.ZipFile(path) as z:
            for member in z.namelist():
                with z.open(member) as f:
                    for line in f:
                        try:
                            o = json.loads(line)
                        except ValueError:
                            continue
                        if "series_id" in o:
                            series += 1
                            sid = o["series_id"]
                            freq = o.get("f") or ""
                            unit = o.get("units") or ""
                            freqs[freq] += 1
                            units[unit] += 1
                            s, e = str(o.get("start", "")), str(o.get("end", ""))
                            first = min(first, s) if first and s else (first or s)
                            last = max(last, e) if last and e else (last or e)
                            key = family(name, sid)
                            fm = families[key]
                            fm[0] += 1
                            fm[1].add(freq)
                            fm[2].add(unit)
                            fm[3] = min(fm[3], s) if fm[3] and s else (fm[3] or s)
                            fm[4] = max(fm[4], e) if fm[4] and e else (fm[4] or e)
                            if not fm[5]:
                                fm[5] = (o.get("name") or "")[:110]
                        elif "category_id" in o:
                            cats += 1
                            cat.write("\t".join([name, str(o["category_id"]), str(o.get("parent_category_id", "")),
                                                 re.sub(r"\s+", " ", o.get("name") or "")[:140], str(len(o.get("childseries", [])))]) + "\n")
        for key, fm in sorted(families.items()):
            fam.write("\t".join([name, key, str(fm[0]), ",".join(sorted(fm[1])), "|".join(sorted(fm[2]))[:120], str(fm[3]), str(fm[4]), fm[5]]) + "\n")
        summary.write(json.dumps({"dataset": name, "title": manifest[name]["title"], "url": url, "zip_bytes": size, "series": series, "categories": cats,
                                  "freq": freqs, "units_count": len(units), "top_units": units.most_common(12), "first": first, "last": last,
                                  "families": len(families), "last_updated": manifest[name]["last_updated"]}) + "\n")
        for fl in (cat, fam, summary):
            fl.flush()
        os.remove(path)
    print("done")


main()
