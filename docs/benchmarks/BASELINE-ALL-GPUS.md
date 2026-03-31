# CryptoNight-GPU Profiling Baselines — All 3 GPUs

**Date:** 2026-03-31 (Session 46 — re-baselined with Phase 4/5 split)
**Commit:** af8c7e9 (optimize/phase45-split-profile merged to master)
**Mode:** `--benchmark 5 --profile` (50 dispatch average)

## Per-Phase Timing Summary

| Phase | RX 9070 XT (RDNA4) | GTX 1070 Ti (Pascal) | RTX 2070 (Turing) |
|-------|--------------------:|---------------------:|-------------------:|
| Phase 1: Keccak | ~100 µs (0.0%) | ~0 µs (0.0%) | ~0 µs (0.0%) |
| Phase 2: Scratchpad | 40,000 µs (6.2%) | 17,083 µs (2.5%) | 24,450 µs (3.1%) |
| **Phase 3: GPU compute** | **355,000 µs (55%)** | **565,756 µs (82.5%)** | **665,676 µs (85.2%)** |
| **Phase 4: Implode** | **~235,000 µs (36%)** | **102,584 µs (15.0%)** | **91,019 µs (11.6%)** |
| Phase 5: Finalize | *combined w/Phase 4* | 263 µs (0.0%) | 288 µs (0.0%) |
| **Total** | **~645,000 µs** | **685,687 µs** | **781,434 µs** |
| **Hashrate** | **~2,380 H/s** | **1,552 H/s** | **2,211 H/s** |
| Intensity | 1,536 | 1,064 | 1,728 |

## Key Findings (Session 46)

### Phase 5 is Negligible
Phase 5 (16 AES rounds + Keccak + target check) takes only ~270 µs on NVIDIA — less than 0.04%
of total time. ALL of the "Phase 4+5" time is in Phase 4 (scratchpad compression).

### Phase 4 Timing by Architecture
- **AMD RDNA4: 36%** — Phase 4 is a major bottleneck on AMD
- **NVIDIA Pascal: 15%** — Secondary target
- **NVIDIA Turing: 11.6%** — Smallest fraction

AMD's Phase 4 takes disproportionately longer than NVIDIA's, likely because:
1. AMD uses software AES via LDS table lookups (4 × 256 entries)
2. NVIDIA uses a bank-conflict-free 256×32 AES table layout
3. RDNA4's LDS bandwidth may not keep up with 160 lookups per scratchpad block

### Environment Change Correction
Session 38 baseline reported 4,427 H/s on AMD RX 9070 XT. Testing at the exact Session 37
commit shows ~2,380 H/s — confirming the environment changed (likely AMD driver update),
NOT a code regression.

## Optimization Priority (Updated)

1. **Phase 3 (55-85%)** — Algorithmically resistant to optimization (intentional FP division chain)
2. **Phase 4 on AMD (36%)** — High-value target. Potential approaches:
   - Bank-conflict-free AES table layout (like CUDA's 256×32 scheme)
   - Prefetching next scratchpad block during AES computation
   - Workgroup size tuning for LDS occupancy
3. **Phase 4 on NVIDIA (12-15%)** — Already well-optimized with conflict-free AES tables
4. **Phase 2 (3-6%)** — Limited impact
