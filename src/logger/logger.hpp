#pragma once
#include "file_manager/file_manager.hpp"

#include <fstream>
#include <toml_v2/config_file.hpp>
#include <Windows.h>

// clang-format off
#include <AsyncLogger/Logger.hpp>
using namespace al;

// clang-format on

namespace big
{
#define ADD_COLOR_TO_STREAM(color) "\x1b[" << int(color) << "m"
#define RESET_STREAM_COLOR         "\x1b[" << int(LogColor::RESET) << "m"
#define HEX_TO_UPPER(value)        "0x" << std::hex << std::uppercase << (DWORD64)value << std::dec << std::nouppercase
#define HEX_TO_UPPER_OFFSET(value) \
	"0x" << std::hex << std::uppercase << ((DWORD64)value - (DWORD64)GetModuleHandle(0)) << std::dec << std::nouppercase

	enum class LogColor
	{
		RESET,
		WHITE   = 97,
		CYAN    = 36,
		MAGENTA = 35,
		BLUE    = 34,
		GREEN   = 32,
		YELLOW  = 33,
		RED     = 31,
		BLACK   = 30
	};

	class logger final
	{
	public:
		logger(std::string_view console_title, file file);
		virtual ~logger();

		void initialize();
		void destroy();

		void refresh_log_filter_values_from_config();

	private:
		void create_backup();

		void open_outstreams();
		void close_outstreams();

		void format_console(const LogMessagePtr msg);
		void format_console_simple(const LogMessagePtr msg);
		void format_file(const LogMessagePtr msg);

	private:
		toml_v2::config_file::config_entry<bool>* m_attach_console_cfg;
		bool m_did_console_exist;

		void (logger::*m_console_logger)(const LogMessagePtr msg);

		std::string_view m_console_title;
		DWORD m_original_console_mode;
		HANDLE m_console_handle;

		std::ofstream m_console_out;
		std::ofstream m_file_out;

		file m_file;

		toml_v2::config_file::config_entry<const char*>* m_log_level_filter_console_cfg{};
		eLogLevel m_log_level_filter_console_value{};

		toml_v2::config_file::config_entry<const char*>* m_log_level_filter_file_cfg{};
		eLogLevel m_log_level_filter_file_value{};
	};

	inline logger* g_log = nullptr;
} // namespace big
