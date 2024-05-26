#include "directory_watcher.hpp"

#include <AsyncLogger/Logger.hpp>
#include <filesystem>
#include <stdexcept>
#include <vector>
#include <Windows.h>

// clang-format off
using namespace al;
// clang-format on
#undef ERROR

namespace big
{
	directory_watcher::directory_watcher(const std::filesystem::path& path) :
	    _path(path),
	    _buffer(sizeof(FILE_NOTIFY_INFORMATION) + MAX_PATH * sizeof(WCHAR) * 1024)
	{
		_file_handle = CreateFileW(path.wstring().c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
		if (_file_handle == INVALID_HANDLE_VALUE)
		{
			LOG(ERROR) << "Failed to create file handle: " << GetLastError();
		}

		// Associate the directory handle with the IO completion port
		_completion_handle = CreateIoCompletionPort(_file_handle, NULL, (uintptr_t)_file_handle + 1, 0);
		if (!_completion_handle)
		{
			LOG(ERROR) << "Failed to associate file handle with IO completion port: " << GetLastError();
			CloseHandle(_file_handle);
			CloseHandle(_completion_handle);
		}

		// Initialize the OVERLAPPED structure and start monitoring the directory
		memset(&_overlapped, 0, sizeof(_overlapped));
		DWORD buffer_length;
		if (!ReadDirectoryChangesW(_file_handle, _buffer.data(), static_cast<DWORD>(_buffer.size()), FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE, &buffer_length, &_overlapped, NULL))
		{
			LOG(ERROR) << "ReadDirectoryChangesW failed: " << GetLastError();
			CloseHandle(_file_handle);
			CloseHandle(_completion_handle);
		}
	}

	directory_watcher::~directory_watcher()
	{
		close_handles();
	}

	directory_watcher::directory_watcher(directory_watcher&& other) noexcept :
	    _path(std::move(other._path)),
	    _buffer(std::move(other._buffer)),
	    _file_handle(other._file_handle),
	    _completion_handle(other._completion_handle),
	    _overlapped(other._overlapped)
	{
		other._file_handle       = INVALID_HANDLE_VALUE;
		other._completion_handle = nullptr;
		memset(&other._overlapped, 0, sizeof(other._overlapped));
	}

	directory_watcher& directory_watcher::operator=(directory_watcher&& other) noexcept
	{
		if (this != &other)
		{
			close_handles();

			_path              = std::move(other._path);
			_buffer            = std::move(other._buffer);
			_file_handle       = other._file_handle;
			_completion_handle = other._completion_handle;
			_overlapped        = other._overlapped;

			other._file_handle       = INVALID_HANDLE_VALUE;
			other._completion_handle = nullptr;
			memset(&other._overlapped, 0, sizeof(other._overlapped));
		}
		return *this;
	}

	void directory_watcher::close_handles()
	{
		if (_file_handle != INVALID_HANDLE_VALUE)
		{
			CancelIo(_file_handle);
			CloseHandle(_file_handle);
			LOG(INFO) << "File handle closed successfully.";
		}
		if (_completion_handle)
		{
			CloseHandle(_completion_handle);
			LOG(INFO) << "Completion handle closed successfully.";
		}
	}

	std::vector<std::filesystem::path> directory_watcher::check()
	{
		DWORD transferred         = 0;
		ULONG_PTR key             = 0;
		LPOVERLAPPED lpOverlapped = nullptr;

		std::vector<std::filesystem::path> modifications;

		BOOL result = GetQueuedCompletionStatus(_completion_handle, &transferred, &key, &lpOverlapped, 0);
		if (!result)
		{
			DWORD error = GetLastError();
			if (error == WAIT_TIMEOUT)
			{
				return modifications;
			}
			else if (error == ERROR_INVALID_HANDLE)
			{
				LOG(ERROR) << "GetQueuedCompletionStatus failed: Invalid handle";
				return modifications;
			}
			else
			{
				LOG(ERROR) << "GetQueuedCompletionStatus failed: " << error;
				return modifications;
			}
		}

		auto record = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(_buffer.data());

		while (true)
		{
			const std::wstring filename(record->FileName, record->FileNameLength / sizeof(WCHAR));
			modifications.push_back(_path / filename);

			if (record->NextEntryOffset == 0)
			{
				break;
			}
			record = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(reinterpret_cast<const BYTE*>(record) + record->NextEntryOffset);
		}

		ZeroMemory(&_overlapped, sizeof(OVERLAPPED));
		DWORD buffer_length;
		if (!ReadDirectoryChangesW(_file_handle, _buffer.data(), static_cast<DWORD>(_buffer.size()), FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE, &buffer_length, &_overlapped, nullptr))
		{
			LOG(ERROR) << "ReadDirectoryChangesW failed: " << GetLastError();
		}

		return modifications;
	}
} // namespace big
