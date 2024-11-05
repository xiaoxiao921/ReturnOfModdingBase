#pragma once

#include <cstddef>
#include <stdint.h>
#include <windows.h>

namespace big
{
	class exception_handler final
	{
	public:
		exception_handler(bool forbid_any_future_set_unhandled_exception_filter, void* custom_top_level_exception_filter);
		virtual ~exception_handler();

		static inline bool g_write_mini_dump = true;

	private:
		int m_previous_abort_behavior = 0;
		typedef void (*signal_handler)(int);
		signal_handler m_previous_handler = nullptr;

		void* m_exception_handler{};
		uint32_t m_old_error_mode{};
	};

	extern LONG big_exception_handler(EXCEPTION_POINTERS* exception_info);
} // namespace big
