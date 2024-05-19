#pragma once

#include "toml_v2/config_file.hpp"

namespace big::config
{
	extern std::unique_ptr<toml_v2::config_file> general_config;

	void init_general();

	toml_v2::config_file& general();
} // namespace big::config
