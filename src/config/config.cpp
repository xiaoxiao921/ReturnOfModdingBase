#pragma once

#include "config.hpp"

#include <file_manager/file_manager.hpp>
#include <rom/rom.hpp>

namespace big::config
{
	std::shared_ptr<toml_v2::config_file> general = nullptr;

	void init_general()
	{
		general = std::make_shared<toml_v2::config_file>((char*)g_file_manager
		                                                     .get_project_file(std::format("config/{}-{}-General.cfg", rom::g_project_name, rom::g_project_name))
		                                                     .get_path()
		                                                     .u8string()
		                                                     .c_str(),
		                                                 true,
		                                                 std::format("{}-{}", rom::g_project_name, rom::g_project_name));
	}
} // namespace big::config
