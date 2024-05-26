#pragma once

#include <filesystem>
#include <vector>
#include <Windows.h>

namespace big
{
	class directory_watcher
	{
	public:
		explicit directory_watcher(const std::filesystem::path& path);
		~directory_watcher();

		// Delete copy constructor and copy assignment operator
		directory_watcher(const directory_watcher&)            = delete;
		directory_watcher& operator=(const directory_watcher&) = delete;

		// Implement move constructor and move assignment operator
		directory_watcher(directory_watcher&& other) noexcept;
		directory_watcher& operator=(directory_watcher&& other) noexcept;

		std::vector<std::filesystem::path> check() const;

	private:
		std::filesystem::path _path;
		std::vector<uint8_t> _buffer;
		void* _file_handle;
		HANDLE _completion_handle;
		OVERLAPPED _overlapped;

		void close_handles();
	};
} // namespace big
