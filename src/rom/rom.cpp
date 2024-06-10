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

	cxxopts::Options get_rom_cxx_options()
	{
		constexpr auto root_folder_arg_name = "rom_modding_root_folder";
		constexpr auto rom_enabled_arg_name = "rom_enabled";

		cxxopts::Options options(rom::g_project_name);
		options.add_options()(root_folder_arg_name, root_folder_arg_name, cxxopts::value<std::string>()->default_value(""));
		options.add_options()(rom_enabled_arg_name, rom_enabled_arg_name, cxxopts::value<std::string>()->default_value("true"));
		return options;
	}

	struct store_reason_to_file
	{
		std::filesystem::path m_file_path;
		bool m_file_exists = false;

		store_reason_to_file()
		{
			const auto current_working_dir = std::filesystem::current_path();

			constexpr auto file_name = "ReturnOfModdingFirstEnabledReason.txt";

			m_file_path   = current_working_dir / file_name;
			m_file_exists = std::filesystem::exists(m_file_path);

			std::ifstream file_stream(m_file_path);
			if (!file_stream.is_open())
			{
				return;
			}

			uint16_t first_enabled_reason = 0;
			file_stream >> first_enabled_reason;
			g_first_enabled_reason = (enabled_reason)first_enabled_reason;

			file_stream.close();
		}

		~store_reason_to_file()
		{
			if (m_file_exists)
			{
				return;
			}

			std::ofstream file_stream(m_file_path);
			if (!file_stream.is_open())
			{
				return;
			}

			file_stream << (uint16_t)g_enabled_reason << std::endl;
			file_stream.close();
		}
	};

	bool is_rom_enabled()
	{
		const store_reason_to_file _store_reason_to_file{};

		constexpr auto rom_enabled_arg_name = "rom_enabled";

		const char* rom_enabled_value = std::getenv(rom_enabled_arg_name);
		if (rom_enabled_value && strlen(rom_enabled_value))
		{
			if (strstr(rom_enabled_value, "true"))
			{
				LOG(INFO) << "ReturnOfModding enabled because " << rom_enabled_value;

				g_enabled_reason = enabled_reason::ENABLED_BY_ENV_VAR;
				return true;
			}
			else
			{
				LOG(INFO) << "ReturnOfModding disabled because " << rom_enabled_value;

				g_enabled_reason = enabled_reason::DISABLED_BY_ENV_VAR;
				return false;
			}
		}
		else
		{
			try
			{
				auto* args  = GetCommandLineA();
				int argc    = 0;
				auto** argv = rom::CommandLineToArgvA(args, &argc);

				auto options = rom::get_rom_cxx_options();

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
						g_enabled_reason = enabled_reason::ENABLED_BY_CMD_LINE;
					}
					else
					{
						LOG(INFO) << "ReturnOfModding disabled from command line";
						g_enabled_reason = enabled_reason::DISABLED_BY_CMD_LINE;
						rom_enabled      = false;
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

		if (g_first_enabled_reason == enabled_reason::ENABLED_BY_ENV_VAR || g_first_enabled_reason == enabled_reason::ENABLED_BY_CMD_LINE)
		{
			LOG(INFO)
			    << "ReturnOfModding disabled because first enabled launch was explicit through env var or cmd line";
			g_enabled_reason = enabled_reason::DISABLED_BECAUSE_FIRST_ENABLED_LAUNCH_WAS_EXPLICIT_THROUGH_ENV_VAR_OR_CMD_LINE;
			return false;
		}
		else
		{
			LOG(INFO) << "ReturnOfModding enabled";
			g_enabled_reason = enabled_reason::ENABLED_BY_DEFAULT;
		}

		return true;
	}
} // namespace rom
