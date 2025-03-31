#pragma once
#include "threads/util.hpp"

#include "memory/module.hpp"

#include <unordered_set>

namespace big::threads
{
	bool are_suspended = false;

	static std::unordered_set<DWORD> g_ntdll_threads;

	PVOID get_thread_start_address(HANDLE hThread)
	{
		typedef LONG NTSTATUS;
		typedef NTSTATUS(WINAPI * pNtQIT)(HANDLE, LONG, PVOID, ULONG, PULONG);
#define STATUS_SUCCESS                  ((NTSTATUS)0x00'00'00'00L)
#define ThreadQuerySetWin32StartAddress 9

		NTSTATUS ntStatus;

		PVOID dwStartAddress;

		pNtQIT NtQueryInformationThread = (pNtQIT)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationThread");

		if (NtQueryInformationThread == NULL)
		{
			return 0;
		}

		HANDLE hCurrentProcess = GetCurrentProcess();

		ntStatus = NtQueryInformationThread(hThread, ThreadQuerySetWin32StartAddress, &dwStartAddress, sizeof(dwStartAddress), NULL);

		DWORD bla = ntStatus;

		if (ntStatus != STATUS_SUCCESS)
		{
			return 0;
		}

		return dwStartAddress;
	}

	static void fill_current_process_ntdll_thread_cache()
	{
		const auto current_process_id = GetCurrentProcessId();

		HANDLE hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, current_process_id);
		if (hModuleSnap == INVALID_HANDLE_VALUE)
		{
			return;
		}

		HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
		if (hThreadSnap == INVALID_HANDLE_VALUE)
		{
			CloseHandle(hModuleSnap);
			return;
		}

		THREADENTRY32 te;
		te.dwSize = sizeof(te);
		if (!Thread32First(hThreadSnap, &te))
		{
			CloseHandle(hThreadSnap);
			CloseHandle(hModuleSnap);
			return;
		}

		const auto ntdll_module_info = memory::module("ntdll.dll");

		do
		{
			if (te.dwSize >= FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(te.th32OwnerProcessID) && te.th32OwnerProcessID == current_process_id)
			{
				HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, te.th32ThreadID);
				if (hThread)
				{
					const auto thread_start_address = get_thread_start_address(hThread);
					if (thread_start_address >= ntdll_module_info.begin().as<PVOID>()
					    && thread_start_address < ntdll_module_info.end().as<PVOID>())
					{
						g_ntdll_threads.insert(te.th32ThreadID);
					}

					CloseHandle(hThread);
				}
			}
		} while (Thread32Next(hThreadSnap, &te));

		CloseHandle(hThreadSnap);
		CloseHandle(hModuleSnap);
	}

	void resume_all(DWORD target_process_id, DWORD thread_id_to_not_suspend)
	{
		if (g_ntdll_threads.empty() && target_process_id == GetCurrentProcessId())
		{
			fill_current_process_ntdll_thread_cache();
		}

		HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
		if (h != INVALID_HANDLE_VALUE)
		{
			THREADENTRY32 te;
			te.dwSize = sizeof(te);
			if (Thread32First(h, &te))
			{
				do
				{
					// clang-format off
					if (te.dwSize >= FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(te.th32OwnerProcessID) &&
						te.th32OwnerProcessID == target_process_id &&
						!g_ntdll_threads.contains(te.th32ThreadID))
					// clang-format on
					{
						HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, te.th32ThreadID);
						if (hThread)
						{
							ResumeThread(hThread);
							are_suspended = false;
							CloseHandle(hThread);
						}
					}
				} while (Thread32Next(h, &te));
			}
			CloseHandle(h);
		}
	}

	void suspend_all_but_one(DWORD target_process_id, DWORD thread_id_to_not_suspend)
	{
		if (g_ntdll_threads.empty() && target_process_id == GetCurrentProcessId())
		{
			fill_current_process_ntdll_thread_cache();
		}

		HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
		if (h != INVALID_HANDLE_VALUE)
		{
			THREADENTRY32 te;
			te.dwSize = sizeof(te);
			if (Thread32First(h, &te))
			{
				do
				{
					// clang-format off
					if (te.dwSize >= FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(te.th32OwnerProcessID) &&
						te.th32OwnerProcessID == target_process_id &&
						te.th32ThreadID != thread_id_to_not_suspend&&
						!g_ntdll_threads.contains(te.th32ThreadID))
					// clang-format on
					{
						HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, te.th32ThreadID);
						if (hThread)
						{
							SuspendThread(hThread);
							are_suspended = true;
							CloseHandle(hThread);
						}
					}
				} while (Thread32Next(h, &te));
			}
			CloseHandle(h);
		}
	}
} // namespace big::threads
