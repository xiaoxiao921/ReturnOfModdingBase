#include "directory_watcher.hpp"

#include <Windows.h>

namespace big
{
	static void CALLBACK empty_cb(DWORD error, DWORD len, LPOVERLAPPED ov)
	{
	}

	directory_watcher::directory_watcher(const std::filesystem::path& path) :
	    _path(path),
	    _buffer(sizeof(FILE_NOTIFY_INFORMATION) + MAX_PATH * sizeof(WCHAR))
	{
		_file_handle = CreateFileW(path.wstring().c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
		_completion_handle = CreateIoCompletionPort(_file_handle, nullptr, reinterpret_cast<ULONG_PTR>(_file_handle), 1);

		OVERLAPPED overlapped = {};
		ReadDirectoryChangesW(_file_handle, _buffer.data(), static_cast<DWORD>(_buffer.size()), FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE, nullptr, &overlapped, empty_cb);
	}

	directory_watcher::~directory_watcher()
	{
		CancelIo(_file_handle);

		CloseHandle(_file_handle);
		CloseHandle(_completion_handle);
	}

	std::vector<std::filesystem::path> directory_watcher::check()
	{
		DWORD transferred;
		ULONG_PTR key;
		OVERLAPPED* overlapped;

		if (!GetQueuedCompletionStatus(_completion_handle, &transferred, &key, &overlapped, 0))
		{
			ReadDirectoryChangesW(_file_handle, _buffer.data(), static_cast<DWORD>(_buffer.size()), FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE, nullptr, overlapped, empty_cb);

			return {};
		}

		auto record = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(_buffer.data());

		std::vector<std::filesystem::path> modifications;
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

		overlapped->hEvent = nullptr;

		ReadDirectoryChangesW(_file_handle, _buffer.data(), static_cast<DWORD>(_buffer.size()), FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE, nullptr, overlapped, empty_cb);

		return modifications;
	}
} // namespace big
