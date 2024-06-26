#pragma once
#include "fwddec.hpp"
#include "handle.hpp"

#include <optional>
#include <vector>
#include <windows.h>

namespace memory
{
	class range
	{
	public:
		range(handle base, std::size_t size);

		handle begin() const;
		handle end() const;
		std::size_t size() const;

		bool contains(handle h) const;

		std::optional<handle> scan(const pattern& sig) const;
		std::vector<handle> scan_all(const pattern& sig) const;

	protected:
		handle m_base;
		std::size_t m_size;
		DWORD m_timestamp;
	};
} // namespace memory
