#pragma once
#include "load_module_result.hpp"
#include "lua/bindings/gui_element.hpp"
#include "lua/sol_include.hpp"
#include "lua_patch.hpp"
#include "module_info.hpp"

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
		};

		lua_module_data m_data;

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
