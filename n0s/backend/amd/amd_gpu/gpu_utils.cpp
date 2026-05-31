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

#include "gpu_utils.hpp"

#include "n0s/platform/compat.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace n0s
{
namespace amd
{

void string_replace_all(std::string& str, const std::string& from, const std::string& to)
{
	size_t pos = 0;
	while((pos = str.find(from, pos)) != std::string::npos)
	{
		str.replace(pos, from.length(), to);
		pos += to.length();
	}
}

void create_directory(const std::string& dirname)
{
	n0s::compat::mkdir(dirname.data());
}

void port_sleep(size_t sec)
{
	n0s::compat::sleep_sec(static_cast<unsigned int>(sec));
}

char* LoadTextFile(const char* filename)
{
	size_t flen;
	char* out;
	FILE* kernel = fopen(filename, "rb");

	if(kernel == nullptr)
		return nullptr;

	fseek(kernel, 0, SEEK_END);
	flen = ftell(kernel);
	fseek(kernel, 0, SEEK_SET);

	out = static_cast<char*>(malloc(flen + 1));
	size_t r = fread(out, flen, 1, kernel);
	fclose(kernel);

	if(r != 1)
	{
		free(out);
		return nullptr;
	}

	out[flen] = '\0';
	return out;
}

} // namespace amd
} // namespace n0s
