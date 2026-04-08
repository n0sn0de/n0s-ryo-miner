#!/bin/bash
# Test the currently available GPU paths: local AMD + one remote NVIDIA host.
set -e

REMOTE="${REMOTE:-nosnode}"

PASS=0
FAIL=0

echo "====================================="
echo "n0s-ryo-miner Available Backend Test"
echo "Local AMD host: this machine"
echo "Remote NVIDIA host: ${REMOTE}"
echo "====================================="

echo ""
echo "[1/2] Local AMD OpenCL smoke test"
if ./test-mine.sh; then
  PASS=$((PASS + 1))
else
  FAIL=$((FAIL + 1))
fi

echo ""
echo "[2/2] Remote NVIDIA smoke test (${REMOTE})"
if REMOTE="${REMOTE}" ./test-mine-remote.sh; then
  PASS=$((PASS + 1))
else
  FAIL=$((FAIL + 1))
fi

echo ""
echo "====================================="
echo "Results: ${PASS} passed, ${FAIL} failed"
echo "====================================="

[ ${FAIL} -eq 0 ]
