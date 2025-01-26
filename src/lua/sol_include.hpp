#pragma once

#define SOL_ALL_SAFETIES_ON           1
#define SOL_NO_CHECK_NUMBER_PRECISION 1

#ifdef LUA_USE_LUAJIT
	#define SOL_LUAJIT 1
#endif

#include "sol/sol.hpp"
