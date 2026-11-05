#!/usr/bin/env python3
"""The regional Reserve Banks' data pages: one row per page (title, data files it links). Reads robots.txt first, one
call at a time. No figures.

    fed_regional.py <pages.json> <out.tsv> <call log>
"""
import html, json, re, sys, time, urllib.error, urllib.request, urllib.robotparser

UA = "Deepwater/1.0"


def get(url, log):
    time.sleep(1.5)
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    try:
        with urllib.request.urlopen(req, timeout=40) as r:
            b = r.read(); log.write(f"GET {url} -> {r.status} {len(b)}\n"); log.flush()
            return r.status, b.decode("utf-8", "replace")
    except urllib.error.HTTPError as e:
        log.write(f"GET {url} -> {e.code}\n"); log.flush()
        return e.code, ""
    except Exception as e:
        log.write(f"GET {url} -> {repr(e)[:80]}\n"); log.flush()
        return "ERR", ""


def main():
    pages, out, log_path = sys.argv[1:4]
    log = open(log_path, "a")
    with open(out, "w") as f:
        f.write("bank\tpath\tstatus\ttitle\tdata_files\n")
        for bank, spec in json.load(open(pages)).items():
            base = spec["base"]
            st, rob = get(base + "/robots.txt", log)
            rp = urllib.robotparser.RobotFileParser()
            rp.parse(rob.splitlines() if st == 200 else [])
            for path in spec["pages"]:
                if not rp.can_fetch(UA, base + path):
                    f.write(f"{bank}\t{path}\trobots\t\t\n")
                    continue
                st, t = get(base + path, log)
                title = (re.findall(r"<title[^>]*>\s*(.*?)\s*</title>", t, re.S) or [""])[0]
                title = re.sub(r"\s+", " ", html.unescape(title))[:110]
                files = sorted({html.unescape(x) for x in re.findall(r'(?:href|data-url|data-href)="([^"]+\.(?:xlsx?|csv|zip|txt))(?:\?[^"]*)?"', t, re.I)} - {""})
                files = [x for x in files if "webmanifest" not in x]
                f.write(f"{bank}\t{path}\t{st}\t{title}\t{' '.join(x.replace(' ', '%20') for x in files[:40])}\n")
                f.flush()
    print("done")


main()
