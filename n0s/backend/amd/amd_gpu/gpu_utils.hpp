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

#include <string>

namespace n0s
{
namespace amd
{

// String utility: replace all occurrences of 'from' with 'to' in str
void string_replace_all(std::string& str, const std::string& from, const std::string& to);

// Create directory with default permissions (0744)
void create_directory(const std::string& dirname);

// Sleep for specified seconds
void port_sleep(size_t sec);

// Load text file into heap-allocated buffer (caller must free())
// Returns nullptr on failure
[[nodiscard]] char* LoadTextFile(const char* filename);

} // namespace amd
} // namespace n0s
