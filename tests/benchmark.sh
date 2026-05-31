#!/bin/bash
# Benchmark harness for n0s-ryo-miner
# Usage: ./tests/benchmark.sh [--wait SEC] [--work SEC] [--json FILE] [--compare FILE]
#
# Runs the built-in benchmark, captures results, and optionally compares against
# a previous run for A/B performance testing.

set -e

BINARY="$(dirname "$0")/../build/bin/n0s-ryo-miner"
BLOCK_VERSION=12  # CryptoNight-GPU block version
WAIT_SEC=15
WORK_SEC=60
JSON_FILE=""
COMPARE_FILE=""

usage() {
    echo "Usage: $0 [options]"
    echo "  --wait SEC       Warm-up time (default: $WAIT_SEC)"
    echo "  --work SEC       Benchmark duration (default: $WORK_SEC)"
    echo "  --json FILE      Write JSON results to FILE"
    echo "  --compare FILE   Compare against previous JSON results"
    echo "  --quick          Quick run (wait=10, work=30)"
    echo "  --long           Long run (wait=20, work=120)"
    echo "  -h, --help       Show this help"
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --wait) WAIT_SEC="$2"; shift 2 ;;
        --work) WORK_SEC="$2"; shift 2 ;;
        --json) JSON_FILE="$2"; shift 2 ;;
        --compare) COMPARE_FILE="$2"; shift 2 ;;
        --quick) WAIT_SEC=10; WORK_SEC=30; shift ;;
        --long) WAIT_SEC=20; WORK_SEC=120; shift ;;
        -h|--help) usage ;;
        *) echo "Unknown option: $1"; usage ;;
    esac
done

if [ ! -x "$BINARY" ]; then
    echo "ERROR: Binary not found at $BINARY"
    echo "Build first: cd build && cmake --build . -j\$(nproc)"
    exit 1
fi

echo "=== n0s-ryo-miner Benchmark ==="
echo "Binary:    $BINARY"
echo "Block ver: $BLOCK_VERSION"
echo "Warm-up:   ${WAIT_SEC}s"
echo "Work:      ${WORK_SEC}s"
echo ""

# Build benchmark args
ARGS="--benchmark $BLOCK_VERSION --benchwait $WAIT_SEC --benchwork $WORK_SEC"

if [ -n "$JSON_FILE" ]; then
    ARGS="$ARGS --benchmark-json $JSON_FILE"
fi

# Run benchmark
$BINARY $ARGS 2>&1 | tee /tmp/n0s-benchmark-$$.log
EXIT_CODE=${PIPESTATUS[0]}

if [ $EXIT_CODE -ne 0 ]; then
    echo ""
    echo "FAIL: Benchmark exited with code $EXIT_CODE"
    exit $EXIT_CODE
fi

# Compare against previous results if requested
if [ -n "$COMPARE_FILE" ] && [ -n "$JSON_FILE" ] && [ -f "$COMPARE_FILE" ] && [ -f "$JSON_FILE" ]; then
    echo ""
    echo "=== A/B Comparison ==="

    # Extract total_hps from both files
    OLD_HPS=$(python3 -c "import json; print(json.load(open('$COMPARE_FILE'))['benchmark']['total_hps'])" 2>/dev/null || echo "N/A")
    NEW_HPS=$(python3 -c "import json; print(json.load(open('$JSON_FILE'))['benchmark']['total_hps'])" 2>/dev/null || echo "N/A")

    if [ "$OLD_HPS" != "N/A" ] && [ "$NEW_HPS" != "N/A" ]; then
        CHANGE=$(python3 -c "
old, new = $OLD_HPS, $NEW_HPS
diff = ((new - old) / old) * 100
sign = '+' if diff >= 0 else ''
print(f'Old: {old:.1f} H/s | New: {new:.1f} H/s | Change: {sign}{diff:.1f}%')
")
        echo "$CHANGE"
    else
        echo "Could not parse results for comparison"
    fi
fi

rm -f /tmp/n0s-benchmark-$$.log
echo ""
echo "=== Benchmark Complete ==="
