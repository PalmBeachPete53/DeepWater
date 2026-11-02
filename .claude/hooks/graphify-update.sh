#!/bin/bash
# Claude Code Stop hook: keep graphify-out/ current without being asked. Runs `graphify update .`
# (AST only, no LLM, a few seconds) when a source or doc file changed since the last run; a stamp
# file remembers that run, a lock keeps two runs from overlapping. Silent and never blocking.
cd "${CLAUDE_PROJECT_DIR:-.}" || exit 0
[ -f graphify-out/graph.json ] || exit 0
stamp=graphify-out/.hook-stamp
[ -f "$stamp" ] || : > "$stamp"
changed=$(find src tests README.md CLAUDE.md CMakeLists.txt -type f -newer "$stamp" 2>/dev/null | head -1)
[ -n "$changed" ] || exit 0
mkdir graphify-out/.update.lock 2>/dev/null || exit 0
trap 'rmdir graphify-out/.update.lock 2>/dev/null' EXIT
: > "$stamp"
graphify update . > graphify-out/.last-update.log 2>&1
exit 0
