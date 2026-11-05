#!/usr/bin/env python3
"""Eurostat: the table of contents as a TSV with theme paths, then the structure of the macro and markets families.

Structure only, no observation. One call at a time, a second apart. Resumable. Run with `python3 -I`.

    eurostat.py docs/sources/catalog/eurostat.tsv docs/sources/catalog/eurostat_structures.jsonl [--list-only]
"""
import csv
import io
import json
import os
import sys
import time
import urllib.request
import xml.etree.ElementTree as ET

TOC = "https://ec.europa.eu/eurostat/api/dissemination/catalogue/toc/txt?lang=en"
SDMX = "https://ec.europa.eu/eurostat/api/dissemination/sdmx/2.1"
HDR = {"User-Agent": "Deepwater/1.0"}
S = "{http://www.sdmx.org/resources/sdmxml/schemas/v2_1/structure}"
C = "{http://www.sdmx.org/resources/sdmxml/schemas/v2_1/common}"

# What the app reads, then the families a macro and markets terminal asks for (prefixes of data set codes).
READ = """apro_cpsh1 earn_mw_cur ei_bsin_q_r2 ei_bssi_m_r2 gov_10q_ggdebt gov_10q_ggnfa ilc_lvho02 jvs_q_nace2 lc_lci_r2_q
migr_asyappctzm nama_10_a64 namq_10_a10 namq_10_gdp nasq_10_ki nrg_cb_pem prc_hicp_minr prc_hpi_q road_eqr_carpda
sts_cobp_q sts_copr_m sts_inpi_m sts_inppd_m sts_inpr_m sts_rb_q sts_trtu_m tour_occ_nim une_ltu_q une_rt_m""".split()
PREFIXES = ("ei_", "irt_", "ert_", "bop_", "ext_st", "gov_10", "sts_", "prc_hicp", "prc_hpi", "prc_ppp", "une_", "lfsi_",
            "jvs_", "lc_lci", "earn_mw", "nama_10_", "namq_", "nasq_", "nasa_10_", "nrg_cb", "nrg_pc", "apro_cpsh",
            "road_eqr_car", "proj_25n", "demo_pjan", "demo_gind", "tour_occ_ni", "migr_asyapp", "bd_", "lfsq_ur", "ilc_di")
PAUSE = 1.0


def fetch(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers=HDR), timeout=120) as r:
        return r.read()


def toc():
    """Every data set and table of the tree, one row per place in the tree: a code can sit under several themes."""
    text = fetch(TOC).decode("utf-8")
    rows = list(csv.reader(io.StringIO(text), delimiter="\t"))[1:]
    stack, out = [], []
    for r in rows:
        depth = (len(r[0]) - len(r[0].lstrip())) // 4
        stack = stack[:depth]
        if r[2] == "folder":
            stack.append(r[0].strip())
            continue
        out.append({"type": r[2], "code": r[1], "title": r[0].strip(), "theme": " > ".join(stack), "updated": r[3].strip(),
                    "structure_changed": r[4].strip(), "from": r[5].strip(), "to": r[6].strip(), "values": r[7].strip()})
    return out


def structure(code):
    root = ET.fromstring(fetch(f"{SDMX}/dataflow/ESTAT/{code}?references=descendants&detail=referencepartial"))
    lists = {}
    for cl in root.iter(S + "Codelist"):
        names = {}
        for c in cl.findall(S + "Code"):
            name = next((n.text for n in c.findall(C + "Name") if n.get("{http://www.w3.org/XML/1998/namespace}lang") == "en"), "")
            names[c.get("id")] = name
        lists[cl.get("id")] = names
    dims = []
    for d in root.iter(S + "Dimension"):
        ref = d.find(f"{S}LocalRepresentation/{S}Enumeration/Ref")
        dims.append({"id": d.get("id"), "codelist": ref.get("id") if ref is not None else ""})
    time.sleep(PAUSE)
    avail = {}
    try:
        root = ET.fromstring(fetch(f"{SDMX}/contentconstraint/ESTAT/{code}"))
        for region in root.iter(S + "CubeRegion"):
            for kv in region.findall(C + "KeyValue"):
                avail[kv.get("id")] = [v.text for v in kv.findall(C + "Value")]
    except Exception as e:  # a data set without a constraint keeps its code lists
        avail = {"_error": str(e)}
    for dim in dims:
        names = lists.get(dim["codelist"], {})
        vals = avail.get(dim["id"], list(names))
        dim["codes_available"] = len(vals)
        # Every code of a short list with its label, a sample of a long one: what the axis holds, not the data.
        dim["codes"] = {v: names.get(v, "") for v in vals[:60]}
    return dims


def main():
    tsv, jsonl = sys.argv[1], sys.argv[2]
    rows = toc()
    cols = ["type", "code", "title", "theme", "updated", "structure_changed", "from", "to", "values"]
    with open(tsv, "w", encoding="utf-8") as f:
        f.write("\t".join(cols) + "\n")
        for r in rows:
            f.write("\t".join(r[c] for c in cols) + "\n")
    print(len(rows), "rows,", len({r["code"] for r in rows if r["type"] == "dataset"}), "distinct data sets")
    if "--list-only" in sys.argv:
        return
    meta = {}
    for r in rows:
        if r["type"] == "dataset":
            meta.setdefault(r["code"], r)
    wanted = [c for c in READ if c in meta] + [c for c in meta if c.startswith(PREFIXES) and c not in READ]
    done = set()
    if os.path.exists(jsonl):
        with open(jsonl, encoding="utf-8") as f:
            done = {json.loads(line)["id"] for line in f}
    print(len(wanted), "structures wanted,", len(done), "already there")
    with open(jsonl, "a", encoding="utf-8") as out:
        for code in wanted:
            if code in done:
                continue
            m = meta[code]
            rec = {"id": code, "name": m["title"], "theme": m["theme"], "from": m["from"], "to": m["to"], "values": m["values"],
                   "updated": m["updated"]}
            try:
                rec["dimensions"] = structure(code)
            except Exception as e:
                rec["error"] = str(e)
                print("FAIL", code, e, flush=True)
            out.write(json.dumps(rec, ensure_ascii=False) + "\n")
            out.flush()
            time.sleep(PAUSE)


if __name__ == "__main__":
    main()
