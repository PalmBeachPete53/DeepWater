#!/usr/bin/env python3
"""IDB's data portal (CKAN): every package, its organisation, formats, tags and period if stated.

Metadata only, no resource is downloaded. One call at a time, a second apart. Run with `python3 -I`.

    iadb.py docs/sources/catalog/iadb.tsv
"""
import json
import sys
import time
import urllib.request

BASE = "https://data.iadb.org/api/3/action"
HDR = {"User-Agent": "Deepwater/1.0"}


def fetch(path):
    time.sleep(1)
    with urllib.request.urlopen(urllib.request.Request(BASE + path, headers=HDR), timeout=60) as r:
        return json.load(r)["result"]


def text(v):
    """A title may be a plain string or one per language."""
    return (v.get("en") or next(iter(v.values()), "")) if isinstance(v, dict) else (v or "")


with open(sys.argv[1], "w", encoding="utf-8") as out:
    out.write("name\ttitle\torganization\tformats\tresources\tmodified\ttags\n")
    for name in fetch("/package_list"):
        try:
            p = fetch("/package_show?id=" + name)
        except Exception as e:
            out.write(f"{name}\terror: {e}\t\t\t\t\t\n")
            continue
        res = p.get("resources", [])
        fmts = sorted({(r.get("format") or "").upper() for r in res if r.get("format")})
        tags = [text(t.get("name")) for t in p.get("tags", [])][:12]
        cols = [name, text(p.get("title")), text((p.get("organization") or {}).get("title")), ",".join(fmts), str(len(res)),
                (p.get("metadata_modified") or "")[:10], ",".join(tags)]
        out.write("\t".join(" ".join(c.split()) for c in cols) + "\n")
        out.flush()
