# Autotune

Autotune searches for GPU parameters that perform well on the current hardware and driver stack.

## Quick Start

```bash
./build/bin/n0s-ryo-miner --autotune
```

Common variants:

```bash
./build/bin/n0s-ryo-miner --autotune --autotune-mode quick
./build/bin/n0s-ryo-miner --autotune --autotune-mode balanced
./build/bin/n0s-ryo-miner --autotune --autotune-backend amd
./build/bin/n0s-ryo-miner --autotune --autotune-backend nvidia
./build/bin/n0s-ryo-miner --autotune --autotune-reset
```

## Flags

| Flag | Description |
|---|---|
| `--autotune` | Enable autotune mode |
| `--autotune-mode quick|balanced|exhaustive` | Search depth |
| `--autotune-backend amd|nvidia|all` | Select GPU backend |
| `--autotune-gpu 0,1` | Tune specific GPU indexes |
| `--autotune-reset` | Ignore cached results |
| `--autotune-resume` | Resume interrupted tuning |
| `--autotune-benchmark-seconds N` | Per-candidate benchmark duration |
| `--autotune-stability-seconds N` | Winner validation duration |
| `--autotune-target hashrate|efficiency|balanced` | Optimization goal |
| `--autotune-export PATH` | Write results to a selected path |

## Output

Autotune writes backend settings to:

- `amd.txt`
- `nvidia.txt`
- `autotune.json`

The cache is keyed by GPU fingerprint. Driver updates or hardware changes should trigger a fresh tune.

## How It Works

1. Discover GPUs.
2. Generate safe candidate settings.
3. Run isolated benchmark subprocesses.
4. Reject unstable or invalid candidates.
5. Validate the winner for a longer interval.
6. Save config for normal mining.

The subprocess model keeps failed GPU experiments from poisoning the main autotune process.
