#!/usr/bin/env python3
"""World Bank API (api.worldbank.org/v2): databases, their concepts, series, countries and years; the indicator list.

Structure only, no observation. Sequential, paced. Resumable on the per-database file.

    python3 -I worldbank.py docs/sources/catalog

Writes worldbank.tsv (all indicators), worldbank_sources.jsonl (one line per database), worldbank_sources.tsv (the same,
flat), worldbank_series.tsv (database, series id, label: the same indicators seen from each database, which can overlap).
"""
import json
import os
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

BASE = "https://api.worldbank.org/v2"
HDR = {"User-Agent": "Deepwater/1.0"}
PAUSE = 1.0
AREAS = {"country", "admin region", "states", "provinces", "receiving countries"}


def fetch(path):
    url = f"{BASE}/{path}"
    last = None
    for attempt in range(3):
        time.sleep(PAUSE * (attempt + 1))
        try:
            with urllib.request.urlopen(urllib.request.Request(url, headers=HDR), timeout=180) as r:
                return json.load(r)
        except urllib.error.HTTPError as e:
            last = f"HTTP {e.code}"
            if e.code in (403, 429):
                raise SystemExit(f"refused: {last} on {url}")
        except Exception as e:  # noqa: BLE001
            last = repr(e)[:120]
    return {"_error": last}


def clean(s):
    return re.sub(r"\s+", " ", s or "").strip()


def indicators(out):
    path = os.path.join(out, "worldbank.tsv")
    if os.path.exists(path):
        return
    rows, page, pages = [], 1, 1
    while page <= pages:
        d = fetch(f"indicator?format=json&per_page=2000&page={page}")
        pages = int(d[0]["pages"])
        for x in d[1]:
            topics = "; ".join(clean(t.get("value")) for t in x.get("topics", []) if t.get("value"))
            rows.append((x["id"], x["source"]["id"], clean(x["source"]["value"]), topics, clean(x.get("unit")),
                         clean(x["name"]), clean(x.get("sourceOrganization"))[:120]))
        page += 1
    with open(path, "w", encoding="utf-8") as f:
        f.write("indicator\tsource_id\tsource\ttopics\tunit\tname\torganization\n")
        for r in rows:
            f.write("\t".join(r) + "\n")
    print(len(rows), "indicators", flush=True)


def members(source, concept, page_size=5000):
    vals, page, pages, total = [], 1, 1, None
    path = urllib.parse.quote(concept.lower().replace(" ", ""))
    while page <= pages:
        d = fetch(f"sources/{source}/{path}?format=json&per_page={page_size}&page={page}")
        if not isinstance(d, dict) or "source" not in d:
            return vals, total, (d.get("_error") if isinstance(d, dict) else str(d)[:160]) or str(d)[:160]
        pages = int(d.get("pages", 1))
        total = int(d.get("total", 0))
        for s in d["source"] if isinstance(d["source"], list) else [d["source"]]:
            for c in s.get("concept", []):
                vals += c.get("variable", [])
        page += 1
    return vals, total, None


def main():
    out = sys.argv[1]
    indicators(out)
    jsonl, stsv = os.path.join(out, "worldbank_sources.jsonl"), os.path.join(out, "worldbank_series.tsv")
    sources = fetch("sources?format=json&per_page=300")[1]
    done = set()
    if os.path.exists(jsonl):
        with open(jsonl, encoding="utf-8") as f:
            done = {json.loads(line)["id"] for line in f if line.strip()}
    with open(jsonl, "a", encoding="utf-8") as j, open(stsv, "a", encoding="utf-8") as st:
        if not done:
            st.write("source_id\tsource\tseries\tlabel\n")
        for s in sources:
            if s["id"] in done:
                continue
            rec = {"id": s["id"], "code": s.get("code"), "name": s["name"], "lastupdated": s.get("lastupdated"),
                   "dataavailability": s.get("dataavailability"), "metadataavailability": s.get("metadataavailability"),
                   "concepts": []}
            d = fetch(f"sources/{s['id']}/concepts?format=json")
            try:
                concepts = [c["id"] for c in d["source"][0]["concept"]]
            except Exception:  # noqa: BLE001
                concepts = []
                rec["error"] = str(d)[:160]
            for c in concepts:
                vals, total, err = members(s["id"], c)
                item = {"concept": c, "total": total}
                if err:
                    item["error"] = err
                low = c.lower()
                if low in ("time", "year"):
                    ids = [v["id"] for v in vals]
                    item["first"], item["last"] = (ids[0], ids[-1]) if ids else (None, None)
                    freq = set()
                    for i in ids:
                        u = i.upper()
                        freq.add("M" if "M" in u[-3:] and not u.startswith("YR") else
                                 "Q" if "Q" in u[-2:] else "A" if u.startswith("YR") else "other")
                    item["frequencies"] = sorted(freq)
                elif low == "series":
                    st.writelines(f"{s['id']}\t{clean(s['name'])}\t{clean(v['id'])}\t{clean(v.get('value'))}\n" for v in vals)
                    item["sample"] = {v["id"]: v.get("value", "") for v in vals[:8]}
                else:
                    item["members"] = {v["id"]: v.get("value", "") for v in vals[:60]}
                rec["concepts"].append(item)
            j.write(json.dumps(rec, ensure_ascii=False) + "\n")
            j.flush()
            st.flush()
            print(s["id"], s["name"], [(c["concept"], c.get("total")) for c in rec["concepts"]], flush=True)
    with open(jsonl, encoding="utf-8") as f, open(os.path.join(out, "worldbank_sources.tsv"), "w", encoding="utf-8") as t:
        t.write("id\tcode\tname\tlastupdated\tareas\tseries\tperiods\tfirst\tlast\tother_concepts\n")
        for x in map(json.loads, f):
            area = next((c for c in x["concepts"] if c["concept"].strip().lower() in AREAS), {})
            ser = next((c for c in x["concepts"] if c["concept"].lower() == "series"), {})
            tim = next((c for c in x["concepts"] if c["concept"].lower() in ("time", "year")), {})
            other = " ".join(f"{c['concept'].strip()}:{c['total']}" for c in x["concepts"] if c not in (area, ser, tim))
            t.write("\t".join(str(v if v is not None else "") for v in (
                x["id"], x.get("code"), clean(x["name"]), x.get("lastupdated"),
                f"{area.get('concept', '').strip()}:{area.get('total')}", ser.get("total"), tim.get("total"),
                tim.get("first"), tim.get("last"), other)) + "\n")
    print("done", flush=True)


if __name__ == "__main__":
    main()
