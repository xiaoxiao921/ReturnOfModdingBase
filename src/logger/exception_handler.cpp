#include "exception_handler.hpp"

#include "hooks/hooking.hpp"
#include "logger.hpp"
#include "paths/paths.hpp"
#include "rom/rom.hpp"
#include "stack_trace.hpp"

#include <csignal>
#include <DbgHelp.h>
#include <set>

namespace big
{
	static auto hash_stack_trace(std::vector<uintptr_t> stack_trace)
	{
		auto data        = reinterpret_cast<const char*>(stack_trace.data());
		std::size_t size = stack_trace.size() * sizeof(uintptr_t);

		return std::hash<std::string_view>()({data, size});
	}

	static HMODULE DbgHelp_module = nullptr;

	static void abort_signal_handler(int signal)
	{
		RaiseException(0xDE'AD, 0, 0, NULL);
	}

	static LPTOP_LEVEL_EXCEPTION_FILTER WINAPI hook_SetUnhandledExceptionFilter(_In_opt_ LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter)
	{
		return nullptr;
	}

	exception_handler::exception_handler(bool forbid_any_future_set_unhandled_exception_filter, void* custom_top_level_exception_filter)
	{
		if (!DbgHelp_module)
		{
			DbgHelp_module = ::LoadLibraryW(L"DBGHELP.DLL");
			if (!DbgHelp_module)
			{
				LOG(ERROR) << "Failed loading DbgHelp";
			}
		}

		m_previous_handler        = signal(SIGABRT, abort_signal_handler);
		m_previous_abort_behavior = _set_abort_behavior(0, _WRITE_ABORT_MSG);

		m_old_error_mode = SetErrorMode(0);

		// clang-format off
		m_exception_handler = SetUnhandledExceptionFilter(
			custom_top_level_exception_filter ?
			(LPTOP_LEVEL_EXCEPTION_FILTER)custom_top_level_exception_filter :
			big_exception_handler
		);
		// clang-format on

		// TODO: Add a way of adding a AddVectoredContinueHandler for cases where Windows will fail to call our UnhandledExceptionFilter?

		if (forbid_any_future_set_unhandled_exception_filter)
		{
			static detour_hook anti_remover("Anti Exception Handler Remover", SetUnhandledExceptionFilter, hook_SetUnhandledExceptionFilter);
			anti_remover.enable();
		}
	}

	exception_handler::~exception_handler()
	{
		LOG(ERROR) << "Exception handler killed.";
		Logger::FlushQueue();
	}

	typedef BOOL(WINAPI* MINIDUMPWRITEDUMP)(HANDLE hProcess, DWORD dwPid, HANDLE hFile, MINIDUMP_TYPE DumpType, const PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam, const PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam, const PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

	static void print_last_error(const char* main_error_text)
	{
		std::stringstream error_msg;
		error_msg << main_error_text << HEX_TO_UPPER(GetLastError());
		MessageBoxA(0, error_msg.str().c_str(), rom::g_project_name.c_str(), MB_ICONERROR);
	}

	static BOOL write_mini_dump(EXCEPTION_POINTERS* exception_info)
	{
		BOOL is_success = FALSE;

		MINIDUMPWRITEDUMP MiniDumpWriteDump_function = nullptr;
		const auto minidump_type = (MINIDUMP_TYPE)(MiniDumpNormal | MiniDumpWithHandleData | MiniDumpWithProcessThreadData | MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory);

		const auto& dump_file_path  = paths::remove_and_get_dump_file_path();
		const auto dump_file_handle = CreateFileW(dump_file_path.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
		if (dump_file_handle == INVALID_HANDLE_VALUE)
		{
			print_last_error(
			    std::format("CreateFileW failed. Path: {}\nError code: ", (char*)dump_file_path.u8string().c_str()).c_str());

			goto cleanup;
		}

		MiniDumpWriteDump_function = (MINIDUMPWRITEDUMP)::GetProcAddress(DbgHelp_module, "MiniDumpWriteDump");
		if (!MiniDumpWriteDump_function)
		{
			print_last_error("GetProcAddress error code: ");

			goto cleanup;
		}

		// Initialize minidump structure
		MINIDUMP_EXCEPTION_INFORMATION mdei;
		mdei.ThreadId          = GetCurrentThreadId();
		mdei.ExceptionPointers = exception_info;
		mdei.ClientPointers    = FALSE;

		is_success = MiniDumpWriteDump_function(GetCurrentProcess(), GetCurrentProcessId(), dump_file_handle, minidump_type, &mdei, 0, NULL);
		if (!is_success)
		{
			print_last_error("MiniDumpWriteDump_function error code: ");

			goto cleanup;
		}

		is_success = TRUE;

	cleanup:

		if (dump_file_handle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(dump_file_handle);
		}

		return is_success;
	}

	LONG big_exception_handler(EXCEPTION_POINTERS* exception_info)
	{
		const auto exception_code = exception_info->ExceptionRecord->ExceptionCode;
		if (exception_code == EXCEPTION_BREAKPOINT || exception_code == DBG_PRINTEXCEPTION_C || exception_code == DBG_PRINTEXCEPTION_WIDE_C)
		{
			return EXCEPTION_CONTINUE_SEARCH;
		}

		if (IsDebuggerPresent())
		{
			return EXCEPTION_CONTINUE_SEARCH;
		}

		static std::set<std::size_t> logged_exceptions;

		stack_trace trace;
		if (trace.new_stack_trace(exception_info))
		{
			return EXCEPTION_CONTINUE_SEARCH;
		}
		const auto trace_hash = hash_stack_trace(trace.frame_pointers());
		if (const auto it = logged_exceptions.find(trace_hash); it == logged_exceptions.end())
		{
			LOG(ERROR) << trace;
			Logger::FlushQueue();

			logged_exceptions.insert(trace_hash);

			if (exception_handler::g_write_mini_dump)
			{
				write_mini_dump(exception_info);
			}
		}

		return EXCEPTION_CONTINUE_SEARCH;
	}
} // namespace big
