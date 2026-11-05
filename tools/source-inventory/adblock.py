#!/usr/bin/env python3
"""Inventory of the ad blocker's filter lists: reads their URLs from src/AdBlock.cpp, downloads each one in turn with
the app's User-Agent, and writes docs/sources/catalog/adblock.tsv (size, lines, rule kinds, the list's own header).
Sequential, 2 s apart, robots.txt obeyed. Nothing from the lists is kept beyond counts and header fields.

usage: python3 -I tools/source-inventory/adblock.py <repo root>
"""
import gzip
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import urllib.robotparser

ROOT = sys.argv[1]
UA = "Deepwater/0.1 (desktop app; ad blocker lists)"   # src/AdBlock.cpp
FIELDS = ["list", "url", "host", "status", "bytes", "lines", "comment_lines", "network_rules", "exception_rules",
          "cosmetic_rules", "scriptlet_rules", "header_title", "header_expires", "last_modified"]


def get(url):
    req = urllib.request.Request(url, headers={"User-Agent": UA, "Accept-Encoding": "gzip"})
    try:
        r = urllib.request.urlopen(req, timeout=60)
        status = r.status
    except urllib.error.HTTPError as e:
        r, status = e, e.code
    body = r.read()
    if (r.headers.get("Content-Encoding") or "").lower() == "gzip":
        body = gzip.decompress(body)
    return status, body, r.headers


robots = {}


def allowed(url):
    p = urllib.parse.urlsplit(url)
    if p.netloc not in robots:
        rp = urllib.robotparser.RobotFileParser()
        try:
            st, body, _ = get(f"{p.scheme}://{p.netloc}/robots.txt")
            rp.parse(body.decode("utf-8", "replace").splitlines() if st == 200 else [])
        except Exception:
            rp.parse([])
        robots[p.netloc] = rp
        time.sleep(1)
    return robots[p.netloc].can_fetch(UA, url)


src = open(f"{ROOT}/src/AdBlock.cpp", encoding="utf-8").read()
lists = re.findall(r'\{"([^"]+)", "(https://[^"]+)"\}', src)
with open(f"{ROOT}/docs/sources/catalog/adblock.tsv", "w", encoding="utf-8") as out:
    out.write("\t".join(FIELDS) + "\n")
    for name, url in lists:
        host = urllib.parse.urlsplit(url).netloc
        row = dict.fromkeys(FIELDS, "")
        row.update(list=name, url=url, host=host)
        if not allowed(url):
            row["status"] = "robots-disallowed"
        else:
            try:
                st, body, h = get(url)
                row["status"] = st
                row["bytes"] = len(body)
                row["last_modified"] = h.get("Last-Modified", "")
                lines = body.decode("utf-8", "replace").splitlines()
                row["lines"] = len(lines)
                comment = exception = cosmetic = scriptlet = network = 0
                for ln in lines:
                    s = ln.strip()
                    if not s:
                        continue
                    if s.startswith("!") or s.startswith("# ") or s == "#" or s.startswith("[Adblock"):
                        comment += 1
                        m = re.match(r"[!#]\s*(Title|Expires)\s*:\s*(.*)", s)
                        if m:
                            key = "header_title" if m.group(1) == "Title" else "header_expires"
                            row[key] = row[key] or m.group(2)[:60]
                    elif "##+js(" in s or "#%#" in s or "#@#+js(" in s:
                        scriptlet += 1
                    elif "##" in s or "#@#" in s or "#?#" in s:
                        cosmetic += 1
                    elif s.startswith("@@"):
                        exception += 1
                    else:
                        network += 1
                row.update(comment_lines=comment, network_rules=network, exception_rules=exception,
                           cosmetic_rules=cosmetic, scriptlet_rules=scriptlet)
            except Exception as e:
                row["status"] = "ERR " + repr(e)[:60]
        out.write("\t".join(str(row[f]) for f in FIELDS) + "\n")
        out.flush()
        print(name, row["status"], row["bytes"], flush=True)
        time.sleep(2)
