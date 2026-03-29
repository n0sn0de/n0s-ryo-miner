#!/bin/bash
# Test both AMD (local) and NVIDIA (remote) mining
set -e

echo "======================================="
echo "Dual GPU Mining Test"
echo "AMD (nitro): RX 9070 XT"
echo "NVIDIA (nos2): GTX 1070 Ti"
echo "======================================="
echo ""

# Test AMD first
echo "Testing AMD GPU (local)..."
if ! ./test-mine.sh; then
    echo "❌ AMD MINING FAILED"
    exit 1
fi

echo ""
echo "Testing NVIDIA GPU (remote)..."
if ! ./test-mine-remote.sh; then
    echo "❌ NVIDIA MINING FAILED"
    exit 1
fi

echo ""
echo "======================================="
echo "✅ BOTH GPUS WORKING"
echo "AMD + NVIDIA mining verified"
echo "======================================="
