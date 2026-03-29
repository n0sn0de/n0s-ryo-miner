# NVIDIA Build Broken with CUDA 12.6

## Issue
CUDA backend fails to compile with CUDA 12.6 on Ubuntu 22.04 (nos2).

## Errors
```
error: identifier "int2float" is undefined
error: identifier "float_as_int" is undefined
error: identifier "int_as_float" is undefined
```

These functions are used in:
- `cuda_cryptonight_gpu.hpp` (lines 134, 263, 272, 281)
- `cuda_core.cu` (lines 510, 581, 586)

## Root Cause
Old CUDA helper functions that were removed/renamed in CUDA 12.x. The xmr-stak CUDA code was written for CUDA 7-10 era.

## Additional Issues
- Default CUDA architectures include compute_30 (dropped in CUDA 12.x)
- Need to update CMakeLists.txt default: `30;35;37;50;52` → `50;52;60;61;70;75;80`

## Workarounds
1. **Downgrade to CUDA 11.8** on nos2
2. **Fix the code** — define missing functions or use modern equivalents:
   ```cuda
   __device__ __forceinline__ float int_as_float(int x) { return __int_as_float(x); }
   __device__ __forceinline__ int float_as_int(float x) { return __float_as_int(x); }
   __device__ __forceinline__ float int2float(int x) { return __int2float_rn(x); }
   ```

## Testing Status
- **AMD (OpenCL):** ✅ Works (RX 9070 XT on nitro)
- **NVIDIA (CUDA):** ❌ Broken (GTX 1070 Ti on nos2 with CUDA 12.6)
- **CPU:** Not tested (will be removed — cn_gpu is GPU-only)

## Action Plan
1. **Short term:** Test only AMD for rebrand/cleanup work
2. **Medium term:** Fix CUDA code for modern CUDA 12.x compatibility
3. **Long term:** CI/CD with both AMD and NVIDIA testing

## References
- nos2: GTX 1070 Ti, driver 580.126.09, CUDA 12.6
- Test script: `./test-mine-remote.sh`
