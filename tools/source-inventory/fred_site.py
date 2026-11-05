#!/usr/bin/env python3
"""FRED's API needs a key; its public pages do not. Lists what those pages list: every release, every source, every tag,
and the category tree breadth first (name, parent, the page's rounded series count). One page at a time, 2 s apart,
stops on the first refusal. No series values.

    fred_site.py <catalog dir> <call log> [max category pages]
"""
import html, os, re, sys, time, urllib.error, urllib.request

UA = "Deepwater/1.0"
BASE = "https://fred.stlouisfed.org"


class Refused(Exception):
    pass


def page(path, log):
    time.sleep(2)
    url = BASE + path
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    try:
        with urllib.request.urlopen(req, timeout=40) as r:
            body = r.read().decode("utf-8", "replace")
            status = r.status
    except urllib.error.HTTPError as e:
        log.write(f"GET {url} -> {e.code}\n"); log.flush()
        raise Refused(str(e.code))
    log.write(f"GET {url} -> {status} {len(body)}\n"); log.flush()
    return body


def clean(s):
    return re.sub(r"\s+", " ", html.unescape(s)).strip()


def listing(first, pattern, log):
    rows, n = {}, 1
    while True:
        body = page(f"{first}{'&' if '?' in first else '?'}pageID={n}" if n > 1 else first, log)
        found = re.findall(pattern, body)
        new = [f for f in found if f[0] not in rows]
        for k, name in new:
            rows[k] = clean(name)
        if not new or f"pageID={n + 1}" not in body:
            return rows
        n += 1


def main():
    out, log_path = sys.argv[1:3]
    cap = int(sys.argv[3]) if len(sys.argv) > 3 else 900
    log = open(log_path, "a")
    try:
        rel = listing("/releases", r'href="/release\?rid=(\d+)"[^>]*>([^<]+)</a>', log)
        with open(os.path.join(out, "fred_releases.tsv"), "w") as f:
            f.write("release_id\tname\n")
            for k, v in sorted(rel.items(), key=lambda x: int(x[0])):
                f.write(f"{k}\t{v}\n")
        src = listing("/sources", r'href="/source\?soid=(\d+)"[^>]*>([^<]+)</a>', log)
        with open(os.path.join(out, "fred_sources.tsv"), "w") as f:
            f.write("source_id\tname\n")
            for k, v in sorted(src.items(), key=lambda x: int(x[0])):
                f.write(f"{k}\t{v}\n")
        seen = {"0": ("", "Categories", "")}
        queue = ["0"]
        order = []
        pages = 0
        with open(os.path.join(out, "fred_categories.tsv"), "w") as f:
            f.write("category_id\tparent_id\tdepth\tname\tseries_rounded\texpanded\n")
            depth = {"0": 0}
            while queue:
                cid = queue.pop(0)
                if pages >= cap:
                    p, name, cnt = seen[cid]
                    f.write(f"{cid}\t{p}\t{depth[cid]}\t{name}\t{cnt}\tno\n")
                    continue
                body = page("/categories" if cid == "0" else f"/categories/{cid}", log)
                pages += 1
                p, name, cnt = seen[cid]
                f.write(f"{cid}\t{p}\t{depth[cid]}\t{name}\t{cnt}\tyes\n")
                f.flush()
                for k, nm, c in re.findall(r'href="/categories/(\d+)">(?:<strong>)?([^<]+)(?:</strong>)?</a>(?:\s*<span class="category-count">|&nbsp;)\(([^)]*)\)', body):
                    if k not in seen:
                        seen[k] = (cid, clean(nm), c)
                        depth[k] = depth[cid] + 1
                        queue.append(k)
        print("releases", len(rel), "sources", len(src), "categories", len(seen), "pages", pages)
    except Refused as e:
        print("refused", e)


main()
