/**
 * cuda_device.hpp — CUDA device management
 *
 * Device enumeration, capability checking, and memory initialization.
 */
#pragma once

#include "cuda_context.hpp"
#include "n0s/backend/cryptonight.hpp"

#ifdef __cplusplus
extern "C" {
#endif

/** Get number of CUDA devices
 *
 * @param deviceCount Output: number of devices found
 * @return 1 on success, 0 on error
 */
int cuda_get_devicecount(int* deviceCount);

/** Get device information and validate compatibility
 *
 * @param ctx NVIDIA context (device_id must be set)
 * @return 0 = all OK,
 *         1 = general error (driver/device query failed),
 *         2 = gpu cannot be selected,
 *         3 = context cannot be created,
 *         4 = not enough memory,
 *         5 = architecture not supported (not compiled for the gpu architecture)
 */
int cuda_get_deviceinfo(nvid_ctx* ctx);

/** Initialize CUDA device and allocate mining memory
 *
 * @param ctx NVIDIA context (device_id and config must be set)
 * @return 1 on success, 0 on error
 */
int cryptonight_extra_cpu_init(nvid_ctx* ctx);

#ifdef __cplusplus
}
#endif
