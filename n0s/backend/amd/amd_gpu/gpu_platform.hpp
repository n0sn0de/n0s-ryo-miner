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
#include <cstdint>
#include <vector>

namespace n0s
{
namespace amd
{

// Get number of available OpenCL platforms
[[nodiscard]] uint32_t getNumPlatforms();

// Get AMD platform index (or NVIDIA if configured)
// Returns -1 if no suitable platform found
[[nodiscard]] int getAMDPlatformIdx();

// Get all AMD/NVIDIA devices for the specified platform index
[[nodiscard]] std::vector<GpuContext> getAMDDevices(int index);

} // namespace amd
} // namespace n0s
