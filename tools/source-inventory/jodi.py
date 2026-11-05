#!/usr/bin/env python3
"""JODI: what the Oil and Gas CSV zips hold, by area and by item. Structure only: the values are read to tell a number from
a "-" or "x" (not reported) and thrown away.

    python3 -I jodi.py docs/sources/catalog/jodi.tsv primary=world_Primary_CSV.zip secondary=world_Secondary_CSV.zip gas=gas.zip

The zips come from https://www.jodidata.org/_resources/files/downloads/oil-data/ and
https://www.jodidata.org/jodi-publisher/gas/<publicationId>/ (publicationId from api.publisher.jodidata.org/web/files/gas).
Writes one line per file and area: first and last month with a number, number of items (product, flow, unit) with a number.
Prints the totals by file.
"""
import collections
import csv
import io
import sys
import zipfile


def scan(label, path, out):
    z = zipfile.ZipFile(path)
    name = z.namelist()[0]
    rows, numeric, blank = 0, 0, collections.Counter()
    areas, products, flows, units, codes = {}, set(), set(), set(), set()
    triples = set()
    with z.open(name) as f:
        reader = csv.reader(io.TextIOWrapper(f, encoding="utf-8", errors="replace"))
        header = next(reader)
        for r in reader:
            if len(r) < 7:
                continue
            rows += 1
            area, period, product, flow, unit, value, code = r[:7]
            products.add(product)
            flows.add(flow)
            units.add(unit)
            codes.add(code)
            triples.add((product, flow, unit))
            try:
                float(value)
            except ValueError:
                blank[value] += 1
                continue
            numeric += 1
            a = areas.setdefault(area, {"first": period, "last": period, "items": set()})
            a["first"], a["last"] = min(a["first"], period), max(a["last"], period)
            a["items"].add((product, flow, unit))
    print(label, name, "columns", header)
    print(" rows", rows, "with a number", numeric, "without", dict(blank))
    print(" areas with a number", len(areas), "products", sorted(products), "flows", sorted(flows), "units", sorted(units),
          "assessment codes", sorted(codes), "product-flow-unit combinations", len(triples))
    print(" first", min(a["first"] for a in areas.values()), "last", max(a["last"] for a in areas.values()))
    for area in sorted(areas):
        a = areas[area]
        out.write(f"{label}\t{area}\t{a['first']}\t{a['last']}\t{len(a['items'])}\n")


def main():
    with open(sys.argv[1], "w", encoding="utf-8") as out:
        out.write("file\tREF_AREA\tfirst_month_with_number\tlast_month_with_number\titems_with_number\n")
        for arg in sys.argv[2:]:
            label, path = arg.split("=", 1)
            scan(label, path, out)


if __name__ == "__main__":
    main()
