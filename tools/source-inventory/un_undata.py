#!/usr/bin/env python3
"""UNdata's SDMX service: every dataflow with its dimensions, what is available and its period range.

Structure only, no observation. One call at a time, two seconds apart. Run with `python3 -I`.

    un_undata.py docs/sources/catalog/un_undata_structures.jsonl
"""
import json
import sys
import time
import urllib.request
import xml.etree.ElementTree as ET

BASE = "https://data.un.org/ws/rest"
HDR = {"User-Agent": "Deepwater/1.0"}
S = "{http://www.sdmx.org/resources/sdmxml/schemas/v2_1/structure}"
C = "{http://www.sdmx.org/resources/sdmxml/schemas/v2_1/common}"
XL = "{http://www.w3.org/XML/1998/namespace}lang"


def fetch(url):
    time.sleep(2)
    with urllib.request.urlopen(urllib.request.Request(url, headers=HDR), timeout=180) as r:
        return ET.fromstring(r.read())


def name_of(el):
    names = el.findall(C + "Name")
    return next((n.text or "" for n in names if n.get(XL) == "en"), names[0].text if names else "")


with open(sys.argv[1], "w", encoding="utf-8") as out:
    for d in list(fetch(f"{BASE}/dataflow/").iter(S + "Dataflow")):
        agency, fid, ver = d.get("agencyID"), d.get("id"), d.get("version")
        rec = {"id": fid, "agency": agency, "version": ver, "name": name_of(d)}
        try:
            root = fetch(f"{BASE}/dataflow/{agency}/{fid}/{ver}?references=all")
            lists = {cl.get("id"): {c.get("id"): name_of(c) for c in cl.findall(S + "Code")} for cl in root.iter(S + "Codelist")}
            dims = []
            for x in root.iter(S + "Dimension"):
                if not x.get("id"):  # the constraint's references to a dimension, not dimensions
                    continue
                ref = x.find(f"{S}LocalRepresentation/{S}Enumeration/Ref")
                dims.append({"id": x.get("id"), "codelist": ref.get("id") if ref is not None else ""})
            try:
                root = fetch(f"{BASE}/availableconstraint/{agency},{fid},{ver}/all/all/all")
                avail, obs = {}, None
                for region in root.iter(S + "CubeRegion"):
                    for kv in region.findall(C + "KeyValue"):
                        if kv.get("id") == "TIME_PERIOD":
                            tr = kv.find(C + "TimeRange")
                            if tr is not None:
                                rec["from"] = (tr.find(C + "StartPeriod").text if tr.find(C + "StartPeriod") is not None else "")
                                rec["to"] = (tr.find(C + "EndPeriod").text if tr.find(C + "EndPeriod") is not None else "")
                            else:
                                vals = [v.text for v in kv.findall(C + "Value")]
                                rec["from"], rec["to"] = min(vals, default=""), max(vals, default="")
                        else:
                            avail[kv.get("id")] = [v.text for v in kv.findall(C + "Value")]
                for a in root.iter(C + "Annotation"):
                    t = a.find(C + "AnnotationType")
                    if t is not None and t.text == "sdmx_metrics":
                        rec["metrics"] = a.findtext(C + "AnnotationTitle")
            except Exception as e:
                avail = {}
                rec["available_error"] = str(e)
            for dim in dims:
                names = lists.get(dim["codelist"], {})
                vals = avail.get(dim["id"], list(names))
                dim["codes_in_list"] = len(names)
                dim["codes_available"] = len(vals)
                dim["codes"] = {v: names.get(v, "") for v in vals[:60]}
            rec["dimensions"] = dims
        except Exception as e:
            rec["error"] = str(e)
        print(fid, rec.get("from"), rec.get("to"), rec.get("error") or rec.get("available_error") or "", flush=True)
        out.write(json.dumps(rec, ensure_ascii=False) + "\n")
        out.flush()
