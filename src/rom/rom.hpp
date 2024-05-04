#pragma once

#include <cxxopts.hpp>
#include <string>
#include <string_view>
// clang-format off
#include <windows.h>
// clang-format on
#include <shellapi.h>

namespace rom
{
	extern LPSTR* CommandLineToArgvA(LPCSTR cmd_line, int* argc);

	extern cxxopts::Options get_rom_cxx_options();

	extern bool is_rom_enabled();

	inline std::string g_project_name;
	inline std::string g_target_module_name;
	inline std::string g_lua_api_namespace;

	inline void init(std::string_view project_name, std::string_view target_module_name, std::string_view lua_api_namespace)
	{
		g_project_name       = project_name;
		g_target_module_name = target_module_name;
		g_lua_api_namespace  = lua_api_namespace;
	}
} // namespace rom
