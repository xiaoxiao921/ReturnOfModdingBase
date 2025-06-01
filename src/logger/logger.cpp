#pragma once
#include "logger.hpp"

#include "bits/bits.hpp"
#include "config/config.hpp"
#include "memory/module.hpp"
#include "rom/rom.hpp"

#include <sstream>
#include <string/string_conversions.hpp>
#undef ERROR

namespace big
{
	template<typename TP>
	std::time_t to_time_t(TP tp)
	{
		using namespace std::chrono;
		auto sctp = time_point_cast<system_clock::duration>(tp - TP::clock::now() + system_clock::now());
		return system_clock::to_time_t(sctp);
	}

	logger::logger(std::string_view console_title, file file) :
	    m_did_console_exist(false),
	    m_console_logger(&logger::format_console),
	    m_console_title(console_title),
	    m_original_console_mode(0),
	    m_console_handle(nullptr),
	    m_file(file)
	{
		if (rom::get_instance_id())
		{
			auto unique_file_name  = m_file.get_path();
			unique_file_name      += rom::get_instance_id_string();
			m_file                 = unique_file_name;
		}

		auto module = memory::module("ntdll.dll");
		if (const auto env_no_color = std::getenv("NO_COLOR"); module.get_export("wine_get_version") || (env_no_color && strlen(env_no_color)))
		{
			LOG(DEBUG) << "Using simple logger.";
			m_console_logger = &logger::format_console_simple;
		}

		initialize();

		g_log = this;
	}

	logger::~logger()
	{
		g_log = nullptr;
	}

	void logger::initialize()
	{
		m_attach_console_cfg = big::config::general->bind("Logging", "Console Enabled", true, "Enables showing a console for log output.");

		if (m_attach_console_cfg->get_value())
		{
			if (m_did_console_exist = ::AttachConsole(GetCurrentProcessId()); !m_did_console_exist)
			{
				AllocConsole();
			}

			if (m_console_handle = GetStdHandle(STD_OUTPUT_HANDLE); m_console_handle != nullptr)
			{
				SetConsoleTitleA(m_console_title.data());
				SetConsoleOutputCP(CP_UTF8);

				DWORD console_mode;
				GetConsoleMode(m_console_handle, &console_mode);
				m_original_console_mode = console_mode;

				// terminal like behaviour enable full color support
				console_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
				// prevent clicking in terminal from suspending our main thread
				console_mode &= ~(ENABLE_QUICK_EDIT_MODE);

				SetConsoleMode(m_console_handle, console_mode);

				EnableMenuItem(GetSystemMenu(GetConsoleWindow(), 0), SC_CLOSE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
			}
		}
		handle_backups();
		open_outstreams();

		m_log_level_filter_console_cfg = big::config::general->bind("Logging", "Console LogLevels", "DEBUG, INFO, WARNING, ERROR", "Only displays the specified log levels in the console.");
		m_log_level_filter_file_cfg = big::config::general->bind("Logging", "File LogLevels", "DEBUG, INFO, WARNING, ERROR", "Only displays the specified log levels in the log file.");
		refresh_log_filter_values_from_config();

		Logger::Init();

		Logger::AddSink(
		    [this](LogMessagePtr msg)
		    {
			    const auto msg_lvl = msg->Level();
			    if (bits::has_bits_set((int*)&m_log_level_filter_file_value, (int)msg_lvl))
			    {
				    format_file(std::move(msg));
			    }
		    });

		Logger::AddSink(
		    [this](LogMessagePtr msg)
		    {
			    const auto msg_lvl = msg->Level();
			    if (bits::has_bits_set((int*)&m_log_level_filter_console_value, (int)msg_lvl))
			    {
				    (this->*m_console_logger)(std::move(msg));
			    }
		    });
	}

	void logger::destroy()
	{
		Logger::Destroy();
		close_outstreams();

		if (m_did_console_exist)
		{
			SetConsoleMode(m_console_handle, m_original_console_mode);
		}

		if (!m_did_console_exist && m_attach_console_cfg->get_value())
		{
			FreeConsole();
		}
	}

	void logger::handle_backups()
	{
		try
		{
			const auto backup_folder_path = m_file.get_path().parent_path() / "backup";

			if (!std::filesystem::exists(backup_folder_path))
			{
				return;
			}

			std::vector<std::pair<std::wstring, uintmax_t>> files;
			uintmax_t total_size = 0;

			for (const auto& entry : std::filesystem::recursive_directory_iterator(backup_folder_path, std::filesystem::directory_options::skip_permission_denied | std::filesystem::directory_options::follow_directory_symlink))
			{
				if (entry.is_regular_file() && entry.path().extension() == ".log")
				{
					uintmax_t file_size  = entry.file_size();
					total_size          += file_size;
					files.emplace_back(entry.path().wstring(), file_size);
				}
			}

			constexpr auto MB_100 = 100 * 1024 * 1024;
			constexpr auto MB_50  = 50 * 1024 * 1024;
			if (total_size > MB_100)
			{
				LOG(WARNING) << "Deleting backup file until folder is under 50MB.";

				// Sort files by size in descending order
				std::sort(files.begin(),
				          files.end(),
				          [](const auto& a, const auto& b)
				          {
					          return a.second > b.second;
				          });

				// Delete largest files until size is under 50MB
				for (const auto& file : files)
				{
					if (total_size <= MB_50)
					{
						break;
					}
					if (std::filesystem::remove(file.first))
					{
						LOG(WARNING) << "Deleting backup file " << big::string_conversions::utf16_to_utf8(file.first);
						total_size -= file.second;
					}
				}
			}
		}
		catch (const std::exception& e)
		{
			LOG(WARNING) << "Exception while handling backups: " << e.what();
		}
		catch (...)
		{
			LOG(WARNING) << "Unknown exception while handling backups.";
		}

		try
		{
			if (m_file.exists())
			{
				auto file_time  = std::filesystem::last_write_time(m_file.get_path());
				auto time_t     = to_time_t(file_time);
				auto local_time = std::localtime(&time_t);

				m_file.move(std::format("./backup/{:0>2}-{:0>2}-{}-{:0>2}-{:0>2}-{:0>2}_{}",
				                        local_time->tm_mon + 1,
				                        local_time->tm_mday,
				                        local_time->tm_year + 1900,
				                        local_time->tm_hour,
				                        local_time->tm_min,
				                        local_time->tm_sec,
				                        m_file.get_path().filename().string().c_str()));
			}
		}
		catch (const std::exception& e)
		{
			LOG(WARNING) << "Exception while creating backup " << e.what();
		}
		catch (...)
		{
			LOG(WARNING) << "Unknown exception while creating backup.";
		}
	}

	void logger::open_outstreams()
	{
		if (m_attach_console_cfg->get_value())
		{
			m_console_out.open("CONOUT$", std::ios_base::out | std::ios_base::app);
		}

		m_file_out.open(m_file.get_path(), std::ios_base::out | std::ios_base::trunc);
	}

	void logger::close_outstreams()
	{
		if (m_attach_console_cfg->get_value())
		{
			m_console_out.close();
		}

		m_file_out.close();
	}

	const LogColor get_color(const eLogLevel level)
	{
		switch (level)
		{
		case DEBUG:   return LogColor::BLUE;
		case INFO:    return LogColor::GREEN;
		case WARNING: return LogColor::YELLOW;
		case ERROR:   return LogColor::RED;
		}
		return LogColor::WHITE;
	}

	const char* get_level_string(const eLogLevel level)
	{
		switch (level)
		{
		case DEBUG:   return "DEBUG";
		case INFO:    return "INFO";
		case WARNING: return "WARNING";
		case ERROR:   return "ERROR";
		}

		return "INFO";
	}

	static std::string get_local_timestamp(const std::chrono::system_clock::time_point& timestamp)
	{
		// TODO: This function is a bit sus, might get away with using to_time_t(TP tp) and what handle_backups is doing?

		using namespace std::chrono;

		auto time_t_timestamp = system_clock::to_time_t(timestamp);
		auto local_time       = *std::localtime(&time_t_timestamp);

		auto mks         = duration_cast<microseconds>(timestamp.time_since_epoch());
		std::string smks = std::to_string(mks.count() % 10'000'000);
		while (smks.size() < 7)
		{
			smks.insert(smks.begin(), '0');
		}

		return std::format("{}:{}:{}.{}", local_time.tm_hour, local_time.tm_min, local_time.tm_sec, smks);
	}

	void logger::format_console(const LogMessagePtr msg)
	{
		const auto color = get_color(msg->Level());

		const auto timestamp = get_local_timestamp(msg->Timestamp());
		const auto& location = msg->Location();
		const auto level     = msg->Level();

		const auto file = std::filesystem::path(location.file_name()).filename().string();

		m_console_out << "[" << timestamp << "]" << ADD_COLOR_TO_STREAM(color) << "[" << get_level_string(level) << "/" << file << ":"
		              << location.line() << "] " << RESET_STREAM_COLOR << msg->Message() << std::flush;
	}

	void logger::format_console_simple(const LogMessagePtr msg)
	{
		const auto color = get_color(msg->Level());

		const auto timestamp = get_local_timestamp(msg->Timestamp());
		const auto& location = msg->Location();
		const auto level     = msg->Level();

		const auto file = std::filesystem::path(location.file_name()).filename().string();

		m_console_out << "[" << timestamp << "]"
		              << "[" << get_level_string(level) << "/" << file << ":" << location.line() << "] " << msg->Message();

		m_console_out.flush();
	}

	void logger::format_file(const LogMessagePtr msg)
	{
		if (!m_file_out.is_open())
		{
			return;
		}

		const auto timestamp = get_local_timestamp(msg->Timestamp());
		const auto& location = msg->Location();
		const auto level     = msg->Level();

		const auto file = std::filesystem::path(location.file_name()).filename().string();

		m_file_out << "[" << timestamp << "]"
		           << "[" << get_level_string(level) << "/" << file << ":" << location.line() << "] " << msg->Message();

		m_file_out.flush();
	}

	void logger::refresh_log_filter_values_from_config()
	{
		auto init_log_filter = [](std::shared_ptr<toml_v2::config_file::config_entry<const char*>>& cfg, int* flag)
		{
			const auto str = cfg->get_value();
			auto res       = *flag;
			if (str.contains("DEBUG") || str.contains("VERBOSE"))
			{
				res |= DEBUG;
			}
			if (str.contains("INFO"))
			{
				res |= INFO;
			}
			if (str.contains("WARNING"))
			{
				res |= WARNING;
			}
			if (str.contains("ERROR") || str.contains("FATAL"))
			{
				res |= ERROR;
			}

			*flag = res;
		};
		init_log_filter(m_log_level_filter_console_cfg, (int*)&m_log_level_filter_console_value);
		init_log_filter(m_log_level_filter_file_cfg, (int*)&m_log_level_filter_file_value);
	}
} // namespace big
