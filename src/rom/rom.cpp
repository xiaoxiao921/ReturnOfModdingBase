#include "rom.hpp"

#include "logger/logger.hpp"

namespace rom
{
	LPSTR* CommandLineToArgvA(LPCSTR cmd_line, int* argc)
	{
		ULONG len = strlen(cmd_line);
		ULONG i   = ((len + 2) / 2) * sizeof(LPVOID) + sizeof(LPVOID);

		LPSTR* argv = (LPSTR*)GlobalAlloc(GMEM_FIXED, i + (len + 2) * sizeof(CHAR));

		LPSTR _argv = (LPSTR)(((PUCHAR)argv) + i);

		ULONG _argc   = 0;
		argv[_argc]   = _argv;
		BOOL in_qm    = FALSE;
		BOOL in_text  = FALSE;
		BOOL in_space = TRUE;
		ULONG j       = 0;
		i             = 0;

		CHAR a;
		while ((a = cmd_line[i]))
		{
			if (in_qm)
			{
				if (a == '\"')
				{
					in_qm = FALSE;
				}
				else
				{
					_argv[j] = a;
					j++;
				}
			}
			else
			{
				switch (a)
				{
				case '\"':
					in_qm   = TRUE;
					in_text = TRUE;
					if (in_space)
					{
						argv[_argc] = _argv + j;
						_argc++;
					}
					in_space = FALSE;
					break;
				case ' ':
				case '\t':
				case '\n':
				case '\r':
					if (in_text)
					{
						_argv[j] = '\0';
						j++;
					}
					in_text  = FALSE;
					in_space = TRUE;
					break;
				default:
					in_text = TRUE;
					if (in_space)
					{
						argv[_argc] = _argv + j;
						_argc++;
					}
					_argv[j] = a;
					j++;
					in_space = FALSE;
					break;
				}
			}
			i++;
		}
		_argv[j]    = '\0';
		argv[_argc] = NULL;

		(*argc) = _argc;
		return argv;
	}

	bool is_rom_enabled()
	{
		constexpr auto rom_enabled_arg_name = "rom_enabled";

		const char* rom_enabled_value = std::getenv(rom_enabled_arg_name);
		if (rom_enabled_value && strlen(rom_enabled_value))
		{
			if (strstr(rom_enabled_value, "true"))
			{
				LOG(INFO) << "ReturnOfModding enabled because " << rom_enabled_value;

				return true;
			}
			else
			{
				LOG(INFO) << "ReturnOfModding disabled because " << rom_enabled_value;

				return false;
			}
		}
		else
		{
			try
			{
				cxxopts::Options options(rom::g_project_name);
				auto* args  = GetCommandLineA();
				int argc    = 0;
				auto** argv = rom::CommandLineToArgvA(args, &argc);

				options.add_options()(rom_enabled_arg_name, rom_enabled_arg_name, cxxopts::value<std::string>()->default_value("true"));

				const auto result = options.parse(argc, argv);

				bool rom_enabled     = true;
				bool rom_enabled_set = false;
				if (result.count(rom_enabled_arg_name))
				{
					rom_enabled_set = true;

					auto& rom_enabled_value_str = result[rom_enabled_arg_name].as<std::string>();
					LOG(INFO) << rom_enabled_value_str;
					if (rom_enabled_value_str.contains("true"))
					{
						LOG(INFO) << "ReturnOfModding enabled from command line";
					}
					else
					{
						LOG(INFO) << "ReturnOfModding disabled from command line";
						rom_enabled = false;
					}
				}

				LocalFree(argv);

				if (rom_enabled_set)
				{
					return rom_enabled;
				}
			}
			catch (const std::exception& e)
			{
				LOG(WARNING) << "Failed parsing cmd line args " << e.what();
			}
		}

		LOG(INFO) << "ReturnOfModding enabled";

		return true;
	}
} // namespace rom
