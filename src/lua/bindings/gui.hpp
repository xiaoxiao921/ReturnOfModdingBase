#pragma once
#include "lua/lua_module.hpp"

namespace lua::gui
{
	extern std::function<bool()> g_on_is_open;
	extern std::function<void()> g_on_toggle;

	void bind(sol::table& state);
} // namespace lua::gui
