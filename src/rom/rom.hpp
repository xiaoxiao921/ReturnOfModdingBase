#pragma once

#include <string>
#include <string_view>

namespace rom
{
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
