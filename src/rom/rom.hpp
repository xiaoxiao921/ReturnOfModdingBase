#pragma once

#include <string>
#include <string_view>

namespace rom
{
	inline std::string g_project_name;

	inline void init(std::string_view project_name)
	{
		g_project_name = project_name;
	}
} // namespace rom
