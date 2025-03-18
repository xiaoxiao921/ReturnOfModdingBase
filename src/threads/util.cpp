#pragma once
#include "threads/util.hpp"

#include <windows.h>

namespace big::threads
{
	bool are_suspended = false;

	void resume_all(DWORD target_process_id, DWORD thread_id_to_not_suspend)
	{
		HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
		if (h != INVALID_HANDLE_VALUE)
		{
			THREADENTRY32 te;
			te.dwSize = sizeof(te);
			if (Thread32First(h, &te))
			{
				do
				{
					if (te.dwSize >= FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(te.th32OwnerProcessID) && te.th32OwnerProcessID == target_process_id)
					{
						HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, te.th32ThreadID);
						if (hThread)
						{
							// Get the module of the thread
							HANDLE hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, target_process_id);
							if (hModuleSnap != INVALID_HANDLE_VALUE)
							{
								MODULEENTRY32 me;
								me.dwSize = sizeof(me);
								if (Module32First(hModuleSnap, &me))
								{
									do
									{
										if (_stricmp(me.szModule, "ntdll.dll") != 0 && me.th32ProcessID == target_process_id)
										{
											ResumeThread(hThread);
											are_suspended = false;
											break;
										}
									} while (Module32Next(hModuleSnap, &me));
								}
								CloseHandle(hModuleSnap);
							}
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
		HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
		if (h != INVALID_HANDLE_VALUE)
		{
			THREADENTRY32 te;
			te.dwSize = sizeof(te);
			if (Thread32First(h, &te))
			{
				do
				{
					if (te.dwSize >= FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(te.th32OwnerProcessID) && te.th32OwnerProcessID == target_process_id && te.th32ThreadID != thread_id_to_not_suspend)
					{
						HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, te.th32ThreadID);
						if (hThread)
						{
							// Get the module of the thread
							HANDLE hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, target_process_id);
							if (hModuleSnap != INVALID_HANDLE_VALUE)
							{
								MODULEENTRY32 me;
								me.dwSize = sizeof(me);
								if (Module32First(hModuleSnap, &me))
								{
									do
									{
										if (_stricmp(me.szModule, "ntdll.dll") != 0 && me.th32ProcessID == target_process_id)
										{
											SuspendThread(hThread);
											are_suspended = true;
											break;
										}
									} while (Module32Next(hModuleSnap, &me));
								}
								CloseHandle(hModuleSnap);
							}
							CloseHandle(hThread);
						}
					}
				} while (Thread32Next(h, &te));
			}
			CloseHandle(h);
		}
	}
} // namespace big::threads
