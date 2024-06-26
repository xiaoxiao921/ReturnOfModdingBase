#pragma once
#include "paths.hpp"

#include "lua/lua_manager.hpp"

#include <windows.h>

namespace lua::paths
{
	// Lua API: Table
	// Name: paths
	// Table containing helpers for retrieving project related IO file/folder paths.

	// Lua API: Function
	// Table: paths
	// Name: config
	// Returns: string: Returns the config folder path
	// Used for data that must persist between sessions and that can be manipulated by the user.
	static std::string config()
	{
		return (char*)big::g_lua_manager->m_config_folder.get_path().u8string().c_str();
	}

	// Lua API: Function
	// Table: paths
	// Name: plugins_data
	// Returns: string: Returns the plugins_data folder path
	// Used for data that must persist between sessions but not be manipulated by the user.
	static std::string plugins_data()
	{
		return (char*)big::g_lua_manager->m_plugins_data_folder.get_path().u8string().c_str();
	}

	// Lua API: Function
	// Table: paths
	// Name: plugins
	// Returns: string: Returns the plugins folder path
	// Location of .lua, README, manifest.json files.
	static std::string plugins()
	{
		return (char*)big::g_lua_manager->m_plugins_folder.get_path().u8string().c_str();
	}

	void bind(sol::table& state)
	{
		auto ns            = state.create_named("paths");
		ns["config"]       = config;
		ns["plugins_data"] = plugins_data;
		ns["plugins"]      = plugins;
	}
} // namespace lua::paths
