#pragma once

#include "n0s/platform/platform.hpp"
#include <string>

namespace
{
inline std::string get_home()
{
	return n0s::platform::getHomePath();
}
} // namespace
