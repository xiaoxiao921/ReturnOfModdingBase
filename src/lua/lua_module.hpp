#pragma once
#include "load_module_result.hpp"
#include "lua/bindings/gui_element.hpp"
#include "lua/bindings/runtime_func_t.hpp"
#include "lua/bindings/type_info_t.hpp"
#include "lua/sol_include.hpp"
#include "lua_patch.hpp"
#include "module_info.hpp"

#include <ankerl/unordered_dense.h>
#include <shared_mutex>
#include <thunderstore/v1/manifest.hpp>

namespace big
{
	class lua_module
	{
	protected:
		module_info m_info;

		sol::environment m_env;

	public:
		using on_lua_module_init_t = std::function<void(sol::environment&)>;

		struct lua_module_data
		{
			std::vector<sol::protected_function> m_on_all_mods_loaded_callbacks;

			std::vector<std::unique_ptr<lua::gui::gui_element>> m_menu_bar_callbacks;
			std::vector<std::unique_ptr<lua::gui::gui_element>> m_always_draw_independent_gui;
			std::vector<std::unique_ptr<lua::gui::gui_element>> m_independent_gui;

			std::vector<std::unique_ptr<lua_patch>> m_registered_patches;

			std::vector<void*> m_allocated_memory;

			// lua modules own and share the runtime_func_t object, such as when no module reference it anymore the hook detour get cleaned up.
			std::vector<std::shared_ptr<lua::memory::runtime_func_t>> m_dynamic_hooks;

			ankerl::unordered_dense::map<uintptr_t, std::vector<sol::protected_function>> m_dynamic_hook_pre_callbacks;

			ankerl::unordered_dense::map<uintptr_t, std::vector<sol::protected_function>> m_dynamic_hook_post_callbacks;

			ankerl::unordered_dense::map<uintptr_t, std::unique_ptr<uint8_t[]>> m_dynamic_call_jit_functions;

			ankerl::unordered_dense::map<uintptr_t, sol::protected_function> m_dynamic_hook_mid_callbacks;

			ankerl::unordered_dense::map<std::string, std::vector<sol::protected_function>> m_file_watchers;

			std::vector<std::unique_ptr<toml_v2::config_file>> m_config_files;
		};

		std::shared_mutex m_file_watcher_mutex;

		lua_module_data m_data;

		size_t m_error_count = 0;

		lua_module(const module_info& module_info, sol::state_view& state);
		lua_module(const module_info& module_info, sol::environment& env);

	private:
		void init();

	public:

		virtual void cleanup();
		~lua_module();

		static inline auto get_PLUGIN_table(sol::environment& env)
		{
			return env["_PLUGIN"].get_or_create<sol::table>();
		}

		const std::filesystem::path& path() const;
		const ts::v1::manifest& manifest() const;
		const std::string& guid() const;

		load_module_result load_and_call_plugin(sol::state_view& state);

		sol::environment& env();

		static std::string guid_from(sol::this_environment this_env);
		static big::lua_module* this_from(sol::this_environment this_env);
	};
} // namespace big
