#!/usr/bin/env python3
"""Puts a re-extraction of documents into graphify-out/graph.json without losing what the graph knew about them.

`/graphify . --update` replaces a document's nodes with the new extraction. An extraction never comes back with the
same node ids, and is often thinner, so graphify refuses to write the graph ("saved semantic node(s) are missing from
the new graph"), rightly: a one-line change to README.md would cost hundreds of nodes. This merges instead: the new
extraction, plus every earlier semantic node and edge of those documents that it doesn't repeat. A node for something
removed from a document therefore stays until a full rebuild; that is the price, and a small one.

Nothing else is written unless the graph was: the report, labels and manifest always describe graph.json.

Run after the skill's extraction, from the repository's root, with graphify's interpreter:

    $(cat graphify-out/.graphify_python) tools/graphify_update_docs.py graphify-out/.graphify_chunk_*.json
    $(cat graphify-out/.graphify_python) tools/graphify_update_docs.py --from-cache README.md CLAUDE.md   # already extracted
    $(cat graphify-out/.graphify_python) tools/graphify_update_docs.py --keep README.md   # edits that add no concept:
                                                     # no extraction, the document's nodes stay and it counts as current
"""
import argparse
import collections
import glob
import json
import subprocess
import sys
from pathlib import Path

from graphify.build import _lost_saved_semantic_nodes, build_merge
from graphify.cache import check_semantic_cache, save_semantic_cache
from graphify.cluster import cluster, remap_communities_to_previous
from graphify.detect import detect_incremental, save_manifest
from graphify.diagnostics import diagnose_extraction
from graphify.export import to_json
from graphify.paths import out_path
from graphify.watch import _read_build_excludes, _read_build_gitignore

GRAPH = Path("graphify-out/graph.json")
SPEC = ".claude/skills/graphify/references/extraction-spec.md"
MAX_FOLDED = 30


def relative(path):
    p = Path(path)
    return str(p.resolve().relative_to(Path.cwd())) if p.is_absolute() else str(p)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("inputs", nargs="+", help="chunk files, or documents with --from-cache")
    ap.add_argument("--from-cache", action="store_true", help="the inputs are documents whose extraction is cached")
    ap.add_argument("--keep", action="store_true", help="the inputs are documents to keep as they are in the graph")
    args = ap.parse_args()
    spec = str(Path(SPEC).resolve())

    nodes, edges, hyperedges = [], [], []
    if args.keep:
        docs = [str(Path(d).resolve()) for d in args.inputs]
    elif args.from_cache:
        docs = [str(Path(d).resolve()) for d in args.inputs]
        nodes, edges, hyperedges, missing = check_semantic_cache(docs, root=".", prompt_file=spec)
        if missing:
            sys.exit(f"no cached extraction for: {', '.join(relative(m) for m in missing)}")
    else:
        docs = []
        for chunk in args.inputs:
            d = json.loads(Path(chunk).read_text(encoding="utf-8"))
            nodes += d.get("nodes", [])
            edges += d.get("edges", [])
            hyperedges += d.get("hyperedges", [])
            for source, lines in (d.get("coverage") or {}).items():
                total = sum(1 for _ in open(source, errors="replace"))
                if int(lines) < total:
                    sys.exit(f"{source}: read {lines} of {total} lines; extract it again")
                docs.append(str(Path(source).resolve()))
        save_semantic_cache(nodes, edges, hyperedges, root=".", allowed_source_files=docs, prompt_file=spec)
    names = {relative(d) for d in docs}

    old = json.loads(GRAPH.read_text(encoding="utf-8"))
    links = old.get("links", old.get("edges", []))
    seen = set()
    new_nodes = []
    for n in nodes:
        if n["id"] not in seen:
            seen.add(n["id"])
            new_nodes.append({**n, "_origin": "semantic"})
    kept = 0
    for n in old["nodes"]:
        if n.get("_origin") != "ast" and n.get("source_file") in names and n["id"] not in seen:
            seen.add(n["id"])
            new_nodes.append({k: v for k, v in n.items() if k not in ("community", "community_name")})
            kept += 1
    new_edges = [{**e, "_origin": "semantic"} for e in edges]
    all_old = {n["id"] for n in old["nodes"]} | seen
    pairs = {(e["source"], e["target"], e.get("relation")) for e in edges}
    for e in links:
        s, t = e.get("_src", e.get("source")), e.get("_tgt", e.get("target"))
        if (e.get("_origin") != "ast" and e.get("source_file") in names and s in all_old and t in all_old
                and (s, t, e.get("relation")) not in pairs):
            new_edges.append({**e, "source": s, "target": t})
    for h in old.get("hyperedges", old.get("graph", {}).get("hyperedges", [])):
        if h.get("source_file") in names and all(x["id"] != h.get("id") for x in hyperedges):
            hyperedges.append(h)

    extraction = {"nodes": new_nodes, "edges": new_edges, "hyperedges": hyperedges, "input_tokens": 0, "output_tokens": 0}
    G = build_merge([extraction], graph_path=str(GRAPH), root=".", directed=False)
    previous = {n["id"]: n["community"] for n in old["nodes"] if n.get("community") is not None}
    communities = remap_communities_to_previous(cluster(G), previous)
    named = collections.defaultdict(collections.Counter)
    for n in old["nodes"]:
        if n.get("community_name"):
            named[n.get("community")][n["community_name"]] += 1
    labels = {}
    for cid, members in communities.items():
        if named[cid]:
            labels[cid] = named[cid].most_common(1)[0][0]
        else:
            labels[cid] = G.nodes[max(members, key=G.degree)].get("label", f"Community {cid}")
    # Deduplication folds an extraction's near-identical nodes into their twins, whose edges it keeps. The guard only
    # sees ids that went away, and refuses. Leaving them unfolded isn't an answer: every later `graphify update` folds
    # them and then refuses to shrink the graph, the Stop hook included. So a few folded ids are let through, and
    # anything more is a real loss and stops here.
    now = [{"id": i, **{k: v for k, v in d.items() if k != "id"}} for i, d in G.nodes(data=True)]
    gone = _lost_saved_semantic_nodes(old["nodes"], now, set(), {})
    if len(gone) > MAX_FOLDED:
        sys.exit(f"{len(gone)} earlier semantic nodes would go ({', '.join(n['id'] for n in gone[:12])}, ...): "
                 "nothing was changed")
    if not to_json(G, communities, str(GRAPH), community_labels=labels, force=bool(gone)):
        sys.exit("graphify refused to write graph.json: nothing else was changed")

    Path("graphify-out/.graphify_labels.json").write_text(
        json.dumps({str(k): v for k, v in labels.items()}, ensure_ascii=False), encoding="utf-8")
    state = detect_incremental(Path("."), extra_excludes=_read_build_excludes(out_path()),
                               gitignore=_read_build_gitignore(out_path()))
    save_manifest(state["files"], root=".", scan_corpus={f for fl in state["files"].values() for f in fl})
    subprocess.run(["graphify", "cluster-only", "."], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for stale in glob.glob("graphify-out/.graphify_chunk_*.json"):
        Path(stale).unlink()

    written = json.loads(GRAPH.read_text(encoding="utf-8"))
    check = diagnose_extraction({"nodes": written["nodes"], "edges": written.get("links", written.get("edges", []))},
                                directed=False, root=".")
    print(f"{len(names)} documents: {len(nodes)} extracted nodes, {kept} earlier nodes kept, "
          f"{len(gone)} folded into a twin: {', '.join(n['id'] for n in gone)}")
    print(f"graph: {len(written['nodes'])} nodes, {len(written.get('links', written.get('edges', [])))} links, "
          f"{len(communities)} communities")
    print({k: check.get(k) for k in ("dangling_endpoint_edges", "missing_endpoint_edges", "self_loop_edges")})


if __name__ == "__main__":
    main()
