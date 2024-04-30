#pragma once
// clang-format off
#include <windows.h>
#include <tlhelp32.h>

// clang-format on

namespace big::threads
{
	extern bool are_suspended;

	extern void resume_all(DWORD target_process_id = GetCurrentProcessId(), DWORD thread_id_to_not_suspend = GetCurrentThreadId());

	extern void suspend_all_but_one(DWORD target_process_id = GetCurrentProcessId(), DWORD thread_id_to_not_suspend = GetCurrentThreadId());
} // namespace big::threads
