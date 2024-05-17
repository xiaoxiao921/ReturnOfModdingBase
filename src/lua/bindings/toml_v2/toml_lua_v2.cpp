#pragma once
#include "toml_lua_v2.hpp"

namespace lua::toml_lua_v2
{
	void bind(sol::table& state)
	{
		sol::table ns = state.create_named("toml_v2");
	}
} // namespace lua::toml_lua_v2
