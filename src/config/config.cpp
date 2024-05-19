#pragma once

#include "config.hpp"

#include <file_manager/file_manager.hpp>
#include <rom/rom.hpp>

namespace big::config
{
	std::unique_ptr<toml_v2::config_file> general_config = nullptr;

	void init_general()
	{
		general_config =
		    std::make_unique<toml_v2::config_file>((char*)g_file_manager
		                                               .get_project_file(std::format("config/{}-{}-General.cfg", rom::g_project_name, rom::g_project_name))
		                                               .get_path()
		                                               .u8string()
		                                               .c_str(),
		                                           true,
		                                           std::format("{}-{}", rom::g_project_name, rom::g_project_name));
	}

	toml_v2::config_file& general()
	{
		return *general_config;
	}
} // namespace big::config
