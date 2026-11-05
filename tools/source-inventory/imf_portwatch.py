#!/usr/bin/env python3
"""IMF ArcGIS organisation weJ1QsnbMYJlCHdG (PortWatch, Climate Data Dashboard): every service, layer and field.

Structure only: names, field types, row counts, first and last date. Paced, resumable.

The JSON directory answers are unreliable on this organisation. For a good share of the services, `FeatureServer?f=json`,
`.../layers?f=json` and `.../0?f=json` answer HTTP 200 with {"error":{"message":"Invalid URL"}} for minutes at a time, and
`layers?f=json` can answer an empty {"layers":[],"tables":[]} for a service that has layers. The HTML directory page of a
layer (`.../FeatureServer/0`, no `f`) answers: it is what this script parses, with the service's HTML page for the layer
ids, then for PortWatch proper the JSON query for the row count and the date range, retried.

    python3 -I imf_portwatch.py docs/sources/catalog

Writes imf_portwatch.jsonl (one line per service, its layers and fields) and imf_portwatch.tsv (one line per layer).
"""
import html
import json
import os
import re
import sys
import time
import urllib.parse
import urllib.request

BASE = "https://services9.arcgis.com/weJ1QsnbMYJlCHdG/arcgis/rest/services"
HDR = {"User-Agent": "Deepwater/1.0"}
PAUSE = 1.0
calls = {"ok": 0, "failed": 0}
# The services of PortWatch proper, asked first and with their row counts and dates: the rest of the organisation is the
# Climate Data Dashboard, for which the fields are enough (each answer takes seconds).
FAMILY = re.compile(r"Daily_|PortWatch|portwatch|disruption|Monthly_TradeNow|Container_Metrics|spillover|Spillover|scenarios_trade"
                    r"|Bilateral|geopulse|gdacs|airports|FR24|Global_Shipping|portsfacts|PW_Globe|sankey|events_wp|L2_|L3_|country_")


def get(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers=HDR), timeout=60) as r:
        return r.read().decode("utf-8", "replace")


def fetch(url, tries=2):
    last = None
    for attempt in range(tries):
        time.sleep(PAUSE * (attempt + 1))
        # The same path answers "Invalid URL" under one spelling and not the other, at different moments.
        spelled = url.replace("/arcgis/rest/", "/ArcGIS/rest/") if attempt % 2 else url
        try:
            d = json.loads(get(spelled))
            if "error" not in d:
                calls["ok"] += 1
                return d
            last = d["error"].get("message", "error")
        except Exception as e:  # noqa: BLE001
            last = repr(e)[:120]
    calls["failed"] += 1
    return {"_error": last}


def page(path, tries=2):
    """An HTML directory page, or None when it says Invalid URL or that nothing is there."""
    for attempt in range(tries):
        time.sleep(PAUSE * (attempt + 1))
        try:
            b = get(f"{BASE}/{path}")
        except Exception:  # noqa: BLE001
            continue
        if "restBody" in b and "Invalid URL" not in b and "does not exist" not in b:
            return b
    return None


def text_after(body, label):
    m = re.search(rf"<b>{label}:</b>\s*(.*?)<br", body, re.S)
    return html.unescape(re.sub(r"<[^>]+>", "", m.group(1))).strip() if m else ""


def layer_ids(quoted):
    b = page(f"{quoted}/FeatureServer")
    if b:
        ids = sorted({int(x) for x in re.findall(r'FeatureServer/(\d+)"', b)})
        if ids:
            return ids, b
    return [], b


def layer(quoted, lid, deep):
    b = page(f"{quoted}/FeatureServer/{lid}")
    if not b:
        return None
    fields = [{"name": html.unescape(n), "type": t}
              for n, t in re.findall(r"<li>\s*([^<]+?)\s*<i>\(type: esriFieldType(\w+)", b)]
    row = {"id": lid, "name": text_after(b, "Name"), "kind": text_after(b, "Type"),
           "geometry": text_after(b, "Geometry Type").replace("esriGeometry", ""),
           "description": text_after(b, "Description")[:300],
           "lastEdit": text_after(b, "Data Last Edit Date") or text_after(b, "Last Edit Date"), "fields": fields}
    if not row["name"] and not fields:
        return None     # a page in the directory's layout, for a layer that is not there or did not load
    if not deep:
        return row
    q = urllib.parse.urlencode({"where": "1=1", "returnCountOnly": "true", "f": "json"})
    row["rows"] = fetch(f"{BASE}/{quoted}/FeatureServer/{lid}/query?{q}").get("count")
    dates = [f["name"] for f in fields if f["type"] in ("Date", "DateOnly")]
    if dates:
        row["date_field"] = dates[0]
        stats = json.dumps([{"statisticType": "min", "onStatisticField": dates[0], "outStatisticFieldName": "a"},
                            {"statisticType": "max", "onStatisticField": dates[0], "outStatisticFieldName": "b"}])
        q = urllib.parse.urlencode({"where": "1=1", "outStatistics": stats, "f": "json"})
        try:
            a = fetch(f"{BASE}/{quoted}/FeatureServer/{lid}/query?{q}")["features"][0]["attributes"]
            conv = lambda v: time.strftime("%Y-%m-%d", time.gmtime(v / 1000)) if isinstance(v, (int, float)) else v
            row["first"], row["last"] = conv(a.get("a")), conv(a.get("b"))
        except (KeyError, IndexError):
            pass
    return row


def main():
    out = sys.argv[1]
    jsonl, tsv = os.path.join(out, "imf_portwatch.jsonl"), os.path.join(out, "imf_portwatch.tsv")
    services = json.loads(get(BASE + "?f=json"))["services"]
    print(len(services), "services", flush=True)
    done = {}
    if os.path.exists(jsonl):
        with open(jsonl, encoding="utf-8") as f:
            for line in f:
                if line.strip():
                    x = json.loads(line)
                    done[x["service"]] = x
    with open(jsonl, "a", encoding="utf-8") as j:
        for s in sorted(services, key=lambda s: not FAMILY.search(s["name"])):
            name = s["name"]
            if name in done and (done[name].get("layers") or done[name].get("final")):
                continue
            rec = {"service": name, "type": s["type"]}
            if s["type"] != "FeatureServer":
                rec["final"] = True
                done[name] = rec
                j.write(json.dumps(rec, ensure_ascii=False) + "\n")
                continue
            quoted = urllib.parse.quote(name)
            deep = bool(FAMILY.search(name))
            ids, root = layer_ids(quoted)
            layers = []
            if ids:
                for lid in ids:
                    row = layer(quoted, lid, deep)
                    if row:
                        layers.append(row)
            else:
                # The service page answered nothing useful: walk ids from 0 until one is missing.
                for lid in range(0, 8):
                    row = layer(quoted, lid, deep)
                    if not row:
                        break
                    layers.append(row)
            rec["layers"] = layers
            if not layers:
                rec["error"] = "no layer page answered" if root is None else "the service page lists no layer"
            done[name] = rec
            j.write(json.dumps(rec, ensure_ascii=False) + "\n")
            j.flush()
            print(name, len(layers), rec.get("error", ""), flush=True)
    with open(jsonl, "w", encoding="utf-8") as j:
        j.writelines(json.dumps(x, ensure_ascii=False) + "\n" for x in done.values())
    with open(tsv, "w", encoding="utf-8") as f:
        f.write("service\tlayer_id\tlayer\tkind\tgeometry\trows\tdate_field\tfirst\tlast\tn_fields\tfields\n")
        for name in sorted(done):
            x = done[name]
            if not x.get("layers"):
                f.write(f"{name}\t\t\t{x.get('type')}\t\t\t\t\t\t\t{x.get('error', '')}\n")
            for lay in x.get("layers", []):
                fields = ",".join(g["name"] for g in lay["fields"])
                f.write("\t".join(str(v if v is not None else "") for v in (
                    name, lay["id"], lay["name"], lay["kind"], lay["geometry"], lay.get("rows"), lay.get("date_field"),
                    lay.get("first"), lay.get("last"), len(lay["fields"]), fields)) + "\n")
    print("done", calls, flush=True)


if __name__ == "__main__":
    main()
