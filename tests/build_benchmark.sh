#!/bin/bash
# Build the benchmark harness with OpenCL support
set -e

cd "$(dirname "$0")/.."

echo "======================================="
echo "Building Benchmark Harness"
echo "======================================="

# Check for OpenCL
if [ ! -f "/usr/lib/x86_64-linux-gnu/libOpenCL.so" ]; then
    echo "❌ ERROR: OpenCL library not found"
    echo "   Install: apt-get install ocl-icd-opencl-dev"
    exit 1
fi

# Build OpenCL version
echo "Building OpenCL benchmark..."
g++ -std=c++17 -O2 -march=native \
    -I. \
    tests/benchmark_harness.cpp \
    -o tests/benchmark_harness \
    -lOpenCL -lpthread

if [ $? -eq 0 ]; then
    echo "✅ Build successful: tests/benchmark_harness"
    echo ""
    echo "Usage:"
    echo "  ./tests/benchmark_harness --opencl --device 0 --duration 60"
    echo "  ./tests/benchmark_harness --opencl --json results.json"
    echo ""
else
    echo "❌ Build failed"
    exit 1
fi

# Check for CUDA (optional)
if command -v nvcc &> /dev/null; then
    echo ""
    echo "Building CUDA benchmark..."
    nvcc -std=c++17 -O2 \
        -I. \
        tests/benchmark_harness.cpp \
        -o tests/benchmark_harness_cuda \
        -lOpenCL -lpthread
    
    if [ $? -eq 0 ]; then
        echo "✅ CUDA build successful: tests/benchmark_harness_cuda"
    else
        echo "⚠️  CUDA build failed (continuing with OpenCL-only)"
    fi
else
    echo ""
    echo "ℹ️  CUDA not available (nvcc not found)"
    echo "   OpenCL-only benchmark built"
fi

echo ""
echo "======================================="
echo "Benchmark harness ready!"
echo "======================================="
