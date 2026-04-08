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
| Windows x64 | NVIDIA CUDA + OpenCL | MSVC / GitHub Actions config present | Not natively validated in this pass | ⚠️ unvalidated runtime | `build-windows.ps1` and CI workflow exist, but `win11` access was blocked by SSH auth |
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
| `win11` | Windows 11 | NVIDIA GPU | Intended native validation target | Blocked this pass, SSH auth failed |

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

## Highest-leverage next step

Regain native access to `win11`, run `scripts/build-windows.ps1 -CudaEnable -OpenclEnable`, then do at least:

1. `n0s-ryo-miner.exe --version`
2. a short GPU benchmark run
3. if possible, a short pool smoke test

That is the blocker between today's honest matrix and a truthful Windows release story.
