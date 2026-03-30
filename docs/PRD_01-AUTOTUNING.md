# PRD: Algorithm/Kernel Autotuning for `n0s-ryo-miner`

## Document status
Draft v1

## Owner
n0sn0de / miner core

## Summary
Add first-class **algorithm/kernel autotuning** to `n0s-ryo-miner` so the miner can discover stable, device-specific launch and workload parameters automatically, persist the results, and reuse them on later runs.

This feature is explicitly about **miner execution tuning**, not board-level overclocking or power management. The tuner should optimize runtime parameters such as OpenCL/CUDA workload shape, kernel launch geometry, and miner-specific execution settings while staying inside driver-enforced limits.

## Why now
The current miner already supports both AMD/OpenCL and NVIDIA/CUDA, generates backend-specific config files on first run, and exposes backend-specific manual settings:

- `amd.txt` for AMD GPU settings such as **intensity** and **worksize**
- `nvidia.txt` for NVIDIA GPU settings such as **threads**, **blocks**, and **bfactor**

That means the product already has the right configuration surface for manual tuning, but it lacks a built-in way to discover good values automatically per GPU, per driver/runtime stack, and per miner version.

Without autotune, users must:
- guess launch parameters
- copy values from unrelated GPU models
- manually benchmark multiple runs
- tolerate instability or leave performance on the table

Autotune should turn first-run setup into a guided, measurable process and make the miner much more competitive for mixed rigs, fresh installs, and new GPU generations.

## Current product context
This PRD is scoped to the repo as it exists today:

- `n0s-ryo-miner` is a **GPU-only** CryptoNight-GPU miner for RYO Currency.
- It supports **AMD via OpenCL** and **NVIDIA via CUDA**.
- It generates `pools.txt`, `amd.txt`, and `nvidia.txt` on first run.
- It includes an optional **HTTP monitoring API**.
- It already has local AMD mining tests, remote NVIDIA tests, a triple-GPU integration test, and a compile/build matrix.
- The build is split into a core backend library plus separately built CUDA and OpenCL backend libraries.

## Problem statement
Users need the miner to select **stable, high-performing kernel/runtime settings automatically** for the exact GPU(s) and software stack present on their machine.

Today, a user with a new GPU or driver stack has no built-in answer to:
- what `intensity` and `worksize` should be used on AMD
- what `threads`, `blocks`, and `bfactor` should be used on NVIDIA
- whether a setting that benchmarks well for a few seconds stays valid under real mining load
- whether a config should differ by GPU model, driver version, runtime version, or miner version

The absence of autotune increases setup friction, increases support burden, and makes the miner feel less polished than it should.

## Goals
1. **One-command tuning** for supported AMD and NVIDIA GPUs.
2. Discover **stable** settings, not just peak short-burst settings.
3. Persist tuned settings per GPU and reuse them automatically.
4. Integrate cleanly with existing `amd.txt` and `nvidia.txt` generation and loading.
5. Expose progress and results in logs and, when enabled, through the HTTP monitoring API.
6. Keep the implementation portable across the repo’s current AMD/OpenCL and NVIDIA/CUDA backend split.

## Non-goals
This feature does **not** include:
- voltage, clock, fan, or power-limit management
- BIOS strap editing or firmware flashing
- automatic undervolting or overclocking
- pool-side benchmarking logic
- tuning for algorithms other than the miner’s current CryptoNight-GPU focus

## User personas
### 1. First-run miner user
Wants the miner to “just find the right settings” and begin mining without manual trial and error.

### 2. Power user / farm operator
Wants reproducible tuning output, per-GPU persistence, logs, and CLI control over speed vs thoroughness.

### 3. Maintainer / release engineer
Wants deterministic behavior, robust failure handling, and test coverage that works with the current matrix and hardware smoke tests.

## User stories
- As a new user, I can run the miner with `--autotune` and get a working config without editing backend files manually.
- As a user with multiple GPUs, I can autotune one GPU at a time or all GPUs in sequence.
- As a user with mixed AMD and NVIDIA GPUs, I can autotune both backends in one workflow.
- As a power user, I can choose quick, balanced, or exhaustive tuning modes.
- As a maintainer, I can inspect a saved tuning artifact and understand how the final parameters were selected.
- As a user, I can resume a partially completed tune after interruption.

## Success metrics
### User-facing
- Reduce “manual config required before first useful mining run” from current baseline to near zero for most supported GPUs.
- 90%+ of successful autotune runs produce a stable config without manual edits.
- Tuned config improves sustained accepted hashrate versus default/generated config on the same machine.

### Engineering
- No regression to normal mining when autotune is unused.
- Hardware tests remain green for existing local AMD, remote NVIDIA, and triple-GPU paths.
- Compile matrix remains green with autotune disabled and enabled at build time.

## Product requirements

### 1. Entry points
The miner must support:

- `--autotune`
- `--autotune-mode quick|balanced|exhaustive`
- `--autotune-backend amd|nvidia|all`
- `--autotune-gpu <index>[,<index>...]`
- `--autotune-reset`
- `--autotune-benchmark-seconds <n>`
- `--autotune-stability-seconds <n>`
- `--autotune-target hashrate|efficiency|balanced`
- `--autotune-resume`
- `--autotune-export <path>`

Default mode should be `balanced`.

### 2. Tuning scope
#### AMD/OpenCL
The first release must autotune at least:
- `intensity`
- `worksize`

Optional later expansion:
- multi-thread or multi-kernel launch variants if the AMD backend meaningfully supports them
- backend-specific chunking or inner-loop parameters if exposed internally

#### NVIDIA/CUDA
The first release must autotune at least:
- `threads`
- `blocks`
- `bfactor`

Optional later expansion:
- `bsleep` or similar throttling/scheduling knobs if present or added
- backend-specific kernel variants if there are multiple viable launch strategies

### 3. Device fingerprinting
The tuner must identify and persist results against a device/runtime fingerprint including:
- backend type
- GPU vendor/model name
- compute capability for NVIDIA or OpenCL device capabilities for AMD
- VRAM size
- driver/runtime version
- miner version / git commit if available
- algorithm identifier

A tuning result must be reused only when the fingerprint still matches within defined compatibility rules.

### 4. Search strategy
The tuner must use a **bounded, staged search** rather than brute-forcing an unbounded parameter space.

Required phases:
1. **Device detect and legal-range generation**
2. **Coarse search** over plausible ranges
3. **Local refinement** around top candidates
4. **Stability validation** on finalists
5. **Persistence** of best stable result

The search space must be generated using backend-aware heuristics. Example principles:
- avoid work sizes that violate runtime constraints
- avoid settings that exceed available VRAM or per-kernel resource limits
- prefer architecture-appropriate candidate sets rather than every mathematically possible value

### 5. Scoring model
The tuner must optimize for **steady accepted throughput**, not instantaneous raw kernel throughput.

Required score inputs:
- average hashrate over benchmark window
- accepted shares / valid share rate, if available during the window
- kernel execution success/failure
- backend error count
- restart/watchdog events
- stale/invalid result penalties
- optional power metric if available via monitoring API or backend telemetry, only when target is `efficiency` or `balanced`

Recommended score rule:
- begin with steady-state hashrate
- apply severe penalties for invalid shares, kernel failures, restarts, or watchdog events
- reject candidates that exceed guardrails rather than merely penalizing them

### 6. Stability guardrails
Autotune must protect the normal mining session from bad candidates.

It must detect and handle:
- kernel launch failure
- OpenCL/CUDA runtime errors
- invalid or corrupt result output
- repeated stale/invalid shares
- miner backend initialization failure
- GPU disappearing or resetting during the test
- benchmark windows that never reach valid work production

A failed candidate must be marked bad and not retried in the same run unless the user explicitly forces full retry.

### 7. Runtime isolation
The tuner should isolate risky candidate evaluation from the long-lived mining session as much as practical.

Preferred implementation:
- evaluate candidate configs in a controlled child-runner mode or short-lived worker process
- if full process isolation is too large for v1, at minimum isolate candidate state and ensure bad candidate cleanup is reliable before the next attempt

### 8. Warm-up handling
The tuner must distinguish between:
- startup/compilation/warm-up time
- real benchmark time

This is especially important for:
- first OpenCL kernel compilation and cache creation
- CUDA JIT and backend initialization
- network and pool connection ramp-up

The benchmark window must begin only after the miner reaches an initialized, actively hashing state.

### 9. Persistence and config writing
The feature must save:
1. final backend parameters into existing `amd.txt` and/or `nvidia.txt`
2. tuning metadata into a separate artifact such as `autotune.json`

`autotune.json` should include:
- device fingerprint
- mode used
- candidate history summary
- final winner
- benchmark/stability durations
- miner version / git commit
- timestamp
- whether the run completed successfully or was resumed

The miner must support reading existing tuned configs and skipping re-tune unless:
- no tuned result exists
- the fingerprint changed materially
- the user passes `--autotune-reset`
- the user passes `--autotune` with `--autotune-resume` or equivalent override behavior

### 10. Multi-GPU behavior
For multiple GPUs, v1 must support:
- sequential tuning per GPU
- per-GPU result persistence
- mixed-rig tuning across AMD and NVIDIA

Out of scope for v1:
- simultaneous cross-GPU autotune that attempts to globally optimize all devices at once under shared thermal or PCIe pressure

### 11. UX and observability
The miner must print clear progress logs:
- device discovery
- candidate being tested
- benchmark phase transitions
- current best candidate
- reason for candidate rejection
- final selected config

When the HTTP monitoring API is enabled, it should expose:
- autotune active/inactive
- current device being tuned
- candidate progress counters
- current best score
- final selected settings when complete

### 12. Defaults
Default user behavior should be:
- if no backend config exists, the miner may prompt or log a suggestion to run autotune
- if `--autotune` is set with no additional flags, run balanced mode for all detected GPUs
- reuse tuned settings on later runs unless device/runtime fingerprint changed materially

## Functional design requirements

### Candidate generation rules
#### AMD candidate families
Start with a bounded search based on:
- OpenCL max workgroup size
- device local memory constraints
- VRAM size
- known-safe worksize families such as powers of two up to runtime limits
- intensity ranges derived from memory pressure and device class

#### NVIDIA candidate families
Start with a bounded search based on:
- compute capability
- SM count if available
- VRAM size
- likely good thread/block shapes for the architecture family
- conservative `bfactor` defaults then refinement around top candidates

### Benchmark protocol
For each candidate:
1. initialize backend
2. warm up until hashing is active
3. discard warm-up samples
4. benchmark for configured duration
5. collect metrics
6. score candidate
7. either keep, refine around, or reject

### Stability validation protocol
For finalists:
- run longer than the benchmark window
- require no backend errors
- require no invalid-share anomaly
- require throughput not to collapse versus short benchmark expectations

### Resume behavior
If autotune is interrupted:
- partial progress must be recoverable from `autotune.json`
- resume should skip already failed candidates unless explicitly requested otherwise
- resume should continue from the latest completed phase where possible

## Suggested architecture for implementation
This is still a PRD, but the feature should align to the repo’s current shape.

### New modules
Suggested additions:
- `n0s/autotune/` common orchestration, scoring, persistence
- `n0s/autotune/autotune_manager.*`
- `n0s/autotune/autotune_state.*`
- `n0s/autotune/autotune_score.*`
- `n0s/backend/amd/autotune_amd.*`
- `n0s/backend/nvidia/autotune_nvidia.*`

### Integration points
- CLI parsing in `n0s/cli/`
- config read/write in existing config generation/loading paths
- HTTP exposure in `n0s/http/`
- backend launch adapters in AMD and NVIDIA backend folders

## Required logging fields
Each candidate attempt should log at minimum:
- device index
- backend
- candidate parameter set
- benchmark duration
- average hashrate
- valid/invalid results if available
- error state
- final score
- reject reason, if rejected

## Failure-handling requirements
The miner must:
- fall back to existing generated/manual config if autotune fails completely
- never write a known-bad config as the active winner
- preserve the previous known-good config until a new candidate is validated
- clearly distinguish “autotune incomplete” from “autotune successful”

## Compatibility requirements
- Must work when only AMD backend is built.
- Must work when only NVIDIA backend is built.
- Must work when both backends are built.
- Must not require the HTTP API to be enabled.
- Must not require containerized builds.
- Must compile cleanly under the repo’s existing build matrix assumptions.

## Security and safety
- No privileged hardware clock/power operations.
- No writing outside the miner’s config/artifact paths.
- No network behavior changes beyond normal mining activity.
- No shelling out to vendor tools as part of v1 autotune.

## Test plan
### Unit tests
Add tests for:
- candidate generation legality
- score calculation
- persistence and resume behavior
- fingerprint compatibility rules
- bad-candidate rejection logic

### Integration tests
Extend current test coverage with:
- local AMD autotune smoke test
- remote NVIDIA autotune smoke test
- mixed-rig autotune integration test
- compile-only coverage with autotune code paths enabled

### Regression tests
Ensure:
- existing manual `amd.txt`/`nvidia.txt` workflows still function
- normal mining startup path is unchanged when autotune is not requested
- HTTP API still works when autotune is disabled

## Acceptance criteria
### v1 acceptance
1. User can run `n0s-ryo-miner --autotune` and receive persisted settings for detected GPUs.
2. AMD autotune produces `intensity` and `worksize` values and writes them to `amd.txt`.
3. NVIDIA autotune produces `threads`, `blocks`, and `bfactor` values and writes them to `nvidia.txt`.
4. A separate tuning artifact is written with fingerprint and benchmark metadata.
5. Re-running the miner reuses the tuned result when fingerprint is compatible.
6. Existing mine tests still pass.
7. Failed candidates do not poison the active config.
8. Logs clearly show tuning progress and final selection.

### v1.1 acceptance
1. Resume support survives interrupted runs.
2. HTTP API exposes tune status.
3. Quick/balanced/exhaustive modes are fully implemented and documented.

### v2 ideas
- backend-specific deeper parameter search
- optional efficiency mode with vendor telemetry inputs
- online revalidation after driver/miner upgrades
- fleet export/import of tuned profiles

## Rollout plan
### Phase 1
Backend-agnostic framework, persistence, scoring, CLI, and one backend prototype.

### Phase 2
Full AMD + NVIDIA support for the first required parameter sets.

### Phase 3
HTTP API exposure, resume support, and stronger integration tests.

### Phase 4
Refinement of heuristics based on real miner data and user feedback.

## Open questions
1. Should benchmark scoring use pool-side accepted shares only, local hash verification only, or both?
2. Should autotune run against a real pool by default, or should there be a local benchmark mode that does not require a live pool?
3. What exact fingerprint changes should invalidate a cached tune: miner commit, driver version, runtime version, or only major architecture/runtime changes?
4. How much of candidate evaluation should be isolated into a child process in v1 versus v1.1?
5. Should the first-run experience auto-suggest autotune, or should it stay fully opt-in?

## Recommended v1 decisions
- Keep autotune **opt-in** via CLI for the initial release.
- Optimize for **steady hashrate with strict stability rejection**, not peak synthetic benchmark results.
- Use a **separate `autotune.json`** artifact rather than overloading `amd.txt` or `nvidia.txt` with metadata.
- Run **sequential per-GPU tuning** in v1.
- Reuse current backend-specific config surfaces instead of inventing a new unified active config format.

## Appendix: repository-aligned assumptions
This PRD assumes and preserves the following repo characteristics:
- GPU-only miner, no CPU mining mode
- AMD backend via OpenCL
- NVIDIA backend via CUDA
- existing first-run config generation for `pools.txt`, `amd.txt`, and `nvidia.txt`
- optional HTTP monitoring API
- local AMD test, remote NVIDIA test, mixed integration test, and compile matrix workflows
- separate core, CUDA, and OpenCL build outputs

