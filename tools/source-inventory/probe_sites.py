#!/usr/bin/env python3
"""Calls every body of resources/data_sources.json once and records what its site says it holds.

Keeps structure only (title, sections, links to data pages, file formats, API hints), never figures. One GET of the
home page, robots.txt read first and obeyed, then up to three data pages of the same site. Identifies itself plainly;
a site that refuses is recorded as refusing, never retried under another identity. Resumable: bodies already in the
output are skipped.

    probe_sites.py resources/data_sources.json docs/sources/catalog/bodies_probe.jsonl [workers]
"""
import concurrent.futures as cf
import gzip
import json
import re
import ssl
import sys
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
import urllib.robotparser
from html.parser import HTMLParser

UA = "Deepwater/1.0"
TIMEOUT = 20
MAX_BYTES = 1_500_000
DATA_WORDS = re.compile(
    r"statisti|\bstats?\b|\bdata\b|database|dataset|indicator|time.?series|open.?data|\bapi\b|publication|bulletin|"
    r"donn[ée]es|indicateur|estad[ií]stic|\bdatos\b|indicador|estat[ií]stic|\bdados\b|statistik|\bdaten\b|statistiche|"
    r"\bdati\b|statistiek|\bcijfers\b|statystyk|статист|данн|показател|إحصا|بيانات|统计|統計|数据|データ|통계|สถิติ|thống kê|"
    r"istatistik|\bveri\b|tilastot|statistikk|statistikk|štatist|statist", re.I)
FILE_RE = re.compile(r"\.(csv|xlsx?|pdf|json|xml|zip|px|sdmx|tsv|ods|dta|sav)(?:[?#]|$)", re.I)
API_RE = re.compile(r"\b(api|sdmx|odata|pxweb|px-web|json-stat|rest service|web service|developer|open data|opendata|ckan|"
                    r"data portal|dataverse|prognoz|\.stat\b)\b", re.I)
CTX = ssl.create_default_context()


class Page(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.title = ""
        self.lang = ""
        self.description = ""
        self.heads = []
        self.links = []
        self.feeds = []
        self._tag = None
        self._href = None
        self._buf = []

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if tag == "html":
            self.lang = a.get("lang", "") or a.get("xml:lang", "")
        elif tag == "meta" and (a.get("name", "") or "").lower() in ("description", "og:description"):
            self.description = self.description or (a.get("content") or "").strip()
        elif tag == "link" and "alternate" in (a.get("rel") or "") and "xml" in (a.get("type") or ""):
            self.feeds.append(a.get("href") or "")
        if tag in ("title", "h1", "h2", "h3", "a"):
            self._tag, self._buf = tag, []
            self._href = a.get("href") if tag == "a" else None

    def handle_data(self, data):
        if self._tag:
            self._buf.append(data)

    def handle_endtag(self, tag):
        if tag != self._tag:
            return
        text = re.sub(r"\s+", " ", "".join(self._buf)).strip()
        if tag == "title":
            self.title = self.title or text
        elif tag in ("h1", "h2", "h3") and text:
            self.heads.append(text)
        elif tag == "a" and self._href:
            self.links.append((text, self._href))
        self._tag = None


def get(url):
    req = urllib.request.Request(url, headers={"User-Agent": UA, "Accept-Encoding": "gzip", "Accept": "*/*"})
    with urllib.request.urlopen(req, timeout=TIMEOUT, context=CTX) as r:
        raw = r.read(MAX_BYTES)
        if r.headers.get("Content-Encoding") == "gzip":
            try:
                raw = gzip.decompress(raw)
            except Exception:
                pass
        charset = r.headers.get_content_charset() or "utf-8"
        return r.status, r.geturl(), r.headers.get_content_type(), raw.decode(charset, "replace")


def dedupe(items, limit):
    out = []
    seen = set()
    for x in items:
        key = x[1] if isinstance(x, tuple) else x
        if key and key not in seen:
            seen.add(key)
            out.append(x)
        if len(out) >= limit:
            break
    return out


def probe(body):
    rec = {"name": body["name"], "url": body["url"], "calls": []}
    url = body["url"]
    if not url.startswith("http"):
        rec["state"] = "no link"
        return rec
    t0 = time.time()
    try:
        host = urllib.parse.urlsplit(url)
        rp = urllib.robotparser.RobotFileParser()
        sitemaps = []
        try:
            _, _, _, robots = get(f"{host.scheme}://{host.netloc}/robots.txt")
            rp.parse(robots.splitlines())
            sitemaps = re.findall(r"(?im)^sitemap:\s*(\S+)", robots)
        except Exception:
            rp.parse([])
        if not rp.can_fetch(UA, url):
            rec["state"] = "robots.txt forbids"
            return rec
        status, final, ctype, text = get(url)
        rec["calls"].append([url, status])
        rec["final_url"] = final
        rec["state"] = "ok"
        page = Page()
        try:
            page.feed(text)
        except Exception:
            pass
        rec.update(title=page.title[:200], lang=page.lang[:8], description=page.description[:300],
                   headings=dedupe(page.heads, 25), sitemaps=sitemaps[:3], feeds=dedupe(page.feeds, 5))
        rec["javascript_only"] = len(page.links) < 3 and "<script" in text.lower()
        data_links, files, api = [], set(), set()
        for label, href in page.links:
            abs_url = urllib.parse.urljoin(final, href)
            if not abs_url.startswith("http"):
                continue
            m = FILE_RE.search(abs_url)
            if m:
                files.add(m.group(1).lower())
            if API_RE.search(label) or API_RE.search(abs_url):
                api.add(abs_url)
            if DATA_WORDS.search(label) or DATA_WORDS.search(urllib.parse.urlsplit(abs_url).path):
                data_links.append((label[:90], abs_url))
        data_links = dedupe(data_links, 40)
        rec["data_links"] = data_links
        sections = []
        same = urllib.parse.urlsplit(final).netloc
        followed = 0
        for label, link in data_links:
            if followed == 3:
                break
            if urllib.parse.urlsplit(link).netloc != same or FILE_RE.search(link) or not rp.can_fetch(UA, link):
                continue
            followed += 1
            time.sleep(1.0)
            try:
                s2, f2, _, t2 = get(link)
                rec["calls"].append([link, s2])
                p2 = Page()
                try:
                    p2.feed(t2)
                except Exception:
                    pass
                labels = [t for t, h in p2.links if DATA_WORDS.search(t) and 3 < len(t) < 90]
                for t, h in p2.links:
                    a2 = urllib.parse.urljoin(f2, h)
                    m = FILE_RE.search(a2)
                    if m:
                        files.add(m.group(1).lower())
                    if API_RE.search(t) or API_RE.search(a2):
                        api.add(a2)
                sections.append({"page": link, "title": p2.title[:140], "headings": dedupe(p2.heads, 20),
                                 "links": dedupe(labels, 30)})
            except urllib.error.HTTPError as e:
                rec["calls"].append([link, e.code])
            except Exception as e:
                rec["calls"].append([link, type(e).__name__])
        rec["sections"] = sections
        rec["file_formats"] = sorted(files)
        rec["api_hints"] = sorted(api)[:12]
    except urllib.error.HTTPError as e:
        rec["state"] = f"http {e.code}"
    except ssl.SSLError as e:
        rec["state"] = "tls error"
    except Exception as e:
        reason = getattr(e, "reason", e)
        rec["state"] = "tls error" if isinstance(reason, ssl.SSLError) else f"{type(reason).__name__}"
    rec["ms"] = int((time.time() - t0) * 1000)
    return rec


def main():
    data = json.load(open(sys.argv[1], encoding="utf-8"))
    out = sys.argv[2]
    workers = int(sys.argv[3]) if len(sys.argv) > 3 else 6
    done = set()
    try:
        with open(out, encoding="utf-8") as f:
            done = {json.loads(line)["name"] for line in f if line.strip()}
    except FileNotFoundError:
        pass
    todo = [b for b in data["sources"] if b["name"] not in done]
    print(f"{len(todo)} bodies to call, {len(done)} done", flush=True)
    lock = threading.Lock()
    n = 0
    with open(out, "a", encoding="utf-8") as f, cf.ThreadPoolExecutor(workers) as ex:
        for rec in ex.map(probe, todo):
            with lock:
                f.write(json.dumps(rec, ensure_ascii=False) + "\n")
                f.flush()
                n += 1
                if n % 100 == 0:
                    print(n, flush=True)
    print("done", n, flush=True)


if __name__ == "__main__":
    main()
