/*
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program.  If not, see <http://www.gnu.org/licenses/>.
  */

#pragma once

#include "gpu.hpp"
#include <cstddef>
#include <cstdint>

namespace n0s
{
namespace amd
{

// Initialize a single OpenCL GPU context
// Returns 0 on success, ERR_OCL_API on failure
[[nodiscard]] size_t InitOpenCLGpu(cl_context opencl_ctx, GpuContext* ctx, const char* source_code);

// Update kernel runtime statistics
// Returns the measured runtime in milliseconds
uint64_t updateTimings(GpuContext* ctx, uint64_t t);

// Adjust interleave delay for multi-thread GPU mining
// Returns the timestamp after applying delay
uint64_t interleaveAdjustDelay(GpuContext* ctx, bool enableAutoAdjustment);

} // namespace amd
} // namespace n0s
