# n0s-ryo-miner ⚡

GPU miner for [RYO Currency](https://ryo-currency.com) using the **CryptoNight-GPU** algorithm.

Fork of [xmr-stak](https://github.com/fireice-uk/xmr-stak), stripped down to the GPU paths that still matter here.

**Important:** the repo now builds a **single executable per target** with the web GUI embedded. It does **not** ship separate backend `.so` / `.dll` files anymore.

## Reality check

This is the current honest status after revalidating the repo instead of trusting stale docs:

- ✅ **Ubuntu 24.04 AMD OpenCL** native build + native benchmark verified on `nitro` (RX 9070 XT, **2457.6 H/s**)
- ✅ **Ubuntu 24.04 NVIDIA CUDA** native build + native benchmark verified on `nosnode` (RTX 2070, CUDA 13.2, **2227.2 H/s**)
- ✅ **Windows 11 NVIDIA CUDA + OpenCL** native build + benchmark + pool smoke verified on `win11` (RTX 3070, CUDA 11.0, **2742.1 H/s CUDA**, **3085.2 H/s OpenCL**)
- ✅ **Windows OpenCL cross-build** verified from Ubuntu with MinGW, compile-only
- ⚠️ **Windows AMD OpenCL** remains unvalidated (no Windows+AMD box available)
- ✅ The core miner (CUDA + OpenCL, no HTTP/TLS/hwloc) is now verified on both Linux and Windows

For the detailed matrix, read [docs/BUILD-MATRIX.md](docs/BUILD-MATRIX.md).

## Prebuilt releases

Check [GitHub Releases](https://github.com/n0sn0de/n0s-ryo-miner/releases), but use the build matrix above before assuming a platform/backend combination is fully validated.

## Validated build paths

### Ubuntu AMD / OpenCL (verified)

```bash
cmake -S . -B build-amd \
  -DCUDA_ENABLE=OFF \
  -DOpenCL_ENABLE=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DN0S_COMPILE=generic
cmake --build build-amd -j$(nproc)
```

Optional sanity run:

```bash
./build-amd/bin/n0s-ryo-miner --version
./build-amd/bin/n0s-ryo-miner --noNVIDIA --benchmark 10 --benchmark-json amd-benchmark.json
```

### Ubuntu NVIDIA / CUDA (verified on `nosnode`)

```bash
cmake -S . -B build-cuda \
  -DCUDA_ENABLE=ON \
  -DOpenCL_ENABLE=OFF \
  -DCUDA_ARCH=75 \
  -DCMAKE_BUILD_TYPE=Release \
  -DN0S_COMPILE=generic
cmake --build build-cuda -j$(nproc)
```

Optional sanity run:

```bash
./build-cuda/bin/n0s-ryo-miner --version
./build-cuda/bin/n0s-ryo-miner --noAMD --benchmark 10 --benchmark-json cuda-benchmark.json
```

### Windows OpenCL cross-build from Ubuntu (compile only)

```bash
./scripts/cross-build-windows.sh --skip-deps
```

Output lands in `dist/windows-opencl/`. This produces a Windows `.exe`, but it was **not** natively executed on Windows in this validation pass.

### Windows MSVC CUDA + OpenCL (verified on Windows 11)

**Prerequisites:**
- Visual Studio 2019 or 2022 with C++ workload (Build Tools or Community)
- CUDA Toolkit 11.0+ (if `-CudaEnable`)
- CMake 3.18+ (ships with Visual Studio)
- vcpkg (optional, for HTTP/TLS/hwloc support)

**Build:**
```powershell
.\scripts\build-windows.ps1 -CudaEnable -OpenclEnable
```

The script auto-detects your CUDA version and selects compatible GPU architectures. Without vcpkg, it builds a core miner (no HTTP API, no TLS, no hwloc) that is sufficient for mining and benchmarking.

**Verified on:** Windows 11, RTX 3070, CUDA 11.0, MSVC 2019, no vcpkg

## Container builds

Container builds are useful for compile validation and packaging, not as a replacement for real hardware checks.

```bash
# OpenCL / AMD compile path
./scripts/container-build-opencl.sh

# CUDA compile matrix
./scripts/container-build.sh 11.8
./scripts/container-build.sh 12.6
./scripts/container-build.sh 12.8

# Build all configured CUDA variants
./scripts/build-matrix.sh
```

Notes:

- `scripts/container-build-opencl.sh` now packages the current **single-binary** layout
- GUI assets are embedded into the executable during the build
- Container success only proves the code compiles in that image, not that it ran on real hardware

## Usage

```bash
./build/bin/n0s-ryo-miner -o pool:port -u wallet_address -p x
```

### Autotune (Recommended for First Run)

Find optimal GPU settings automatically before mining:

```bash
# Quick autotune — tests ~3-9 candidates, takes 2-6 minutes
./build/bin/n0s-ryo-miner --autotune

# Balanced — more candidates, better coverage (~5-10 minutes)
./build/bin/n0s-ryo-miner --autotune --autotune-mode balanced

# AMD only or NVIDIA only
./build/bin/n0s-ryo-miner --autotune --autotune-backend amd
./build/bin/n0s-ryo-miner --autotune --autotune-backend nvidia

# Re-tune (ignore cached results)
./build/bin/n0s-ryo-miner --autotune --autotune-reset
```

Autotune writes optimized settings to `amd.txt` / `nvidia.txt` and caches results in `autotune.json`. Subsequent runs reuse cached settings automatically unless hardware or drivers change.

**How it works:**
1. Discovers GPUs and collects device fingerprints
2. Generates candidate parameter sets based on GPU architecture
3. Benchmarks each candidate in an isolated subprocess (crash-safe)
4. Scores candidates by hashrate stability (penalizes variance and errors)
5. Runs extended stability validation on the winner
6. Writes optimal config files ready for mining

| Autotune Flag | Description |
|---|---|
| `--autotune` | Enable autotune mode |
| `--autotune-mode quick\|balanced\|exhaustive` | Search depth (default: `balanced`) |
| `--autotune-backend amd\|nvidia\|all` | Which GPUs to tune (default: `all`) |
| `--autotune-gpu 0,1` | Specific GPU indices to tune |
| `--autotune-reset` | Ignore cached results, re-tune from scratch |
| `--autotune-resume` | Resume an interrupted autotune run |
| `--autotune-benchmark-seconds N` | Per-candidate benchmark time (default: 30) |
| `--autotune-stability-seconds N` | Final stability validation time (default: 60) |
| `--autotune-target hashrate\|efficiency\|balanced` | Optimization target (default: `hashrate`) |
| `--autotune-export PATH` | Export results to a specific file |

### Command-Line Options

| Flag | Description |
|---|---|
| `-o pool:port` | Pool address |
| `-u wallet` | Wallet address |
| `-p password` | Pool password (usually `x`) |
| `--noAMD` | Disable AMD GPU backend |
| `--noNVIDIA` | Disable NVIDIA GPU backend |
| `--noAMDCache` | Don't cache compiled OpenCL kernels |
| `--benchmark N` | Run benchmark for block version N and exit |
| `--benchmark-json FILE` | Write benchmark results as JSON |
| `--profile` | Enable per-kernel phase timing (use with `--benchmark`) |

### Configuration Files

Generated on first run (or by `--autotune`) in the working directory:
- **`pools.txt`** — Pool connection settings
- **`amd.txt`** — AMD GPU settings (intensity, worksize per GPU)
- **`nvidia.txt`** — NVIDIA GPU settings (threads, blocks, bfactor per GPU)
- **`autotune.json`** — Cached autotune results with device fingerprints

### HTTP Monitoring API

When built with libmicrohttpd (`-DMICROHTTPD_ENABLE=ON`, the default):
```
http://localhost:<port>/api.json
```

Returns JSON with hashrate, pool stats, and GPU status. Port is configured in the miner's config.

---

## Benchmarks

Tested on 3 GPU architectures with autotuned settings:

| GPU | Architecture | VRAM | Optimal Settings | Hashrate |
|---|---|---|---|---|
| AMD RX 9070 XT | RDNA 4 (gfx1201) | 16 GB | intensity=1536, worksize=8 | **4,531 H/s** |
| NVIDIA RTX 2070 | Turing (sm_75) | 8 GB | threads=8, blocks=216 | **2,160 H/s** |
| NVIDIA GTX 1070 Ti | Pascal (sm_61) | 8 GB | threads=8, blocks=114 | **1,687 H/s** |

*Settings discovered via `--autotune --autotune-mode quick`. Your results may vary by ±5% due to thermal conditions, driver version, and system load.*

### Per-Phase Kernel Profiling

Use `--benchmark 10 --profile` to see where time is spent:

| Phase | Description | RX 9070 XT | GTX 1070 Ti | RTX 2070 |
|---|---|---|---|---|
| Phase 1 | Keccak init | <0.1% | <0.1% | <0.1% |
| Phase 2 | Scratchpad fill | 12.0% | 2.5% | 3.1% |
| **Phase 3** | **GPU compute (FP math)** | **69.5%** | **82.4%** | **85.3%** |
| Phase 4+5 | AES implode + finalize | 18.5% | 15.1% | 11.7% |

Phase 3 (cooperative floating-point computation with data-dependent memory access) dominates on all architectures. The algorithm is designed to be GPU-friendly but resistant to algorithmic shortcuts.

---

## Tested Platforms

Real-world build validations on specific hardware/OS combinations:

### Ubuntu 24.04 + CUDA 13.2 + RTX 2070 (April 2026)

| Component | Version |
|---|---|
| **OS** | Ubuntu 24.04.4 LTS (Noble Numbat) |
| **Kernel** | 6.17.0-20-generic |
| **GPU** | NVIDIA GeForce RTX 2070 (8 GB, Turing sm_75) |
| **CUDA Toolkit** | 13.2.0 (nvcc V13.2.51) |
| **NVIDIA Driver** | 580.126.09 (reports CUDA runtime 13.0) |
| **GCC** | 13.3.0 |
| **CMake** | 3.28.3 |

**Build commands (exact):**

```bash
# Ensure CUDA is in PATH
export PATH=/usr/local/cuda/bin:$PATH
export LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH

# Install dependencies (if not already present)
sudo apt-get install -y build-essential cmake git libmicrohttpd-dev libssl-dev libhwloc-dev

# Clone and build
git clone https://github.com/n0sn0de/n0s-ryo-miner.git
cd n0s-ryo-miner
mkdir build && cd build

# CUDA 13.x REQUIRES the -static-global-template-stub=false flag
cmake .. -DOpenCL_ENABLE=OFF -DCUDA_ENABLE=ON \
    -DCUDA_ARCH="75" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCUDA_NVCC_FLAGS="-static-global-template-stub=false"

cmake --build . -j$(nproc)
```

**Validation results:**
- `--version`: `n0s-ryo-miner 3.4.0 d673c6e` ✅
- Golden hash test: 3/3 passed ✅
- Benchmark: **2,150 H/s** (RTX 2070, threads=8, blocks=216) ✅
- CUDA detection: `CUDA [13.0/13.2] GPU#0, device architecture 75: "NVIDIA GeForce RTX 2070"` ✅

> **Note:** The driver reports CUDA 13.0 while the toolkit is 13.2. This triggers a "minor version forward compat mode" message — this is normal and everything works correctly.

#### CUDA 13.x Notes

CUDA 13.x introduced a breaking change: the default for `-static-global-template-stub` changed to `true` in whole-program compilation mode (`-rdc=false`). This causes linker errors for `__global__` function templates that are declared in headers but defined in separate `.cu` files:

```
undefined reference to `kernel_implode_scratchpad<...>'
undefined reference to `cryptonight_extra_gpu_final<...>'
```

**Fix:** Pass `-static-global-template-stub=false` to nvcc via:

```bash
cmake .. -DCUDA_NVCC_FLAGS="-static-global-template-stub=false" [other flags...]
```

This restores the CUDA 12.x behavior where template stub symbols remain visible across translation units. CUDA 11.x and 12.x do **not** need this flag.

---

## Testing

### Unit Tests
```bash
bash tests/build_harness.sh && ./tests/cn_gpu_harness   # Golden hash verification
bash tests/build_autotune_test.sh                       # Autotune framework (21 tests)
```

### Local AMD Test
```bash
./test-mine.sh                         # Build + mine for 30s on local AMD GPU
```

### Remote NVIDIA Test
```bash
REMOTE=nosnode ./test-mine-remote.sh   # Build on remote NVIDIA host, mine or benchmark
```

### Triple-GPU Integration Test
```bash
./test-both.sh                         # AMD + NVIDIA Pascal + NVIDIA Turing
```

### Containerized Compile Matrix
```bash
./scripts/matrix-test.sh               # 10 platform combos, compile-only
./scripts/matrix-test.sh --test        # Compile + hardware mine tests
```

---

## Project Structure

```
├── CMakeLists.txt              # Build system (C++17, CUDA + OpenCL)
├── scripts/
│   ├── build-windows.ps1       # Windows MSVC build helper
│   ├── cross-build-windows.sh  # MinGW cross-compile (Linux → Windows)
│   ├── container-build.sh      # Containerized CUDA build (podman)
│   ├── container-build-opencl.sh # Containerized OpenCL build
│   ├── build-matrix.sh         # Build all CUDA versions + optional test
│   ├── matrix-test.sh          # Full 10-platform compile validation
│   ├── test-remote-binary.sh   # Deploy + mine-test on remote GPU host
│   └── test-nosnode.sh         # nosnode-specific test script
├── cmake/
│   └── mingw-w64-x86_64.cmake # MinGW cross-compile toolchain file
├── n0s/
│   ├── cli/                    # CLI entry point
│   ├── autotune/               # GPU autotuning framework
│   ├── backend/
│   │   ├── amd/                # AMD OpenCL backend
│   │   │   └── amd_gpu/opencl/ # OpenCL kernels (.cl)
│   │   ├── nvidia/             # NVIDIA CUDA backend
│   │   │   └── nvcc_code/      # CUDA kernels (.cu, .hpp)
│   │   └── cpu/                # Shared crypto library (hash verification)
│   ├── net/                    # Pool connection (Stratum)
│   ├── http/                   # HTTP monitoring API
│   └── misc/                   # Utilities, logging, config
├── docs/
│   └── CN-GPU-WHITEPAPER.md    # CryptoNight-GPU algorithm deep dive
├── test-mine.sh                # Local AMD mining test
├── test-mine-remote.sh         # Remote NVIDIA mining test
└── test-both.sh                # Triple-GPU integration test
```

---

## Algorithm

This miner implements **CryptoNight-GPU** (`cn/gpu`), a proof-of-work algorithm designed specifically for GPU mining. It uses:
- Keccak-1600 for state initialization
- A 2 MiB scratchpad filled via Keccak permutations
- A GPU-optimized main loop with 16-thread cooperative groups performing floating-point math, creating a workload that favors massively parallel GPU architectures
- AES + mix-and-propagate finalization
- Final Keccak hash for proof verification

See [`docs/CN-GPU-WHITEPAPER.md`](docs/CN-GPU-WHITEPAPER.md) for the complete algorithm specification.

---

## License

GPLv3 — see [LICENSE](LICENSE).

## Credits

- **fireice-uk** and **psychocrypt** — original xmr-stak
- **RYO Currency team** — CryptoNight-GPU algorithm design
- **n0sn0de** — modernization, cleanup, and ongoing development
