#pragma once

#include <ankerl/unordered_dense.h>
#include <filesystem>
#include <string>
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

		std::vector<std::filesystem::path> check();

	private:
		std::filesystem::path _path;
		std::vector<uint8_t> _buffer;
		void* _file_handle;
		HANDLE _completion_handle;
		OVERLAPPED _overlapped;
		ankerl::unordered_dense::set<std::wstring> _last_modifications;

		void close_handles();
	};
} // namespace big
