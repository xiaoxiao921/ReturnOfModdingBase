#include "hooks/hooking.hpp"

#include "memory/module.hpp"
#include "threads/util.hpp"

namespace big
{
	hooking::hooking()
	{
		for (auto& detour_hook_helper : m_detour_hook_helpers)
		{
			const auto is_lazy_hook = detour_hook_helper.m_on_hooking_available.operator bool();
			if (is_lazy_hook)
			{
				detour_hook_helper.m_detour_hook->set_target_and_create_hook(detour_hook_helper.m_on_hooking_available());
			}
		}

		g_hooking = this;
	}

	hooking::~hooking()
	{
		if (m_enabled)
		{
			disable();
		}

		g_hooking = nullptr;
	}

	void hooking::enable()
	{
		threads::suspend_all_but_one();

		for (auto& detour_hook_helper : m_detour_hook_helpers)
		{
			detour_hook_helper.m_detour_hook->enable();
		}

		threads::resume_all();

		m_enabled = true;
	}

	void hooking::disable()
	{
		m_enabled = false;

		threads::suspend_all_but_one();

		for (auto& detour_hook_helper : m_detour_hook_helpers)
		{
			detour_hook_helper.m_detour_hook->disable();
		}

		threads::resume_all();

		m_detour_hook_helpers.clear();
	}

	hooking::detour_hook_helper::~detour_hook_helper()
	{
	}

	void hooking::detour_hook_helper::enable_now()
	{
		m_detour_hook->enable();
	}

	void hooking::detour_hook_helper::enable_hook_if_hooking_is_already_running()
	{
		if (g_hooking && g_hooking->m_enabled)
		{
			if (m_on_hooking_available)
			{
				m_detour_hook->set_target_and_create_hook(m_on_hooking_available());
			}

			m_detour_hook->enable();
		}
	}
} // namespace big
