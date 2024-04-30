#pragma once

#include "lua/sol_include.hpp"

namespace lua::log
{
	void bind(sol::state_view& state, sol::table& lua_ext);
}
