#pragma once

#include <windows.h>

namespace memory
{
	template<class T>
	inline void force_write(T& dst, const T& src)
	{
		DWORD old_flag;
		::VirtualProtect(&dst, sizeof(T), PAGE_EXECUTE_READWRITE, &old_flag);
		dst = src;
		::VirtualProtect(&dst, sizeof(T), old_flag, &old_flag);
	}
} // namespace memory
