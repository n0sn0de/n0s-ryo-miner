/**
 * cuda_kernels.cu — CUDA kernel orchestration (compatibility shim)
 *
 * This file has been refactored into focused modules:
 *
 *   cuda_device.cu/hpp       — Device management (init, enumeration)
 *   cuda_phase1.cu/hpp       — Phase 1 kernel (Keccak + AES key expansion)
 *   cuda_phase4_5.cu/hpp     — Phase 4 + 5 kernels (implode + finalize)
 *   cuda_dispatch.cu/hpp     — Host-side kernel dispatch
 *
 * This file now serves as a compatibility include for any code that
 * still includes cuda_kernels.cu directly. All functionality has moved
 * to the modular files above.
 *
 * Phase 2 and 3 kernels remain in cuda_cryptonight_gpu.hpp (already modular).
 */

#include "cuda_device.hpp"
#include "cuda_phase1.hpp"
#include "cuda_phase4_5.hpp"
#include "cuda_dispatch.hpp"
