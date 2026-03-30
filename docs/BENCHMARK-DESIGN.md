# Benchmark Harness Design

**Goal:** Controlled, repeatable performance measurements for CN-GPU optimization work.

## Current State

The benchmark harness (`tests/benchmark_harness.cpp`) is a **minimal foundation** that provides:

- OpenCL device initialization and memory allocation
- Timing infrastructure (fixed duration, clean shutdown via SIGINT)
- Result reporting (console + JSON export)
- Signal handling for graceful termination

**What it doesn't do yet:** Run actual CN-GPU kernels (currently just an empty dispatch loop).

---

## Implementation Roadmap

### Phase 1: Kernel Integration (Current Priority)

**Goal:** Make the benchmark harness run real CN-GPU hashing.

**Tasks:**

1. **Load OpenCL kernels** (`n0s/backend/amd/amd_gpu/opencl/*.cl`)
   - Read kernel source from disk (cryptonight.cl, cryptonight_gpu.cl, wolf-aes.cl)
   - Compile with `clBuildProgram` (hardcode algorithm constants for benchmark)
   - Extract kernel handles (`clCreateKernel`)

2. **Implement 5-phase hash pipeline**
   - Phase 1: `cn_gpu_phase1` (Keccak + AES key expansion)
   - Phase 2: `cn_gpu_phase2` (scratchpad expansion via Keccak)
   - Phase 3: `cn_gpu_phase3` (main memory-hard loop)
   - Phase 4: `cn_gpu_phase4` (scratchpad implosion)
   - Phase 5: `cn_gpu_phase5` (final Keccak)

3. **Use test vectors from `cn_gpu_harness.cpp`**
   - Import the 3 golden test vectors (76-byte zero block, nonce=1, 43-byte all-ones)
   - Validate each hash against known-good output
   - Detect any regressions immediately

4. **Measure GPU metrics**
   - Kernel execution time (via `clGetEventProfilingInfo`)
   - Memory bandwidth (scratchpad reads/writes per hash × hash rate)
   - Occupancy (via vendor tools: `rocprof` for AMD, `nvprof` for NVIDIA)

**Estimated effort:** 4-6 hours (copy-paste from `n0s/backend/amd/amd_gpu/gpu.cpp`, simplify).

---

### Phase 2: CUDA Support (Optional)

**Goal:** Benchmark NVIDIA GPUs using CUDA backend.

**Tasks:**

1. Load CUDA kernels from `n0s/backend/nvidia/nvcc_code/*.cu`
2. Implement 5-phase pipeline (mirror OpenCL structure)
3. Use `cudaEvent_t` for precise GPU timing
4. Validate with same golden test vectors

**Estimated effort:** 3-4 hours.

---

### Phase 3: Multi-Device Benchmarking (Future)

**Goal:** Compare performance across multiple GPUs in parallel.

**Tasks:**

1. Enumerate all OpenCL/CUDA devices
2. Spawn one benchmark thread per device
3. Aggregate results (total hashrate, per-device breakdown)
4. JSON export with array of device results

**Estimated effort:** 2-3 hours.

---

### Phase 4: Regression Tracking (Future)

**Goal:** Detect performance regressions automatically.

**Tasks:**

1. Store baseline results in `docs/benchmarks/baseline.json`
2. Compare new results against baseline (% change)
3. Fail CI if hashrate drops >5%
4. Track improvements over time

**Estimated effort:** 1-2 hours.

---

## Why This Matters

**Before optimization work, we need:**

1. **Reproducible measurements** — Same input → same hashrate
2. **Isolation** — GPU-only performance (no pool overhead, no thread contention)
3. **Fast iteration** — Run benchmark in 60 seconds, not 5 minutes of mining
4. **Validation** — Every run verifies bit-exact hashes (catch correctness bugs)
5. **Clean shutdown** — No GPU hangs, no memory leaks

**Current testing gaps:**

- `test-mine.sh` measures end-to-end pool mining (network latency, pool variance)
- `cn_gpu_harness` validates correctness but doesn't measure performance
- No tool for **isolated GPU performance measurement**

The benchmark harness fills that gap.

---

## Usage Examples (After Phase 1)

```bash
# Quick 60-second benchmark on AMD GPU 0
./tests/benchmark_harness --opencl --device 0 --duration 60

# Extended 5-minute run with JSON export
./tests/benchmark_harness --opencl --duration 300 --json baseline.json

# Compare two builds
./tests/benchmark_harness --opencl --json before.json
# <make code changes>
./tests/benchmark_harness --opencl --json after.json
diff before.json after.json

# Benchmark all available GPUs
./tests/benchmark_harness --all-devices --duration 120 --json multi-gpu.json
```

---

## Integration with Optimization Work

Once Phase 1 is complete, we can:

1. **Profile kernel hotspots** (rocprof/nvprof → identify slow phases)
2. **Test kernel changes** (modify Phase 3 → rebuild → benchmark → compare)
3. **Tune occupancy** (tweak block size → benchmark → find optimal)
4. **Optimize memory** (change scratchpad layout → benchmark → measure BW impact)

Every optimization gets **quantitative before/after data** instead of subjective "feels faster."

---

## Next Steps

**Priority 1:** Implement Phase 1 (kernel integration)
- Copy kernel loading logic from `n0s/backend/amd/amd_gpu/gpu.cpp:InitOpenCL()`
- Copy dispatch logic from `n0s/backend/amd/minethd.cpp:work_main()`
- Simplify for single-threaded, fixed-input benchmark

**Priority 2:** Validate against golden hashes (reuse `cn_gpu_harness` test vectors)

**Priority 3:** Add GPU timing (OpenCL events or vendor profiler)

**Priority 4:** Document results (baseline.json in docs/benchmarks/)
