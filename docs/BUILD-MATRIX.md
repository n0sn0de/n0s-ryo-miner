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
- The web GUI is **embedded into the executable** on the validated Linux builds and on the Windows MinGW cross-build.
- It is **not honest yet** to publish a single generic `n0s-ryo-miner-linux` / `n0s-ryo-miner-win` pair and imply the whole matrix is equally real.
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

## Recommended release posture right now

### Safe to claim

- Ubuntu AMD OpenCL: verified
- Ubuntu NVIDIA CUDA: verified
- Windows OpenCL: compile-only
- Windows NVIDIA CUDA: prepared, but still needs native validation on `win11`

### Not safe to claim yet

- Windows AMD OpenCL support
- A fully verified single-binary Windows release covering both CUDA and OpenCL
- A generic `n0s-ryo-miner-win` release alias that implies equivalent validation to Linux

## What is now verified on Windows

- **Native Windows build with MSVC 2019** works without vcpkg (core miner with HTTP/TLS/hwloc disabled)
- **CUDA 11.0 + OpenCL** build succeeds on Windows 11
- **RTX 3070 CUDA benchmark**: 2742.1 H/s (15-second test)
- **RTX 3070 OpenCL benchmark**: 3085.2 H/s (15-second test, using NVIDIA's OpenCL runtime)
- **Pool smoke test**: Successfully connected to pool, submitted 13 accepted shares in 20 seconds
- **Build script** (`scripts/build-windows.ps1`) now auto-detects CUDA version and works on stock Windows PowerShell 5.1

## Remaining Windows work

- Windows AMD OpenCL: still unvalidated (no Windows+AMD box available)
- vcpkg-based builds (with full HTTP/TLS/hwloc): build script supports it, but not tested in this pass
- Release artifact automation: Windows binaries can now be built and validated, but release workflow may need adjustment
