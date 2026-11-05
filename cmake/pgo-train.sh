#!/bin/zsh
# Profile-guided optimisation, step 2 of 3 (README › Build options):
#   1. cmake -S . -B build-pgo-gen -DCMAKE_BUILD_TYPE=Release -DDEEPWATER_PGO=GENERATE [-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" ...]
#      cmake --build build-pgo-gen -j 10
#   2. cmake/pgo-train.sh build-pgo-gen [profile-dir]      # default profile-dir: build-pgo-profiles
#   3. cmake -S . -B build -DDEEPWATER_PGO=USE -DDEEPWATER_PGO_DIR=<profile-dir> && cmake --build build -j 10
# Runs the instrumented test programs (indicators, backtest, Markov, footprint, Big Trades, heatmap, chart rendering)
# on every architecture of the build, each under `arch -<arch>`, and merges one <arch>.profdata per slice. The x86_64
# slice runs under Rosetta on an Apple silicon Mac (softwareupdate --install-rosetta), natively on an Intel one.
set -e
build=${1:?usage: cmake/pgo-train.sh <instrumented build dir> [profile dir]}
out=${2:-${0:A:h:h}/build-pgo-profiles}
mkdir -p $out
export QT_QPA_PLATFORM=offscreen
t=$build/tests-quant
workload=(
    "$t/core/qtm_test_core"
    "$t/backtest/qtm_test_backtest"
    "$t/markov/qtm_test_markov"
    "$t/footprint/qtm_test_footprint"
    "$t/bigtrades/qtm_test_bigtrades"
    "$t/heatmap/qtm_test_heatmap load"
    "$t/chart/qtm_chart_demo --bench"
)
for a in $(lipo -archs $t/core/qtm_test_core); do
    raw=$out/raw-$a
    rm -rf $raw && mkdir -p $raw
    for cmd in $workload; do
        echo "pgo: $a ${cmd##*/}"
        LLVM_PROFILE_FILE="$raw/%p-%m.profraw" arch -$a ${=cmd} > /dev/null
    done
    xcrun llvm-profdata merge -o $out/$a.profdata $raw/*.profraw
    rm -rf $raw
    echo "pgo: $out/$a.profdata"
done
