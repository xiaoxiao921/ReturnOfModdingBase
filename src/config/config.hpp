#pragma once

#include "toml_v2/config_file.hpp"

namespace big::config
{
	extern std::shared_ptr<toml_v2::config_file> general;

	void init_general();
} // namespace big::config
