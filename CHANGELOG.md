# Changelog

## v3.4.1 — Honest Release Assets, Windows Validation & Runtime Polish (2026-04-14)

### Release workflow and packaging

- **Tagged releases now ship honest archive names and checksums** instead of ambiguous raw binaries
- **Published asset set is now explicit:**
  - `n0s-ryo-miner-linux-opencl.tar.gz`
  - `n0s-ryo-miner-linux-cuda12-opencl.tar.gz`
  - `n0s-ryo-miner-windows-cuda12-opencl.zip`
  - `n0s-ryo-miner-windows-opencl-cross.zip`
  - `SHA256SUMS`
- **Release workflow hardened** so tagged publishing matches the artifact set we can actually explain and defend

### Windows validation and build hardening

- **Native Windows 11 NVIDIA validation is now real, not assumed**
  - MSVC 2019 build verified on RTX 3070
  - CUDA benchmark: **2742.1 H/s**
  - OpenCL benchmark on NVIDIA runtime: **3085.2 H/s**
  - Pool smoke test passed with accepted shares
- **`scripts/build-windows.ps1` now works on stock Windows setups**
  - Removed assumptions around Ninja, PowerShell 7, and mandatory vcpkg
  - Uses the MSVC / NMake path directly
  - Adds CUDA arch fallback logic for older CUDA 11.0 installs
- **Benchmark and autotune flows no longer trip over a missing `pools.txt`**

### Runtime UX and autotune reliability

- **Console capability detection** now avoids Windows mojibake and strips ANSI escapes on consoles that cannot render them cleanly
- **Huge-page / memory-lock fallback is now honest**
  - Non-fatal cases emit a single `MEMORY NOTICE`
  - Real allocation failures still report `MEMORY ALLOC FAILED`
- **Autotune persistence reload is hardened** so restored state cleanly replaces stale in-memory entries, with unit coverage tightened

### Known caveats

- **Windows AMD OpenCL remains unvalidated**
- **The full Windows vcpkg + HTTP/TLS/hwloc path still needs a fresh native validation pass**
- **The Windows OpenCL cross-build archive is a convenience compile artifact, not a native Windows AMD validation claim**

---

## v3.4.0 — Windows Support (2026-04-03)

### Windows release foundation

- **First tagged Windows-capable release of `n0s-ryo-miner`**
- **MinGW cross-build path established** for Windows OpenCL packaging from Ubuntu
- **MSVC compatibility work landed across the codebase** so native Windows builds became realistic instead of aspirational
- **GitHub Actions Linux + Windows release automation** was introduced as the release foundation

---

## v3.3.0 — CI/CD, NVML Telemetry & Windows Prep (2026-04-03)

### CI/CD Pipeline

- **GitHub Actions CI** — Automated build & test on every push/PR to master
  - 3 parallel jobs: Linux OpenCL (Ubuntu 24.04), CUDA 11.8 (container), CUDA 12.8+OpenCL (container)
  - Golden hash constant verification on every build
  - Single-binary artifact verification (no .so files produced)
  - Concurrency groups with `cancel-in-progress` to prevent stacking builds
- **Automated Release workflow** — Tag `v*` triggers full build matrix → GitHub Release with SHA256 checksums
  - 3 Linux variants: OpenCL-only, CUDA 11.8, CUDA 12.8+OpenCL
  - Auto-generated release notes

### API Authentication

- **Bearer token authentication** for REST API endpoints
  - New `http_api_token` config key (optional)
  - Dual auth: Bearer token OR digest auth (http_login/http_pass) — either accepted
  - Backward compatible: old configs without token parse fine (open access when unconfigured)

### NVML Direct Telemetry

- **Replaced nvidia-smi subprocess with direct NVML API calls**
  - Runtime-loaded NVML via dlopen (8 function pointers, lazy init)
  - ~50-100ms per query → <1ms — eliminates ~25-50 subprocess spawns/min
  - Graceful fallback to nvidia-smi if NVML unavailable
  - Clean shutdown (nvmlShutdown + socket cleanup on exit)

### Windows Preparation (Pillar 3)

- **Platform abstraction layer** (`n0s/platform/`) — 14 cross-platform functions
  - `openBrowser()`, `getConfigDir()`, `getCacheDir()`, `getHomePath()`
  - `setThreadName()`, `setupSignalHandlers()`, `enableConsoleColors()`
  - Linux + Windows implementations (Windows untested until CI available)
- **Cross-platform compatibility layer** (`n0s/platform/compat.hpp`)
  - 7 function families: strcasecmp, mkdir, sleep, popen/pclose, mkstemp, sysconf_nproc
  - All POSIX-only code wrapped — every .cpp/.hpp now compiles under both GCC and MSVC
  - VirtualAlloc/VirtualLock on Windows, mmap/mlock on Linux (with large page support on both)
  - CreateProcess on Windows, fork/exec on Linux for autotune subprocess isolation
- **CMake MSVC support** — /W4, static CRT, /arch:AVX2, ws2_32+shell32 linking
- **Dead includes removed** — cleaned up `<unistd.h>` and other POSIX-only headers

---

## v3.2.0 — Single Binary + GUI Dashboard (2026-04-02)

### Single Executable (Pillar 1)

- **One binary, zero companion files.** Eliminated the entire `dlopen`/`dlsym` plugin system.
- Both CUDA and OpenCL backends compile as static libraries and link directly into the executable.
- Removed `plugin.hpp` (81 lines of dlopen/dlsym/dlclose abstraction).
- Removed `-ldl` dependency.
- Simplified CMake install rules to single binary.

### GUI Dashboard (Pillar 2)

- **Embedded web dashboard** — modern dark-themed SPA served from the miner binary.
- **9 REST API v1 endpoints** with clean JSON responses:
  - `/api/v1/status` — Mining state, uptime, pool connection
  - `/api/v1/hashrate` — Per-GPU + total hashrate (10s/60s/15m windows)
  - `/api/v1/hashrate/history` — Time-series ring buffer (3600 samples, 1s resolution)
  - `/api/v1/gpus` — GPU telemetry with device names, temp/power/fan/clocks, H/W efficiency
  - `/api/v1/pool` — Shares, difficulty, ping, top difficulties
  - `/api/v1/config` — Pool configuration (wallet masked for security)
  - `/api/v1/autotune` — Cached autotune results per GPU
  - `/api/v1/version` — Version, build info, enabled backends
- **Hashrate history ring buffer** — 3600 samples, 1-second resolution, ~115 KB memory
- **Real-time hashrate chart** — Pure `<canvas>` drawing, per-GPU lines + total with gradient fill
- **GPU telemetry table** — Device names (nvidia-smi / amd-smi), temp coloring, H/W efficiency
- **Tab navigation** — Monitor (live data) + Configuration pages
- **Responsive design** — Mobile-friendly with horizontal scroll for GPU table
- **Pre-gzipped embedded assets** — `Content-Encoding: gzip`, zero runtime compression
- **Total frontend size: 6.1 KB gzipped** (12% of 50 KB budget)
- **`--gui` flag** — Opens browser to dashboard; mining continues in CLI
- **`--gui-dev DIR`** — Hot-reload development mode (serve from filesystem)
- **Legacy endpoints preserved** — `/h`, `/c`, `/r`, `/api.json`, `/style.css` all still work

### Performance (carried from v3.1.x optimization sessions)

| GPU | Hashrate | vs v3.1.0 |
|---|---|---|
| AMD RX 9070 XT (OpenCL) | 5,069 H/s | +12% |
| NVIDIA GTX 1070 Ti (CUDA 11.8) | 1,631 H/s | −3% (intensity rebalance) |
| NVIDIA RTX 2070 (CUDA 12.6) | 2,236 H/s | +4% |

### Build Sizes

| Variant | Size |
|---|---|
| OpenCL-only | 1.0 MB |
| CUDA 11.8 | 3.0 MB |
| CUDA 12.6 | 3.5 MB |
| Container (CUDA 11.8) | 2.3 MB |

---

## v3.1.0 — GPU Autotune (2026-03-31)

### New Features

- **`--autotune` — Automatic GPU parameter discovery**
  - One-command optimal settings for any AMD/OpenCL or NVIDIA/CUDA GPU
  - Quick (2-6 min), Balanced (5-10 min), and Exhaustive modes
  - Subprocess-based evaluation with crash isolation (OOM-safe)
  - Device fingerprinting with cached results in `autotune.json`
  - Stability validation: penalizes high variance and error rates
  - Writes optimized `amd.txt` / `nvidia.txt` configs automatically
  - 10 CLI flags for full control over the tuning process

- **CryptoNight-GPU kernel-aware candidate generation**
  - NVIDIA: threads=8 fixed (128 threads/block matching `__launch_bounds__`)
  - NVIDIA: blocks sweep around architecture-optimal (Pascal=7×SM, Turing+=6×SM)
  - AMD: intensity/worksize sweep with CU-aligned candidates
  - Accurate per-hash memory calculations from actual kernel requirements

- **NVIDIA SM count estimation fallback**
  - Lookup table for 20+ GPU models (Pascal through Ada Lovelace)
  - Architecture-based fallback for unknown models
  - Robust nvidia-smi parsing with digit-prefix validation

### Documentation

- Comprehensive autotune section in README with CLI reference
- New `docs/AUTOTUNE.md` architecture guide
- Benchmark results table with 3-GPU validation data
- Per-phase kernel profiling table

### Testing

- 21 unit tests for autotune framework (scoring, candidates, persistence)
- Golden hash verification on all changes
- 3-GPU validation matrix (AMD RDNA4 + NVIDIA Pascal + NVIDIA Turing)

### Validated Performance (Autotuned)

| GPU | Settings | Hashrate |
|---|---|---|
| AMD RX 9070 XT | intensity=1536, worksize=8 | 4,531 H/s |
| NVIDIA RTX 2070 | threads=8, blocks=216 | 2,160 H/s |
| NVIDIA GTX 1070 Ti | threads=8, blocks=114 | 1,687 H/s |

---

## v3.0.0 — Modern C++ Rewrite (2026-03-29)

Complete modernization of the xmr-stak codebase:
- Pure C++17, Linux-only, zero C files
- `n0s::` namespace, `n0s/` directory structure
- Zero-warning build (`-Wall -Wextra`)
- Single-algorithm focus (CryptoNight-GPU only)
- Container build system (Podman/Docker)
- `--benchmark` with JSON output and stability CV%
- `--profile` for per-kernel phase timing
- Dead code removal, smart pointers, modern headers
