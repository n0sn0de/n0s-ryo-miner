#pragma once

#include <cstdlib>
#include <pwd.h>
#include <string>
#include <unistd.h>

namespace
{
inline std::string get_home()
{
	const char* home = ".";

	if((home = getenv("HOME")) == nullptr)
		home = getpwuid(getuid())->pw_dir;

	return home;
}
} // namespace
