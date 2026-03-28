# Container Build Tests

Containerfiles for testing n0s-cngpu builds across Ubuntu LTS versions.

## Variants

### CPU-only builds
| File | Distro | GCC | CMake |
|------|--------|-----|-------|
| `Containerfile.bionic` | Ubuntu 18.04 | 7 | 3.10 |
| `Containerfile.focal` | Ubuntu 20.04 | 9 | 3.16 |
| `Containerfile.jammy` | Ubuntu 22.04 | 11/12 | 3.22 |
| `Containerfile.noble` | Ubuntu 24.04 | 13/14 | 3.28 |

### OpenCL builds (compile-only, no GPU required)
| File | Distro | Notes |
|------|--------|-------|
| `Containerfile.jammy-opencl` | Ubuntu 22.04 | ocl-icd + headers |
| `Containerfile.noble-opencl` | Ubuntu 24.04 | ocl-icd + headers |

## Usage

Run all tests:
```bash
./scripts/test-all-distros.sh --all
```

Test a single distro:
```bash
./scripts/test-all-distros.sh --distro jammy
```

See `./scripts/test-all-distros.sh --help` for all options.

## What's tested

Each build verifies:
1. Source compiles cleanly with the distro's default toolchain
2. Binary `bin/n0s-cngpu` exists
3. `--help` output contains "n0s-cngpu"
4. `--version` output contains "n0s-cngpu" and "1.0.0"
