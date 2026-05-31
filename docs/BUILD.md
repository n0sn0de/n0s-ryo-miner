# Build Guide

## Requirements

Common tools:

- CMake 3.18+
- A C++17 compiler
- Git

Optional GPU stacks:

- AMD OpenCL runtime and headers
- NVIDIA CUDA Toolkit
- Visual Studio Build Tools on Windows
- vcpkg for optional HTTP, TLS, and hwloc dependencies

## Linux AMD OpenCL

```bash
cmake -S . -B build-opencl \
  -DCUDA_ENABLE=OFF \
  -DOpenCL_ENABLE=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DN0S_COMPILE=generic

cmake --build build-opencl -j"$(nproc)"
./build-opencl/bin/n0s-ryo-miner --version
```

Benchmark:

```bash
./build-opencl/bin/n0s-ryo-miner --noNVIDIA --benchmark 10 --benchmark-json opencl-benchmark.json
```

## Linux NVIDIA CUDA

Set `CUDA_ARCH` to match the target GPU. Examples: Pascal `61`, Turing `75`, Ampere `86`, Ada `89`.

```bash
cmake -S . -B build-cuda \
  -DCUDA_ENABLE=ON \
  -DOpenCL_ENABLE=OFF \
  -DCUDA_ARCH=75 \
  -DCMAKE_BUILD_TYPE=Release \
  -DN0S_COMPILE=generic

cmake --build build-cuda -j"$(nproc)"
./build-cuda/bin/n0s-ryo-miner --version
```

Benchmark:

```bash
./build-cuda/bin/n0s-ryo-miner --noAMD --benchmark 10 --benchmark-json cuda-benchmark.json
```

## Windows Native Build

Open a Visual Studio Developer PowerShell and run:

```powershell
.\scripts\build-windows.ps1 -CudaEnable -OpenclEnable
```

Without vcpkg, the build still produces a core miner suitable for mining and benchmarking, but optional HTTP, TLS, and hwloc features may be disabled.

## Container Compile Checks

Container builds are useful for packaging and compile validation. They do not prove runtime GPU correctness.

```bash
./scripts/container-build-opencl.sh
./scripts/container-build.sh 11.8
./scripts/container-build.sh 12.6
./scripts/container-build.sh 12.8
./scripts/build-matrix.sh
```

## Generated Assets

The web UI is embedded at build time into `n0s/http/embedded_assets.hpp`. If GUI files change, regenerate the header with:

```bash
./scripts/embed_assets.sh
```
