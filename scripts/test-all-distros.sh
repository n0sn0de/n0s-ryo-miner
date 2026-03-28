#!/usr/bin/env bash
# test-all-distros.sh — Build and smoke-test n0s-cngpu across Ubuntu LTS versions
# Uses podman (or docker as fallback) to build Containerfiles
#
# Usage:
#   ./scripts/test-all-distros.sh [OPTIONS]
#
# Options:
#   --cpu-only    Test CPU-only builds (default if no option given)
#   --opencl      Test OpenCL builds on supported distros
#   --all         Run all test variants
#   --parallel    Run builds in parallel (faster, noisier output)
#   --distro X    Test only distro X (bionic|focal|jammy|noble)
#   --clean       Remove test images after run
#   -h, --help    Show this help

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CONTAINERS_DIR="$PROJECT_ROOT/containers"

# Distros and variants
CPU_DISTROS=(bionic focal jammy noble)
OPENCL_DISTROS=(jammy noble)  # Only distros with OpenCL Containerfiles

# Defaults
MODE="cpu-only"
PARALLEL=false
FILTER_DISTRO=""
CLEAN=false

# Parse args (before runtime detection so --help works without podman/docker)
while [[ $# -gt 0 ]]; do
    case "$1" in
        --cpu-only) MODE="cpu-only"; shift ;;
        --opencl)   MODE="opencl";   shift ;;
        --all)      MODE="all";      shift ;;
        --parallel) PARALLEL=true;   shift ;;
        --distro)   FILTER_DISTRO="$2"; shift 2 ;;
        --clean)    CLEAN=true;      shift ;;
        -h|--help)
            head -n 16 "$0" | tail -n 14
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

# Detect container runtime
if command -v podman &>/dev/null; then
    RUNTIME="podman"
elif command -v docker &>/dev/null; then
    RUNTIME="docker"
else
    echo "ERROR: Neither podman nor docker found. Install one to run tests." >&2
    exit 1
fi

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# Results tracking
declare -A RESULTS
PASS=0
FAIL=0

build_and_test() {
    local distro="$1"
    local variant="${2:-cpu}"  # cpu or opencl
    local suffix=""
    local containerfile=""

    if [[ "$variant" == "opencl" ]]; then
        suffix="-opencl"
        containerfile="$CONTAINERS_DIR/Containerfile.${distro}-opencl"
    else
        containerfile="$CONTAINERS_DIR/Containerfile.${distro}"
    fi

    local image_name="n0s-cngpu-test:${distro}${suffix}"
    local test_key="${distro}${suffix}"

    if [[ ! -f "$containerfile" ]]; then
        echo -e "${YELLOW}SKIP${NC}  ${test_key} — Containerfile not found: $(basename "$containerfile")"
        RESULTS[$test_key]="SKIP"
        return 0
    fi

    echo -e "${CYAN}BUILD${NC} ${test_key} — $(basename "$containerfile")"

    local start_time
    start_time=$(date +%s)

    if $RUNTIME build \
        -t "$image_name" \
        -f "$containerfile" \
        "$PROJECT_ROOT" 2>&1 | tail -n 5; then

        local end_time
        end_time=$(date +%s)
        local elapsed=$((end_time - start_time))

        echo -e "${GREEN}PASS${NC}  ${test_key} (${elapsed}s)"
        RESULTS[$test_key]="PASS (${elapsed}s)"
        ((PASS++))
    else
        local end_time
        end_time=$(date +%s)
        local elapsed=$((end_time - start_time))

        echo -e "${RED}FAIL${NC}  ${test_key} (${elapsed}s)"
        RESULTS[$test_key]="FAIL (${elapsed}s)"
        ((FAIL++))
    fi

    # Clean up image if requested
    if $CLEAN; then
        $RUNTIME rmi "$image_name" &>/dev/null || true
    fi
}

run_tests() {
    local distros=()
    local variant="$1"

    case "$variant" in
        cpu)    distros=("${CPU_DISTROS[@]}") ;;
        opencl) distros=("${OPENCL_DISTROS[@]}") ;;
    esac

    # Apply distro filter
    if [[ -n "$FILTER_DISTRO" ]]; then
        distros=("$FILTER_DISTRO")
    fi

    if $PARALLEL; then
        local pids=()
        for distro in "${distros[@]}"; do
            build_and_test "$distro" "$variant" &
            pids+=($!)
        done
        for pid in "${pids[@]}"; do
            wait "$pid" || true
        done
    else
        for distro in "${distros[@]}"; do
            build_and_test "$distro" "$variant"
        done
    fi
}

# Header
echo ""
echo "╔════════════════════════════════════════════════════╗"
echo "║        n0s-cngpu Multi-Distro Build Test          ║"
echo "╠════════════════════════════════════════════════════╣"
echo "║  Runtime: $(printf '%-40s' "$RUNTIME ($($RUNTIME --version 2>&1 | head -1))")║"
echo "║  Mode:    $(printf '%-40s' "$MODE")║"
echo "║  Source:  $(printf '%-40s' "$PROJECT_ROOT")║"
echo "╚════════════════════════════════════════════════════╝"
echo ""

# Run tests based on mode
case "$MODE" in
    cpu-only)
        run_tests "cpu"
        ;;
    opencl)
        run_tests "opencl"
        ;;
    all)
        run_tests "cpu"
        run_tests "opencl"
        ;;
esac

# Summary
echo ""
echo "════════════════════════════════════════════════════"
echo "  RESULTS SUMMARY"
echo "════════════════════════════════════════════════════"
for key in $(echo "${!RESULTS[@]}" | tr ' ' '\n' | sort); do
    result="${RESULTS[$key]}"
    color="$NC"
    case "$result" in
        PASS*) color="$GREEN" ;;
        FAIL*) color="$RED" ;;
        SKIP*) color="$YELLOW" ;;
    esac
    printf "  %-20s %b%s%b\n" "$key" "$color" "$result" "$NC"
done
echo "════════════════════════════════════════════════════"
echo -e "  Total: $((PASS + FAIL)) tested, ${GREEN}${PASS} passed${NC}, ${RED}${FAIL} failed${NC}"
echo "════════════════════════════════════════════════════"
echo ""

# Exit non-zero if any failures
[[ $FAIL -eq 0 ]]
