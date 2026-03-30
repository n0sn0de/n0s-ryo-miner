#include "environment.hpp"

#include "n0s/misc/console.hpp"
#include "n0s/backend/cpu/crypto/cryptonight.h"
#include "n0s/params.hpp"
#include "n0s/misc/executor.hpp"
#include "n0s/jconf.hpp"

namespace n0s
{
void environment::init_singeltons()
{
	printer::inst();
	globalStates::inst();
	jconf::inst();
	executor::inst();
	params::inst();
}
}
