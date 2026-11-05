#!/usr/bin/env python3
"""One correction of resources/data_sources.json, from the inventory of 5 November 2026 (run once, kept as the record).

The catalogue came from a crawl of Trading Economics: its links are those the site credits and its `api` column was
guessed from names. This applies what the calls established:
- `api`: docs/sources/catalog/api_corrections.tsv, then what the cards and calls showed;
- `url`: wrong links replaced by a site that answered, or removed ("no site known") when none did;
- duplicates: name variants merged into the entry with the most pages, the other names kept in `aliases`.

Countries' indicators point to entries by index, so they are renumbered. Nothing is guessed: a site enters only if it
answers here, and a merge needs a card, or the same country and domain plus an agent's note that they are one body.

    fix_catalogue.py <candidates.json from the verification> docs/sources/catalogue-corrections.md
"""
import collections
import glob
import json
import re
import ssl
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

DATA = "resources/data_sources.json"
TODAY = "2026-11-05"

# By the card of the body: its catalogue names get this `api`.
CARD_API = {
    "abs": "ABS Data API (SDMX), no key",
    "adb": "KIDB SDMX API, no key",
    "beac": "statistical files (XLS, PDF), no key",
    "boi": "SDMX API (Fusion Edge), no key",
    "boj": "Time-Series Data Search API (JSON), no key",
    "census": "Census Data API, free key",
    "european_commission": "Redisstat SDMX API, no key",
    "fao": "FAOSTAT bulk files (CSV), no key",
    "gus": "BDL API, key optional",
    "iadb": "data portal CKAN API, no key",
    "ilo": "ILOSTAT API, no key",
    "indec": "datos.gob.ar series API, no key",
    "insee": "BDM SDMX API and Melodi API, no key",
    "mofjapan": "statistical files (CSV, XLS), no key",
    "mospi": "MCP server and eSankhyiki (CSV), no key",
    "nbb": ".Stat Suite SDMX API, no key",
    "nbschina": "web (its database refuses programs)",
    "rbi": "web (DBIE is a browser application)",
    "secmca": "SIMAFIR API, free account",
    "sipri": "data files (XLSX), no key",
    "statistik_at": "open data catalogue (CSV), no key",
    "statsnz": "bulk CSV files; Aotearoa Data Explorer API, free key",
    "transparency": "data file (XLSX), no key",
    "tuik": "SDMX API, free token; web tables (XLS)",
    "unece": "PxWeb API, no key",
    "unhcr": "Refugee Statistics API, no key",
    "usda": "PSD data files (CSV), no key",
    "who": "GHO OData API, no key",
}

# By the body's name start, where its API page answered on 5 Nov 2026.
BODY_API = {
    "Bank of Slovenia": "PxWeb API, no key",
    "Bundesagentur für Arbeit": "statistics API",
    "Central Bank of Dominican Republic": "API BCRD",
    "Central Informatics Organisation": "open data portal API",
    "Information & eGovernment Authority": "open data portal API",
    "Central Reserve Bank of Peru": "BCRPData API, no key",
    "Central Statistical Bureau of Latvia": "PxWeb API, no key",
    "Croatian Bureau of Statistics": "PX-Web tables, no key",
    "Department of Statistics (DOS), Jordan": "PxWeb API, no key",
    "The Hashemite Kingdom of Jordan Depa": "PxWeb API, no key",
    "Department of Statistics, Malaysia": "OpenDOSM API, no key",
    "Hong Kong Monetary Authority": "HKMA API, no key",
    "Korea Real Estate Board": "R-ONE Open API",
    "Kosovo Agency of Statistics": "PxWeb API, no key",
    "Ministry Of Finance Of Slovak Republ": "SDMX (SDDS+) files, no key",
    "Ministry of Land, Infrastructure and": "Statnuri Open API",
    "Ministère de la Transition écologiqu": "DiDo API, no key",
    "National Bureau of Statistics of the": "PxWeb API, no key",
    "Statistica Moldovei": "PxWeb API, no key",
    "National Institute of Economic Resea": "PxWeb API, no key",
    "National Institute of Statistics - T": "data portal API",
    "National Statistical Office of Thail": "SDMX registry",
    "State Statistical Office of the Repu": "PxWeb API, no key",
    "State Treasury, Finland": "Tutkihallintoa API, no key",
    "Statistics Iceland": "PxWeb API, no key",
}

# Links that lead to the wrong organisation, each read on its site by an agent.
WRONG_LINK = [
    "CNSEE, Congo", "DRC Tax Authority", "Danish Central Tax Administration", "Electricite du Liban",
    "Estonian Tax and Customs Board", "Finance Department, San Marino", "Instituto Ecuatoriano de Seguridad",
    "Ministry Of Finance, Republic of L", "Ministry of Finance and Economic D", "Ministry of Finance and Economy of",
    "Ministry of Finance, Ukraine", "Ministry of Finance, Yemen", "Ministry of Social Policy of Ukrai",
    "Ministry of Tourism & Sports, Thai", "New Zealand Inland Revenue Departm", "Norwegian Tax Administration",
    "Solomon Islands National Statistic", "State Tax Inspectorate", "Tax Administration of the Federati",
    "Tax Administration of the Republic", "Ministry Of Labour and Social Welf", "Ministry of Finance, Republic of K",
    "DILF - Danish Purchasing", "World Health Organization",
]
# Sites to try for those: entered only if they answer.
TRY_SITE = {
    "New Zealand Inland Revenue Departm": "https://www.ird.govt.nz/",
    "Norwegian Tax Administration": "https://www.skatteetaten.no/en/",
    "Estonian Tax and Customs Board": "https://www.emta.ee/en",
    "Tax Administration of the Republic": "https://www.fu.gov.si/en/",
    "Danish Central Tax Administration": "https://skat.dk/",
    "Ministry of Finance, Ukraine": "https://mof.gov.ua/en",
    "State Tax Inspectorate": "https://www.vmi.lt/",
    "World Health Organization": "https://www.who.int/data/gho",
}
# A former address replaced by the one it became.
MOVED = ["Czech Social Security Administration", "Directorate of Internal Revenue", "Federal Customs Administration",
         "Hellenic Republic Ministry of Econ", "Ministry of Finance, Andorra"]
# A site an agent found that is another body's.
REJECT_SITE = ["Ministry of Labour and Immigration"]

# Same country and domain, but not one body: left apart, or only some of the names merged.
NOT_MERGED = {
    "statbel.fgov.be",  # BelgoStat belongs to the National Bank's card
    "english.mofe.go.kr", "gub.uy", "gov.kz", "argentina.gob.ar", "gov.uk", "gov.za", "gov.gd", "rks-gov.net", "gov.im",
    "mfinante.gov.ro",
}
LEAVE_OUT = re.compile(r"^(Ministry of Finance and Economy of|Ministry of Finance - Kingdom of Bah|Federal Office of Statistics, "
                       r"Federati|Norwegian Tax Administration|Gobierno de Mexico|Secretariat of Labor)")
SKIP_CARDS = {"private-institutes", "fed-regional", "us-other-agencies", "fred", "treasury"}
VENDOR = re.compile(r"(^|\.)(vatlive|avalara|kpmg|ey|pwc|rsm|rsmus|expatica|iamexpat|ncbi\.nlm\.nih)\.", re.I)
VARIANT = re.compile(r"(?i)duplicate|name variant|variant of|same body")
CTX = ssl.create_default_context()


def host(u):
    h = urllib.parse.urlsplit(u if "://" in u else "https://" + u).netloc.lower()
    return re.sub(r"^www\d?\.", "", h)


def probe(url):
    t = time.time()
    try:
        r = urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": "Deepwater/1.0"}), timeout=20, context=CTX)
        r.read(2000)
        return r.status, int((time.time() - t) * 1000), ""
    except urllib.error.HTTPError as e:
        return e.code, int((time.time() - t) * 1000), f"http {e.code}"
    except Exception as e:
        return 0, int((time.time() - t) * 1000), type(e).__name__


def starts(name, keys):
    return any(name.startswith(k) for k in keys)


def main():
    cands = json.load(open(sys.argv[1], encoding="utf-8"))
    report = sys.argv[2]
    doc = json.load(open(DATA, encoding="utf-8"))
    S = doc["sources"]
    by_name = {s["name"]: s for s in S}
    bodies = {json.loads(l)["name"]: json.loads(l) for l in open("docs/sources/catalog/bodies.jsonl", encoding="utf-8")}
    log = collections.defaultdict(list)

    # api
    for line in open("docs/sources/catalog/api_corrections.tsv", encoding="utf-8"):
        if "\t" in line:
            name, api = line.rstrip("\n").split("\t")[:2]
            if name in by_name and by_name[name]["api"] != api:
                log["api"].append((name, by_name[name]["api"], api))
                by_name[name]["api"] = api
    groups = []
    for path in glob.glob("docs/sources/*.md"):
        cid = path.split("/")[-1][:-3]
        m = re.match(r"---\n(.*?)\n---\n", open(path, encoding="utf-8").read(), re.S)
        c = re.search(r"^catalogue:\s*(\[.*\])", m.group(1), re.M) if m else None
        try:
            names = [n for n in json.loads(c.group(1)) if n in by_name] if c else []
        except ValueError:
            names = []
        if cid in CARD_API:
            for n in names:
                if by_name[n]["api"] != CARD_API[cid]:
                    log["api"].append((n, by_name[n]["api"], CARD_API[cid]))
                    by_name[n]["api"] = CARD_API[cid]
        if cid not in SKIP_CARDS and len(names) > 1:
            groups.append(names)
    for s in S:
        for key, api in BODY_API.items():
            if s["name"].startswith(key) and s["api"] != api:
                log["api"].append((s["name"], s["api"], api))
                s["api"] = api

    # url
    def set_url(s, url, why):
        status, ms, error = probe(url) if url else (0, 0, "no link")
        if url and status not in (200, 401, 403):
            return False
        log["url"].append((s["name"], s["url"], url, why))
        s["url"] = url
        s["test"] = {"date": TODAY, "ok": status == 200, "ms": ms}
        if status:
            s["test"]["status"] = status
        if error:
            s["test"]["error"] = error
        return True

    for name, c in cands["cands"].items():
        s = by_name[name]
        site = c["site"]
        answered = site and cands["res"].get(site, [0])[0] in (200, 401, 403) and not starts(name, REJECT_SITE)
        if starts(name, WRONG_LINK):
            for key, url in TRY_SITE.items():
                if name.startswith(key) and set_url(s, url, "wrong link; official site answers"):
                    break
            else:
                if host(s["url"]) and s["url"].startswith("http"):
                    set_url(s, "", "wrong link; no site confirmed")
        elif c["vendor"]:
            if not (answered and not VENDOR.search("." + host(site) + ".") and set_url(s, site, "link led to a vendor")):
                set_url(s, "", "link led to a vendor; no site confirmed")
        elif not c["url"].startswith("http"):
            if answered:
                set_url(s, site, "no link; site found and answers")
        elif starts(name, MOVED) and answered:
            set_url(s, site, "moved")

    # merge
    parent = {s["name"]: s["name"] for s in S}

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    def union(names):
        names = [n for n in names if not LEAVE_OUT.match(n)]
        for n in names[1:]:
            parent[find(n)] = find(names[0])

    for g in groups:
        union(g)
    domain = collections.defaultdict(list)
    for n, b in bodies.items():
        s = by_name[n]
        u = s["url"] if s["url"].startswith("http") else ""
        if u:
            domain[(tuple(sorted(s["countries"])), s["kind"], host(u))].append(n)
    for key, names in domain.items():
        if len(names) > 1 and key[2] not in NOT_MERGED and any(VARIANT.search(bodies[n]["note"]) for n in names):
            union(names)

    members = collections.defaultdict(list)
    for s in S:
        members[find(s["name"])].append(s)
    new, remap = [], {}
    for i, s in enumerate(S):
        group = members[find(s["name"])]
        head = max(group, key=lambda x: (x["pages"], -S.index(x)))
        if s is not head:
            continue
        merged = dict(head)
        if len(group) > 1:
            merged["aliases"] = sorted(x["name"] for x in group if x is not head)
            merged["countries"] = sorted({c for x in group for c in x["countries"]})
            merged["pages"] = sum(x["pages"] for x in group)
            if not merged["url"].startswith("http"):
                other = next((x for x in group if x["url"].startswith("http")), None)
                if other:
                    merged["url"], merged["test"] = other["url"], other["test"]
            if merged["api"] in ("web", "web tables"):
                merged["api"] = next((x["api"] for x in group if x["api"] not in ("web", "web tables")), merged["api"])
            log["merge"].append((merged["name"], merged["aliases"]))
        new.append(merged)
    index = {m["name"]: i for i, m in enumerate(new)}
    for i, s in enumerate(S):
        head = max(members[find(s["name"])], key=lambda x: (x["pages"], -S.index(x)))
        remap[i] = index[head["name"]]
    for country in doc["countries"].values():
        country["indicators"] = {k: remap[v] for k, v in country["indicators"].items()}
    doc["sources"] = new
    doc["corrected"] = TODAY
    doc["about"] += (" Corrected by Deepwater on " + TODAY + " after calling the sources: wrong links and programmatic "
                     "access fixed, name variants merged (see docs/sources/catalogue-corrections.md).")
    with open(DATA, "w", encoding="utf-8") as f:
        json.dump(doc, f, ensure_ascii=False, separators=(",", ":"))

    with open(report, "w", encoding="utf-8") as f:
        f.write(f"# Corrections to the catalogue, {TODAY}\n\nApplied to `resources/data_sources.json` by "
                f"`tools/source-inventory/fix_catalogue.py`: {len(S)} entries became {len(new)}.\n\n")
        f.write(f"## Links ({len(log['url'])})\n\n| Body | Was | Now | Why |\n|---|---|---|---|\n")
        for n, a, b, why in sorted(log["url"]):
            f.write(f"| {n} | {a or '-'} | {b or '-'} | {why} |\n")
        f.write(f"\n## Programmatic access ({len(log['api'])})\n\n| Body | Was | Now |\n|---|---|---|\n")
        for n, a, b in sorted(log["api"]):
            f.write(f"| {n} | {a} | {b} |\n")
        f.write(f"\n## Name variants merged ({len(log['merge'])} bodies)\n\n| Body | Other names |\n|---|---|\n")
        for n, al in sorted(log["merge"]):
            f.write(f"| {n} | {'; '.join(al)} |\n")
    print(f"{len(S)} -> {len(new)} entries; links {len(log['url'])}, api {len(log['api'])}, merged bodies {len(log['merge'])}")
    print("set empty", sum(1 for x in log["url"] if not x[2]), "new site", sum(1 for x in log["url"] if x[2]))


if __name__ == "__main__":
    main()
