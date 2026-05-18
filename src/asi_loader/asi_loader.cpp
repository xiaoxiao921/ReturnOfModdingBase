#include "asi_loader.hpp"

#include <string>

namespace big::asi_loader
{
	static std::wstring MyGetModuleFileNameW(HMODULE hModule)
	{
		static constexpr auto INITIAL_BUFFER_SIZE = MAX_PATH;
		static constexpr auto MAX_ITERATIONS      = 7;
		std::wstring ret;
		auto bufferSize = INITIAL_BUFFER_SIZE;
		for (size_t iterations = 0; iterations < MAX_ITERATIONS; ++iterations)
		{
			ret.resize(bufferSize);
			auto charsReturned = GetModuleFileNameW(hModule, &ret[0], bufferSize);
			if (charsReturned < ret.length())
			{
				ret.resize(charsReturned);
				return ret;
			}
			else
			{
				bufferSize *= 2;
			}
		}
		return L"";
	}

	static std::wstring MyGetCurrentDirectoryW()
	{
		static constexpr auto INITIAL_BUFFER_SIZE = MAX_PATH;
		static constexpr auto MAX_ITERATIONS      = 7;
		std::wstring ret;
		auto bufferSize = INITIAL_BUFFER_SIZE;
		for (size_t iterations = 0; iterations < MAX_ITERATIONS; ++iterations)
		{
			ret.resize(bufferSize);
			auto charsReturned = GetCurrentDirectoryW(bufferSize, ret.data());
			if (charsReturned < ret.length())
			{
				ret.resize(charsReturned);
				return ret;
			}
			else
			{
				bufferSize *= 2;
			}
		}
		return L"";
	}

	static void find_files(WIN32_FIND_DATAW* fd)
	{
		auto dir = MyGetCurrentDirectoryW();

		HANDLE asiFile = FindFirstFileW(L"*.asi", fd);
		if (asiFile != INVALID_HANDLE_VALUE)
		{
			do
			{
				if (!(fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					auto pos = wcslen(fd->cFileName);

					if (fd->cFileName[pos - 4] == '.' && (fd->cFileName[pos - 3] == 'a' || fd->cFileName[pos - 3] == 'A') && (fd->cFileName[pos - 2] == 's' || fd->cFileName[pos - 2] == 'S') && (fd->cFileName[pos - 1] == 'i' || fd->cFileName[pos - 1] == 'I'))
					{
						auto path = dir + L'\\' + fd->cFileName;

						if (GetModuleHandleW(path.c_str()) == NULL)
						{
							auto h = LoadLibraryW(path.c_str());
							SetCurrentDirectoryW(dir.c_str()); //in case asi switched it

							if (h == NULL)
							{
								auto e = GetLastError();
								if (e != ERROR_DLL_INIT_FAILED && e != ERROR_BAD_EXE_FORMAT) // in case dllmain returns false or IMAGE_MACHINE is not compatible
								{
									std::wstring msg = L"Unable to load " + std::wstring(fd->cFileName) + L". Error: " + std::to_wstring(e) + L".";
									if (e == ERROR_MOD_NOT_FOUND)
									{
										msg +=
										    L" This ASI file requires a dependency that is missing from your system. "
										    "To identify the missing dependency, download and run the free, "
										    "open-source "
										    "app, "
										    "Dependencies at https://github.com/lucasg/Dependencies";
									}
									MessageBoxW(0, msg.c_str(), L"ASI Loader", MB_ICONERROR);
								}
							}
							else
							{
								auto procedure = (void (*)())GetProcAddress(h, "InitializeASI");
								if (procedure != NULL)
								{
									procedure();
								}
							}
						}
					}
				}
			} while (FindNextFileW(asiFile, fd));
			FindClose(asiFile);
		}
	}

	void init(const std::filesystem::path& base_folder_path, std::function<bool(const std::filesystem::path&)> pred)
	{
		const auto old_directory_path = MyGetCurrentDirectoryW();

		WIN32_FIND_DATAW fd{};

		for (const auto& entry : std::filesystem::recursive_directory_iterator(base_folder_path, std::filesystem::directory_options::skip_permission_denied))
		{
			std::error_code ec;
			if (!entry.is_directory(ec))
			{
				continue;
			}

			const auto& p = entry.path();

			if (pred(p))
			{
				const auto dir = p.wstring();

				SetCurrentDirectoryW(dir.c_str());

				find_files(&fd);
			}
		}

		SetCurrentDirectoryW(old_directory_path.c_str());
	}
} // namespace big::asi_loader
