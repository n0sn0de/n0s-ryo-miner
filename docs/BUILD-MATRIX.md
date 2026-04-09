# Build Matrix

Truthful build and validation status for `n0s-ryo-miner`.

Last updated: 2026-04-08

## What is actually verified

| Platform | Backend | Build | Run / benchmark | Status | Notes |
|---|---|---|---|---|---|
| Ubuntu 24.04 (`nitro`) | AMD OpenCL | Native CMake | Native benchmark | ✅ verified | RX 9070 XT, OpenCL runtime detected via `clinfo`, benchmark JSON recorded at **2457.6 H/s** |
| Ubuntu 24.04 (`nosnode`) | NVIDIA CUDA | Native CMake | Native benchmark | ✅ verified | RTX 2070, CUDA 13.2 toolchain, benchmark JSON recorded at **2227.2 H/s** |
| Ubuntu 22.04 container | AMD OpenCL | `scripts/container-build-opencl.sh` | Not hardware-run in container | ✅ compile-verified | Produces a single binary plus sample config files |
| Windows x64 | OpenCL | MinGW cross-build from Ubuntu | Not run on Windows in this pass | ⚠️ compile only | `scripts/cross-build-windows.sh --skip-deps` produces `dist/windows-opencl/n0s-ryo-miner.exe` |
| Windows 11 (`win11`) | NVIDIA CUDA + OpenCL | MSVC 2019 + NMake | Native build + benchmark + pool smoke | ✅ verified | RTX 3070, CUDA 11.0 toolchain, CUDA benchmark **2742.1 H/s**, OpenCL benchmark **3085.2 H/s**, pool smoke test passed |
| Windows x64 | AMD OpenCL | — | — | ❌ unvalidated | No Windows+AMD box available right now |

## Key reality checks

- The repo now builds a **single executable per target**. It does **not** ship separate backend `.so` / `.dll` files anymore.
- The web GUI assets are embedded at build time. The Linux release build enables the HTTP/TLS path, while the currently trusted Windows release path is still the simpler native core build.
- It is now honest to ship one primary `n0s-ryo-miner-linux` asset and one primary `n0s-ryo-miner-win.exe` asset, as long as the docs keep the validation caveats explicit.
- Linux NVIDIA support is real, but the important fix here was for **CUDA 13.x native builds**. Older docs overstated what had actually been rechecked.

## Verified hosts

| Host | OS | GPU | Toolchain / runtime | Result |
|---|---|---|---|---|
| `nitro` | Ubuntu 24.04.4 | AMD Radeon RX 9070 XT | OpenCL 2.0 / AMD APP 3581.0 | Native build + benchmark verified |
| `nosnode` | Ubuntu 24.04 | NVIDIA RTX 2070 (`sm_75`) | CUDA 13.2 / driver 580.126.09 | Native build + benchmark verified |
| `win11` | Windows 11 | NVIDIA RTX 3070 (`sm_86`) | CUDA 11.0 / driver 586.09 | Native build + benchmark + pool smoke verified |

## What changed to make the matrix more honest

- Fixed native Linux CUDA builds on modern NVCC by moving templated kernel launches behind translation-unit-local wrappers, which avoids CUDA 13.x link failures.
- Fixed the startup banner so it reports the real version instead of a stale hard-coded one.
- Stopped CMake source-archive builds from spewing Git errors when `.git/` is absent.
- Pinned OpenCL compilation to the 1.2 API target that the code actually uses.
- Fixed the OpenCL container builder to install `xxd` and package the current single-binary layout.
- Fixed remote CUDA test tooling to stop expecting a non-existent `libn0s_cuda_backend.so`.

## Current shipped release assets

| Asset | Built from | Honest claim | Caveats |
|---|---|---|---|
| `n0s-ryo-miner-linux` | GitHub Actions Linux release job (`ubuntu-24.04` + CUDA 12.8 container, CUDA + OpenCL enabled) | Primary Linux release binary for the validated Linux AMD OpenCL and Linux NVIDIA CUDA code paths | Built in CI, not re-benchmarked as one combined release artifact on a single host in this pass |
| `n0s-ryo-miner-win.exe` | GitHub Actions native Windows release job (`windows-2022`, MSVC, CUDA + OpenCL enabled) | Primary Windows release binary for the validated Windows 11 NVIDIA path | Windows AMD OpenCL remains unvalidated, and the current release build keeps HTTP/hwloc off |

## What CI still verifies

| Workflow | Purpose |
|---|---|
| `Linux OpenCL (native sanity)` | Fast Linux OpenCL compile and single-binary sanity check |
| `Linux CUDA 11.8 (compile coverage)` | Keeps older Linux NVIDIA toolchain coverage in CI without turning it into a release asset |
| `Linux CUDA 12.8 (compile coverage)` | Keeps current Linux NVIDIA toolchain coverage in CI and matches the Linux release base |
| `Windows x64 native CUDA + OpenCL (coverage)` | Native Windows compile coverage on the MSVC path we actually trust |
| Tag release workflow | Rebuilds only `n0s-ryo-miner-linux`, `n0s-ryo-miner-win.exe`, and `SHA256SUMS` |

## What is now verified on Windows

- **Native Windows build with MSVC 2019** works without vcpkg (core miner with HTTP/TLS/hwloc disabled)
- **CUDA 11.0 + OpenCL** build succeeds on Windows 11
- **RTX 3070 CUDA benchmark**: 2742.1 H/s (15-second test)
- **RTX 3070 OpenCL benchmark**: 3085.2 H/s (15-second test, using NVIDIA's OpenCL runtime)
- **Pool smoke test**: Successfully connected to pool, submitted 13 accepted shares in 20 seconds
- **Build script** (`scripts/build-windows.ps1`) now auto-detects CUDA version and works on stock Windows PowerShell 5.1

## Remaining Windows work

- Windows AMD OpenCL: still unvalidated (no Windows+AMD box available)
- vcpkg-based builds with full HTTP/TLS/hwloc need a fresh native validation pass before we claim parity with the Linux release binary
- If Windows AMD hardware becomes available, re-run the native MSVC release build and benchmark both CUDA-disabled and OpenCL-only paths before broadening release claims
