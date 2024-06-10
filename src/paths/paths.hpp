#pragma once

#include <cxxopts.hpp>
// clang-format off
#include <windows.h>
// clang-format on
#include <shellapi.h>

namespace big::paths
{
	extern std::filesystem::path get_main_module_folder();
	extern std::filesystem::path get_project_root_folder();

	extern void init_dump_file_path();
	extern const std::filesystem::path& remove_and_get_dump_file_path();
} // namespace big::paths
