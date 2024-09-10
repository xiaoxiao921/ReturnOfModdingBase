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
	enum class enabled_reason : uint16_t
	{
		NOT_INIT,

		DISABLED_BY_ENV_VAR,
		ENABLED_BY_ENV_VAR,

		DISABLED_BY_CMD_LINE,
		ENABLED_BY_CMD_LINE,

		DISABLED_BECAUSE_FIRST_ENABLED_LAUNCH_WAS_EXPLICIT_THROUGH_ENV_VAR_OR_CMD_LINE,
		ENABLED_BY_DEFAULT
	};

	inline enabled_reason g_first_enabled_reason = enabled_reason::NOT_INIT;
	inline enabled_reason g_enabled_reason       = enabled_reason::NOT_INIT;

	extern LPSTR* CommandLineToArgvA(LPCSTR cmd_line, int* argc);

	extern cxxopts::Options get_rom_cxx_options();

	extern bool is_rom_enabled();

	inline std::string g_project_name;
	inline std::string g_target_module_name;
	inline std::string g_lua_api_namespace;

	extern int32_t get_instance_id();
	extern std::string& get_instance_id_string();

	inline void init(std::string_view project_name, std::string_view target_module_name, std::string_view lua_api_namespace)
	{
		g_project_name       = project_name;
		g_target_module_name = target_module_name;
		g_lua_api_namespace  = lua_api_namespace;
	}
} // namespace rom
