# n0s-ryo-miner

GPU miner for [RYO Currency](https://ryo-currency.com) using the CryptoNight-GPU algorithm.

This is a modernized fork of [`xmr-stak`](https://github.com/fireice-uk/xmr-stak) focused on the AMD OpenCL and NVIDIA CUDA paths. The current build produces one executable per target with the web UI embedded; it does not use separate runtime backend shared libraries.

## Status

- Linux AMD OpenCL: build and benchmark path supported.
- Linux NVIDIA CUDA: build and benchmark path supported.
- Windows NVIDIA CUDA/OpenCL: native MSVC build path supported.
- Windows OpenCL cross-build from Linux: compile-only convenience path.
- Windows AMD OpenCL runtime validation is not currently claimed.

## Quick Start

```bash
cmake -S . -B build \
  -DCUDA_ENABLE=OFF \
  -DOpenCL_ENABLE=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DN0S_COMPILE=generic

cmake --build build -j"$(nproc)"
./build/bin/n0s-ryo-miner --version
```

Run a benchmark:

```bash
./build/bin/n0s-ryo-miner --benchmark 10 --benchmark-json benchmark.json
```

Mine against a pool:

```bash
./build/bin/n0s-ryo-miner -o pool.example.com:3333 -u RYO_WALLET_OR_LOGIN -p x
```

## Docs

- [Build Guide](docs/BUILD.md)
- [Usage Guide](docs/USAGE.md)
- [Autotune](docs/AUTOTUNE.md)
- [HTTP API](docs/API.md)
- [Security Notes](docs/SECURITY.md)

## Common Options

| Flag | Description |
|---|---|
| `-o host:port` | Pool address |
| `-u wallet` | Wallet address or pool login |
| `-p password` | Pool password, often `x` |
| `--noAMD` | Disable AMD OpenCL backend |
| `--noNVIDIA` | Disable NVIDIA CUDA backend |
| `--benchmark N` | Run benchmark mode for block version `N` |
| `--benchmark-json FILE` | Write benchmark results to JSON |
| `--autotune` | Discover GPU settings automatically |
| `--profile` | Print per-kernel timings with benchmark mode |

## Configuration Files

The miner creates runtime config files in the working directory:

- `pools.txt`
- `amd.txt`
- `nvidia.txt`
- `config.txt`
- `autotune.json`

Do not commit real wallet, pool, rig, or API credentials from local config files.

## License

This project is licensed under GPLv3 because it is derived from xmr-stak.

The codebase has been heavily refactored and renamed, but the fork lineage
still matters. See:

- [LICENSE](LICENSE)
- [NOTICE](NOTICE)
- [THIRD-PARTY-LICENSES](THIRD-PARTY-LICENSES)
