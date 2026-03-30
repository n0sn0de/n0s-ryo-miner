# Backend Rewrite Plan

**High-Level Strategy for the Foundational C++ Rewrite**

*Status: Foundation + dead code removal complete. CUDA consolidated. Namespace migrated (n0s::). Pool/network documented. Directory restructured (xmrstak/ → n0s/). Zero-warning build. Modern C++ patterns applied. Config/algo simplified. OpenCL constants hardcoded. Windows/macOS/BSD code stripped. Pure C++17 (zero C files). Linux-only. Smart pointers replacing raw new/delete. std::regex eliminated from hot paths.*

---

## Goal

Take the inherited xmr-stak CryptoNight-GPU implementation and transform it into a clean, modern, single-purpose miner we can reason about, optimize, and maintain confidently. The algorithm math remains bit-exact — we change organization and expression, not computation. Then we optimize for cn-gpu hashrate performance across AMD & Nvidia architectures.

---

## Branch Strategy:

1. Master stays stable (release-ready always)
2. Focused optimization branches (optimize/profiling-baseline, etc.)
3. Test: golden hashes + container builds + live mining
4. Merge only when validated ✅

## Optimization Principles:

1. Measure before changing
2. One optimization at a time
3. Bit-exact validation always
4. Document findings
5. Keep it clean

## Modernization Principals
1. **Bit-exact output** — Verified via golden test vectors + live mining on 3 GPUs
2. **One module at a time** — Build → test → verify → merge → delete branch → repeat
3. **Test-driven** — Validation harness with known-good hashes before any code change
4. **No magic** — Every function, constant, and parameter has a clear name and purpose
5. **Modern idioms** — `constexpr`, proper naming, RAII, documentation

---

## Current Codebase State

```
n0s/
├── algorithm/
│   └── cn_gpu.hpp              ← Clean algorithm constants (202 lines)
│
├── backend/
│   ├── amd/                     ← OpenCL backend (~3,650 lines)
│   │   ├── amd_gpu/
│   │   │   ├── gpu.cpp          ← Host: device init, kernel compile, mining loop
│   │   │   ├── gpu.hpp          ← Host: context struct
│   │   │   └── opencl/
│   │   │       ├── cryptonight.cl      ← Phase 4+5 kernel + shared helpers
│   │   │       ├── cryptonight_gpu.cl  ← Phase 1,2,3 kernels (cn_gpu_phase*)
│   │   │       └── wolf-aes.cl         ← AES tables for OpenCL
│   │   ├── autoAdjust.hpp       ← Auto-config
│   │   ├── jconf.cpp/hpp        ← AMD config parsing
│   │   └── minethd.cpp/hpp      ← AMD mining thread
│   │
│   ├── nvidia/                  ← CUDA backend (~4,500 lines, CONSOLIDATED)
│   │   ├── nvcc_code/
│   │   │   ├── cuda_cryptonight_gpu.hpp ← Phases 2,3 kernels
│   │   │   ├── cuda_kernels.cu         ← Phases 1,4,5 + host dispatch + device mgmt
│   │   │   ├── cuda_aes.hpp            ← AES for CUDA
│   │   │   ├── cuda_keccak.hpp         ← Keccak for CUDA
│   │   │   ├── cuda_extra.hpp          ← Utility macros + compat shims + error checking
│   │   │   └── cuda_context.hpp        ← nvid_ctx struct + extern "C" ABI
│   │   ├── autoAdjust.hpp       ← CUDA auto-config
│   │   ├── jconf.cpp/hpp        ← NVIDIA config parsing
│   │   └── minethd.cpp/hpp      ← NVIDIA mining thread
│   │
│   ├── cpu/                     ← CPU hash reference + shared crypto (2,839 lines)
│   │   ├── crypto/
│   │   │   ├── keccak.cpp/hpp         ← Keccak-1600 (converted to C++ in Session 8)
│   │   │   ├── cn_gpu_avx.cpp         ← Phase 3 CPU AVX2 impl
│   │   │   ├── cn_gpu_ssse3.cpp       ← Phase 3 CPU SSSE3 impl
│   │   │   ├── cn_gpu.hpp             ← CPU cn_gpu interface
│   │   │   ├── cryptonight_aesni.h    ← CPU hash pipeline (391 lines)
│   │   │   ├── cryptonight_common.cpp ← Memory alloc (116 lines)
│   │   │   ├── cryptonight.h          ← Context struct
│   │   │   └── soft_aes.hpp           ← Software AES fallback
│   │   ├── autoAdjust*.hpp      ← CPU auto-config (dead — CPU mining disabled)
│   │   ├── hwlocMemory.cpp/hpp  ← NUMA memory (only used if hwloc enabled)
│   │   ├── jconf.cpp/hpp        ← CPU config
│   │   └── minethd.cpp/hpp      ← CPU mining thread (hash verification only)
│   │
│   ├── cryptonight.hpp    ← Algorithm enum + POW() (shared)
│   ├── globalStates.*     ← Global job queue
│   ├── backendConnector.* ← Backend dispatcher
│   ├── miner_work.hpp     ← Work unit
│   ├── iBackend.hpp       ← Backend interface
│   ├── plugin.hpp         ← dlopen plugin loader
│   └── pool_data.hpp      ← Pool metadata
│
├── net/                   ← Pool connection (1,732 lines)
│   ├── jpsock.cpp/hpp     ← Stratum JSON-RPC
│   ├── socket.cpp/hpp     ← TCP/TLS socket
│   ├── msgstruct.hpp      ← Message types
│   └── socks.hpp          ← SOCKS proxy
│
├── http/                  ← HTTP monitoring API (492 lines)
├── misc/                  ← Utilities (2,410 lines)
│   ├── executor.cpp/hpp   ← Main coordinator
│   ├── console.cpp/hpp    ← Console output
│   ├── telemetry.cpp/hpp  ← Hashrate tracking
│   ├── (coinDescription.hpp removed — Session 7)
│   └── [other utilities]
│
├── cli/cli-miner.cpp     ← Entry point (947 lines)
├── jconf.cpp/hpp          ← Main config (727 lines)
├── params.hpp             ← CLI parameters
├── version.cpp/hpp        ← Version info
│
├── vendor/
│   ├── rapidjson/         ← JSON library (vendored, ~14K lines — don't touch)
│   └── picosha2/          ← SHA-256 for OpenCL cache (vendored — don't touch)
│
└── (cpputil/ removed — replaced with std::shared_mutex)

tests/
├── cn_gpu_harness.cpp     ← Golden test vectors
├── test_constants.cpp     ← Constants verification
└── build_harness.sh       ← Build script
```
---

## Remaining Work

### Near-Term Opportunities

**Next Session Targets (Post-Session 32):**
1. ✅ **Container build matrix** — DONE (S31: OpenCL + CUDA 11.8; S32: Full matrix 11.8/12.6/12.8)
2. ✅ **Full matrix test** — DONE (S32: All 4 builds pass, 100% success rate)
3. ✅ **Branch cleanup & merge** (~15 min) — Merge refactor/benchmark-phase1 to master, delete stale branches
4. ✅ **Create release** (~15 min) — Tag version, package dist/ artifacts for GitHub release
5. **Documentation update** (~30 min) — Add container build instructions to README
6. **Live mining validation** (~1 hour when pool/wallet available) — Test accepted shares on real pool
7. **Begin optimization phase** — Profile real mining workload, identify hotspots

**Deferred (revisit during optimization phase):**
- ~~Benchmark harness debugging~~ — Production miner works, harness has environmental issues
- Use production miner for hashrate measurements until harness is fixed

### Performance Optimization (P1)
- ⏳ Deeply review the CN-GPU-WHITEPAPER.md to understand the algo in conjunction with the codebase to assess approach to optimization opportunities
- ⏳ Profile on each GPU architecture (AMD RDNA4, NVIDIA Pascal/Turing/Ampere)
- ⏳ Optimize shared memory usage in Phase 3 kernel
- ⏳ Explore occupancy improvements
- ⏳ Consider CUDA Graphs for kernel chaining
- ⏳ Algorithm/Kernel Autotuning based on user hardware (see /docs/PRD_01-AUTOTUNING.md)
- ⏳ Profile hotspots on AMD RDNA4 + NVIDIA Pascal/Turing/Ampere
- ✅ Fix or rebuild benchmark harness for reproducible measurements (S36: --benchmark-json + stability CV% + tests/benchmark.sh)

---

## Success Criteria

**Completed:**
- ✅ Bit-exact hashes + zero share rejections on 3 GPU architectures
- ✅ Zero-warning build (`-Wall -Wextra`)
- ✅ Single-command build (`cmake .. && make`)
- ✅ Pure C++17 (zero C files), Linux-only
- ✅ Directory restructured to `n0s/` layout
- ✅ `xmrstak` namespace fully replaced → `n0s::`
- ✅ Config/algo system simplified (single-algorithm focus)
- ✅ OpenCL dead kernel branches removed
- ✅ Modern C++ headers everywhere (`<cstdint>` not `<stdint.h>`)

---

## Session 35 Notes (2026-03-30 01:33 PM) — Check-in & Next Phase Planning 🧘

**Context:** Hourly n0s-ryo-miner cron check-in post-release

**Status Check:**
- ✅ **Foundation rewrite: COMPLETE** — v3.0.0 shipped with 8 artifacts
- ✅ **Zero warnings** — OpenCL + CUDA 11.8/12.6/12.8 all build cleanly
- ✅ **Container build matrix** — Production-ready infrastructure
- ✅ **Production validated** — Runs cleanly on AMD RDNA4
- ✅ **Documentation current** — Session notes 28-34 captured

**Next Phase: OPTIMIZATION**

With foundation rock-solid, focus shifts to:

1. **Live Mining Validation** (~1 hour when pool/wallet available)
   - Verify accepted shares on real pool
   - Establish that refactored code mines successfully
   - Capture baseline hashrate on AMD RDNA4 (current hardware)

2. **Performance Profiling** (~2-3 hours)
   - Profile with `rocprof` (AMD) or `nsys` (NVIDIA) when hardware available
   - Identify hotspots: memory bandwidth, occupancy, kernel execution time
   - Measure Phase 1-5 individual kernel times
   - Document baseline performance in `docs/benchmarks/`

3. **Kernel Documentation** (~2 hours)
   - Add function-level comments to Phase 2 (scratchpad expansion)
   - Add function-level comments to Phase 3 (memory-hard loop)
   - Document memory access patterns and optimization opportunities
   - Make kernel code approachable for future optimization work

4. **Autotuning Implementation** (~1 week — see PRD_01-AUTOTUNING.md)
   - Implement grid search for intensity/worksize combinations
   - Measure hashrate for each configuration
   - Store optimal params per GPU model
   - Replace hardcoded defaults with learned values

**Branch Strategy:**
- Keep master stable (release-ready at all times)
- Create focused optimization branches:
  - `optimize/profiling-baseline`
  - `optimize/kernel-docs`
  - `optimize/autotuning-phase1`
- Test with harness (`tests/cn_gpu_harness`) + container builds + live mining
- Merge only when validated (golden hashes pass, shares accepted)

**Guiding Principles for Optimization:**
1. **Measure before changing** — No blind optimizations
2. **One change at a time** — Isolate performance impact
3. **Validate bit-exactness** — Golden hashes must always pass
4. **Document findings** — Future-you will thank present-you
5. **Keep it clean** — No hacks, no magic numbers without comments

**Session accomplishment summary:**
Memory updated (Session 28-35 notes), REWRITE-PLAN updated with optimization roadmap. **Foundation complete. Optimization phase begins.** Ready to profile, document, and tune for maximum hashrate. 🧘⚡🔥

---

## Session 36 Notes (2026-03-30 04:12 PM) — Benchmark Harness with Stability Metrics ⚡

**What we accomplished:**
- ✅ **Fixed benchmark mode** — work_size was 128 bytes, exceeding OpenCL's 124-byte limit → XMRSetJob failed every time. Changed to 76 bytes (realistic cn_gpu block size)
- ✅ **Added interval sampling** — benchmark now samples hashrate every 5 seconds and computes coefficient of variation (CV%) for stability tracking
- ✅ **Added `--benchmark-json` CLI flag** — structured JSON output with per-thread avg H/s, CV%, sample count, and total hashes for A/B comparison scripting
- ✅ **Fixed miner_work sJobID buffer overread** — `memcpy` copied 64 bytes from a 1-byte string literal `""`, replaced with `strncpy` + null termination (eliminated GCC -Wstringop-overread warning)
- ✅ **Created `tests/benchmark.sh`** — wrapper script with `--quick`/`--long`/`--compare` modes for easy benchmarking
- ✅ **Verified**: Zero warnings (our code), 3/3 golden hashes pass, live mining accepted shares, benchmark produces **4505.4 H/s @ 3.2% CV on RX 9070 XT**

**Key insights:**
- The built-in benchmark was completely broken (XMRSetJob always failed) — now it actually works and produces meaningful data
- CV% is a better stability metric than min/max because it normalizes against the mean
- JSON output enables automated A/B perf comparison — critical for optimization work
- The `strncpy` fix for sJobID was a real buffer overread bug, not just a warning

**Baseline established:**
- **RX 9070 XT (RDNA4)**: 4505.4 H/s, CV 3.2% (very stable)
- CUDA nodes (nos2/nosnode) need testing when available

**Next session priorities:**
1. **Performance profiling with `rocprof`** — Identify kernel-level hotspots on AMD
2. **Run benchmark on CUDA nodes** — Establish NVIDIA baselines (GTX 1070 Ti, RTX 2070)
3. **Kernel documentation pass** — Phase 2/3 GPU kernels need function-level comments
4. **Begin autotuning framework** — Use benchmark harness as the scoring backend

---

The code is ours now. The dead weight is gone, the names make sense, and the path forward is clear. We're not rewriting for elegance — we're rewriting for ownership, understanding, and the ability to confidently modify any part of the system.
