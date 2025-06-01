#pragma once

#include "toml_v2/config_file.hpp"

namespace big::config
{
	extern std::unique_ptr<toml_v2::config_file> general;

	void init_general();
} // namespace big::config
