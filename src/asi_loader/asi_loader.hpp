#pragma once

#include <Windows.h>

#include <filesystem>
#include <functional>

namespace big::asi_loader
{
	void init(const std::filesystem::path& base_folder_path, std::function<bool(const std::filesystem::path&)> pred);
}
