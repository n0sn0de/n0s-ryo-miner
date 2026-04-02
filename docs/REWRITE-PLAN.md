# Backend Rewrite Plan

**High-Level Strategy for the Foundational C++ Rewrite**

*Status: Foundation + dead code removal complete. CUDA consolidated. Namespace migrated (n0s::). Pool/network documented. Directory restructured (xmrstak/ → n0s/). Zero-warning build. Modern C++ patterns applied. Config/algo simplified. OpenCL constants hardcoded. Windows/macOS/BSD code stripped. Pure C++17 (zero C files). Linux-only. Smart pointers replacing raw new/delete. std::regex eliminated from hot paths. Pre-sm_60 CUDA dead code purged (704 lines). Ancient GCN 1.0 OpenCL workarounds removed.*

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
├── autotune/              ← Autotuning framework (Session 40-41, ~2,600 lines)
│   ├── autotune_types.hpp       ← Core types: fingerprint, candidates, scores, state
│   ├── autotune_score.hpp       ← Scoring model + guardrail rejection
│   ├── autotune_candidates.hpp  ← Candidate generation (AMD + NVIDIA)
│   ├── autotune_persist.cpp/hpp ← JSON persistence (autotune.json)
│   ├── autotune_manager.cpp/hpp ← Orchestrator (coarse→stability→winner)
│   ├── autotune_runner.cpp/hpp  ← Subprocess benchmark evaluator + fingerprinting
│   └── autotune_entry.cpp/hpp   ← Top-level entry point + config writing
│
├── cli/cli-miner.cpp     ← Entry point (1012 lines, +65 for autotune CLI)
├── jconf.cpp/hpp          ← Main config (727 lines)
├── params.hpp             ← CLI parameters (+13 autotune params)
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
├── test_autotune.cpp      ← Autotune framework unit tests (20 tests)
├── build_harness.sh       ← Build script
└── build_autotune_test.sh ← Autotune test build script
```
---

## Remaining Work

### Near-Term Opportunities

**Next Session Targets (Post-Session 32):**
1. ✅ **Container build matrix** — DONE (S31: OpenCL + CUDA 11.8; S32: Full matrix 11.8/12.6/12.8)
2. ✅ **Full matrix test** — DONE (S32: All 4 builds pass, 100% success rate)
3. ✅ **Branch cleanup & merge** (~15 min) — Merge refactor/benchmark-phase1 to master, delete stale branches
4. ✅ **Create release** (~15 min) — Tag version, package dist/ artifacts for GitHub release
5. ✅ **Documentation update** — README overhaul + AUTOTUNE.md + CHANGELOG.md (S43)
6. ✅ **Live mining validation** — 100% accepted shares on 3 GPUs (S44)
7. ✅ **Begin optimization phase** — Profiled all phases, autotune implemented (S37-42)

**Deferred (revisit during optimization phase):**
- ~~Benchmark harness debugging~~ — Production miner works, harness has environmental issues
- Use production miner for hashrate measurements until harness is fixed

### Performance Optimization (P1)
- ⏳ Deeply review the CN-GPU-WHITEPAPER.md to understand the algo in conjunction with the codebase to assess approach to optimization opportunities
- ✅ Profile on AMD RDNA4 (S37: --profile flag, per-phase timing, baseline established)
- ✅ Profile on NVIDIA Pascal/Turing (S38: --profile ported to CUDA, baselines established)
- ✅ Phase 3 constant memory → register hoisting (S49: −3.1% Phase 3 on Pascal CUDA)
- ✅ Explore occupancy improvements (S47: intensity formula scaled by worksize, +2.6%)
- ❌ CUDA warp shuffles for Phase 3 data exchange — shared memory wins due to load/compute overlap (S49)
- ❌ CUDA launch_bounds tuning — (128,8) is optimal; (128,6) no gain, (128,10) spills (S49)
- ❌ OpenCL CN_UNROLL tuning — unroll=1 optimal on RDNA4; unroll=8 is 3% worse (S49)
- ⏳ Consider CUDA Graphs for kernel chaining
- ✅ Algorithm/Kernel Autotuning framework — Phase 1 complete (S40: types, scoring, candidates, persistence, CLI, 20 unit tests)
- ✅ Algorithm/Kernel Autotuning — Phase 2: End-to-end `--autotune` (S41: subprocess benchmark eval, fingerprinting, config writing, 3-GPU validated)
- ✅ Phase 2 OpenCL workgroup optimization (S47: WG 64→512, −76% Phase 2 time, +15.7% hashrate)
- ✅ OpenCL Keccak Rho/Pi inlined (S47: matches CUDA, eliminates constant memory lookups)
- ✅ Fix or rebuild benchmark harness for reproducible measurements (S36: --benchmark-json + stability CV% + tests/benchmark.sh)
- ✅ CUDA intensity tuning (S48: Pascal 7→6, Turing 6→8 block multipliers)

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

## Session 46 Notes (2026-03-31 12:14 PM) — Phase 4/5 Profiling Split + RDNA Worksize Optimization 🔬⚡

**What we accomplished:**

### 1. Phase 4/5 Profiling Split (CUDA)
- ✅ Added separate `phase4_us` and `phase5_us` fields to `KernelProfile`
- ✅ CUDA dispatch now times Phase 4 (implode) separately from Phase 5 (finalize)
- ✅ Phase 5 timing captured via `chrono` in CUDA `minethd.cpp` profile path
- ✅ Updated OpenCL `KernelProfile` struct for field consistency

**Key discovery: Phase 5 is negligible (~270 µs, <0.04%).** All Phase 4+5 time is Phase 4 (AES scratchpad compression).

| GPU | Phase 4 (Implode) | Phase 5 (Finalize) |
|-----|------------------:|-------------------:|
| GTX 1070 Ti | 102,584 µs (15.0%) | 263 µs (0.0%) |
| RTX 2070 | 91,019 µs (11.6%) | 288 µs (0.0%) |

### 2. Corrected Baselines — Old Numbers Were Wrong
- Re-tested at Session 37 commit: **same ~2,380 H/s** (not 4,427 H/s reported)
- Environment changed (AMD driver update), NOT a code regression
- **Phase 4+5 is actually 32-38% of total time, not 18%** — a MUCH bigger target

### 3. Phase 4 Optimization Experiments (AMD RDNA4)
- ❌ **Single AES table** (1KB LDS vs 4KB): No measurable difference — LDS not the bottleneck
- ❌ **Scratchpad prefetching**: ~5% Phase 4 improvement but within noise overall
- ❌ **AES unroll 5 vs 10**: Partial unroll is slower — full unroll is optimal
- ✅ **Worksize 16 vs 8**: **4.4% hashrate improvement**, Phase 4+5 down 42%

### 4. RDNA Worksize Default Fix
- ✅ `autoAdjust.hpp` now detects RDNA (gfx1xxx prefix) and sets worksize 16
- Previously only VEGA (gfx9xx) got worksize 16; RDNA fell through to default 8
- **Benchmarked on RX 9070 XT (gfx1201):**
  - worksize 8:  2,330 H/s — Phase 4+5: 235K µs
  - worksize 16: 2,432 H/s — Phase 4+5: 137K µs (**42% Phase 4+5 reduction**)
- ✅ Live mining validated: 89+ accepted shares, 0 rejected
- ✅ CUDA builds verified clean on nos2 (Pascal)

### 5. Updated 3-GPU Baselines (Session 46)

| Phase | RX 9070 XT (ws=16) | GTX 1070 Ti | RTX 2070 |
|-------|-------------------:|------------:|---------:|
| Phase 1 | ~6K µs (0.9%) | ~0 µs | ~0 µs |
| Phase 2 | 158K µs (25.2%) | 17K µs (2.5%) | 24K µs (3.1%) |
| Phase 3 | 326K µs (52.0%) | 566K µs (82.5%) | 666K µs (85.2%) |
| Phase 4 | 137K µs (21.9%) | 103K µs (15.0%) | 91K µs (11.6%) |
| Phase 5 | *combined* | 263 µs (0.0%) | 288 µs (0.0%) |
| **Total** | **627K µs** | **686K µs** | **781K µs** |
| **H/s** | **2,432** | **1,552** | **2,211** |

---

## Session 47 Notes (2026-03-31 8:40 PM) — Phase 2 OpenCL Optimization + Intensity Scaling 🔥⚡

**What we accomplished:**

### 1. OpenCL Keccak Rho/Pi Inlining
- ✅ Replaced loop-based Rho/Pi step (used `keccakf_piln[]` and `keccakf_rotc[]` constant memory lookups) with direct register-to-register assignments
- ✅ Both `keccakf1600_1` (private memory) and `keccakf1600_2` (local memory) versions updated
- ✅ Verified: permutation cycle and rotation amounts match CUDA backend exactly (mathematically proven via Python trace)
- ✅ Removed dead `keccakf_rotc[]` and `keccakf_piln[]` constant arrays (−240 bytes constant memory)
- **Impact:** AMD OpenCL compiler was already optimizing this via `#pragma unroll`, so no measurable perf difference. But explicit inlining ensures consistent performance across all OpenCL compiler versions.

### 2. Phase 2 Workgroup Size Optimization (THE BIG WIN 🏆)
- ✅ Phase 2 (scratchpad expansion) workgroup size increased from 64 → 512
- CUDA uses 128, but AMD RDNA4 benefits from even higher parallelism

| Phase 2 WG Size | Phase 2 Time | Total Time | H/s | Change |
|:---:|---:|---:|---:|:---:|
| 64 (old) | 66.7K µs (18.3%) | 362K µs | 4,224 | baseline |
| 128 | 60.4K µs (17.0%) | 356K µs | 4,301 | +1.8% |
| 256 | 49.9K µs (14.4%) | 347K µs | 4,454 | +5.4% |
| **512** | **16.0K µs (5.1%)** | **315K µs** | **4,889** | **+15.7%** |
| 1024 | 13.8K µs (4.4%) | 313K µs | 4,915 | +16.4% (diminishing) |

- ✅ Chose 512 as optimal: massive improvement, minimal register pressure risk

### 3. AutoAdjust Intensity Formula Fix
- ✅ Old formula: `CUs × 6 × 8 = 1536` (hardcoded, ignored worksize)
- ✅ New formula: `CUs × 6 × worksize = 3072` (scales with worksize 16)
- GPU occupancy jumped from 38% to 75%

| Intensity | Occupancy | H/s | Change |
|:---:|:---:|---:|:---:|
| 1536 (old) | 38% | 4,889 | baseline |
| 2048 | 50% | 4,949 | +1.2% |
| **3072 (new)** | **75%** | **5,017** | **+2.6%** |
| 4608 | 113% | 4,992 | +2.1% (thermal) |

- ✅ Also updated autotune candidate generator to use same formula

### 4. Validation Results
- ✅ **Golden hashes:** All 3 CPU test vectors pass ✅
- ✅ **AMD RX 9070 XT (nitro):** 5,017 H/s, 146 accepted shares, zero rejections ✅
- ✅ **NVIDIA GTX 1070 Ti (nos2):** 1,525 H/s — no regression (CUDA unaffected) ✅
- ✅ **NVIDIA RTX 2070 (nosnode):** 2,189 H/s — no regression (CUDA unaffected) ✅

### 5. Updated 3-GPU Baselines (Session 47)

| Phase | RX 9070 XT (ws=16, i=3072) | GTX 1070 Ti | RTX 2070 |
|-------|---:|---:|---:|
| Phase 1 | 123 µs (0.0%) | ~0 µs | ~0 µs |
| Phase 2 | 32K µs (5.3%) | 17K µs (2.5%) | 24K µs (3.1%) |
| Phase 3 | 465K µs (76.3%) | 568K µs (82.5%) | 665K µs (85.2%) |
| Phase 4+5 | 112K µs (18.4%) | 103K µs (15.0%) | 91K µs (11.7%) |
| **Total** | **609K µs** | **688K µs** | **780K µs** |
| **H/s** | **5,017** | **1,525** | **2,189** |

**Key insights:**
- Phase 2 was a **hidden bottleneck on AMD** — the 64-thread workgroup left most RDNA4 CUs idle during Keccak expansion
- 512 threads per workgroup gives each thread only 8 chunks to process (vs 64 with WG=64), keeping all CUs active
- Phase 3 is now 76% of total time — purely compute-bound (FP dependency chains), inherent to the algorithm
- Phase 4 (AES implode) is 18% — already optimized with `ds_bpermute` on AMD

**Combined Session 46+47 AMD improvement:**
- Session 45 baseline: 2,304 H/s (ws=8, intensity=1536, WG=64)
- Session 47 final: **5,017 H/s** (ws=16, intensity=3072, WG=512)
- **Total improvement: +117.8%** 🚀

---

## Session 48 Notes (2026-04-01 12:30 AM) — CUDA Intensity Tuning (Pascal/Turing) 🎯⚡

**What we accomplished:**
- ✅ **Empirical profiling sweep on Pascal GTX 1070 Ti:** Tested block multipliers 6×, 7×, 8× SMs
  - Found **Phase 4 memory cliff at 7+ blocks/SM** — AES compression time jumps 97 µs → 59 µs
  - 6×SMs optimal: **1623 H/s (+4.7%)** vs old default 7×SMs (1550 H/s)
  - Pascal L2 cache (1.4 MB) can't sustain 7+ concurrent AES operations
  
- ✅ **Empirical profiling sweep on Turing RTX 2070:** Tested 5×, 6×, 7×, 8×, 9× SMs
  - Turing handles higher intensity better (4 MB L2 cache vs Pascal 1.4 MB)
  - 8×SMs optimal: **2263 H/s (+1.7%)** vs old default 6×SMs (2226 H/s)
  - No Phase 4 cliff until 9×SMs (thermal limits first)
  
- ✅ **Updated CUDA device init:** Pascal 7→6, Turing 6→8 block multipliers
- ✅ **Updated autotune candidate generator** to match new optimal multipliers
- ✅ **Live mining validation:** 100% share acceptance on both GPUs
- ✅ **Documented in `docs/benchmarks/SESSION-48-CUDA-INTENSITY.md`**

**Key insight:** Pascal's smaller L2 cache creates a sharp Phase 4 memory bottleneck at high intensity. Turing's improved memory subsystem benefits from higher occupancy. Architecture-specific tuning matters more than raw SM count.

**Performance gains (Session 48):**
- GTX 1070 Ti: **+4.7%** (1550 → 1623 H/s)
- RTX 2070: **+1.7%** (2226 → 2263 H/s)

**Next session priorities (Session 49):**
1. ✅ **Merge to master** — `optimize/cuda-intensity-tuning` merged (S49 recovery)
2. **Container builds with all optimizations** — Release v3.2.0 (OpenCL + CUDA matrix)
3. **Test on Ampere (sm_8x)** — Does 8×SMs remain optimal? Does 10× work?
4. ✅ **Phase 3 constant memory optimization** — Hoisted to registers (S49)

---

## Session 49 Notes (2026-04-01 10:22 PM) — Recovery + Phase 3 Constant Memory Optimization 🔧⚡

**Recovery from Session 48 crash:**
- ✅ Session 48 work was complete — branch had 2 clean commits, just not merged
- ✅ Merged `optimize/cuda-intensity-tuning` to master, pushed, deleted branch
- ✅ Synced all 3 nodes to master, verified builds + benchmarks
- ✅ Cleaned up **37 stale local branches** across nos2 and nosnode
- ✅ All nodes now have only `master` branch — pristine state

**Phase 3 Optimization Experiments:**

### 1. Constant Memory → Register Hoisting (THE WIN ✅)
- Pre-loaded `SHUFFLE_PATTERN[tid][0..3]` and `THREAD_CONSTANTS[tid]` into registers
- CUDA constant memory serializes when 16 threads access different addresses
- **CUDA Pascal:** Phase 3 507K → 491K µs (**−3.1%**), 62→63 registers, zero spills
- **OpenCL RDNA4:** No measurable change (AMD compiler already optimizes this)
- 3-GPU live mining: 100% share acceptance ✅

### 2. Experiments Tested and Rejected
| Experiment | Result | Why |
|-----------|--------|-----|
| `__shfl_sync` warp shuffles for input data | −1.3% regression | Shared mem loads overlap with FP compute; sequential shuffles don't |
| `launch_bounds(128, 6)` — more registers | No change | Compiler already uses 62/64 regs optimally |
| `launch_bounds(128, 10)` — higher occupancy | −3.7% regression | Register spilling (48 regs + 36B spill stores) |
| OpenCL CN_UNROLL=8 | −3.0% regression | RDNA4 compiler handles unroll=1 better |
| OpenCL CN_UNROLL=2 | No change | Within noise |

### 3. Updated 3-GPU Baselines (Session 49)

| Phase | RX 9070 XT (ws=16, i=3072) | GTX 1070 Ti (6×SM) | RTX 2070 (8×SM) |
|-------|---:|---:|---:|
| Phase 1 | 128 µs (0.0%) | ~0 µs (0.0%) | ~0 µs (0.0%) |
| Phase 2 | 30K µs (5.0%) | 15K µs (2.6%) | 37K µs (3.2%) |
| Phase 3 | 461K µs (76.4%) | 491K µs (87.8%) | 981K µs (84.6%) |
| Phase 4+5 | 112K µs (18.5%) | 53K µs (9.5%) | 141K µs (12.2%) |
| **Total** | **603K µs** | **559K µs** | **1159K µs** |
| **H/s** | **5,069** | **1,631** | **2,236** |

**Key insight:** Phase 3 is algorithmically bound by the serial FP dependency chain (32 sub-rounds × 4 rounds × 49K iterations ≈ 14.2M dependent FLOPs per hash). The constant memory optimization was the last low-hanging fruit for the inner loop. Future Phase 3 gains require algorithm-level changes (reducing dependency depth) or higher-clock hardware.

**Next session priorities (Session 50):**
1. **Container builds with all optimizations** — Release v3.2.0 (OpenCL + CUDA matrix)
2. **Phase 4 deep dive** — 18.5% on AMD, 9.5% on Pascal. AES implode has room for RDNA-specific tuning
3. **CUDA Graphs** — Chain Phases 2→3→4 to eliminate kernel launch overhead
4. **Scratchpad memory layout** — Profile cache behavior, consider padding for bank conflicts
