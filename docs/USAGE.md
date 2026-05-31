# Usage Guide

## Benchmark Mode

Benchmark without connecting to a pool:

```bash
./build/bin/n0s-ryo-miner --benchmark 10 --benchmark-json benchmark.json
```

Useful backend filters:

```bash
./build/bin/n0s-ryo-miner --noNVIDIA --benchmark 10
./build/bin/n0s-ryo-miner --noAMD --benchmark 10
```

Kernel timing profile:

```bash
./build/bin/n0s-ryo-miner --benchmark 10 --profile
```

## Pool Mining

```bash
./build/bin/n0s-ryo-miner -o pool.example.com:3333 -u RYO_WALLET_OR_LOGIN -p x
```

The first run creates config files in the current working directory. Review them before long-running mining.

## Runtime Files

| File | Purpose |
|---|---|
| `config.txt` | General miner settings |
| `pools.txt` | Pool list and credentials |
| `amd.txt` | AMD OpenCL GPU tuning |
| `nvidia.txt` | NVIDIA CUDA GPU tuning |
| `autotune.json` | Cached autotune results |

## Pool Config Shape

```json
{
  "pool_list": [
    {
      "pool_address": "pool.example.com:3333",
      "wallet_address": "RYO_WALLET_OR_LOGIN",
      "rig_id": "rig-1",
      "pool_password": "x",
      "use_nicehash": false,
      "use_tls": false,
      "tls_fingerprint": "",
      "pool_weight": 1
    }
  ]
}
```

## Memory Notices

`MEMORY NOTICE` messages are non-fatal. They usually mean the miner could not enable huge pages or lock a small CPU-side buffer. For a GPU-focused miner this is normally a startup optimization miss, not a major hashrate failure.

To silence the huge-page attempt, set:

```json
"use_slow_memory": "always"
```

To require huge pages or locked memory, set:

```json
"use_slow_memory": "never"
```

Then fix the OS privileges until startup succeeds.
