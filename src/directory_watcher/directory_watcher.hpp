#pragma once

#include <filesystem>
#include <vector>

namespace big
{
	class directory_watcher
	{
	public:
		explicit directory_watcher(const std::filesystem::path& path);
		~directory_watcher();

		std::vector<std::filesystem::path> check();

	private:
		std::filesystem::path _path;
		std::vector<uint8_t> _buffer;
		void* _file_handle;
		void* _completion_handle;
	};
} // namespace big
