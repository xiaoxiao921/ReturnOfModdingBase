#pragma once
#include "load_module_result.hpp"
#include "lua_module.hpp"
#include "module_info.hpp"

#include <file_manager/folder.hpp>
#include <mutex>

namespace big
{
	class lua_manager
	{
	private:
		sol::state_view m_state;

	public:
		std::recursive_mutex m_module_lock;
		std::vector<std::unique_ptr<lua_module>> m_modules;

		folder m_config_folder;
		folder m_plugins_data_folder;
		folder m_plugins_folder;

	public:
		using on_lua_state_init_t = std::function<void(sol::state_view&, sol::table&)>;

	private:
		on_lua_state_init_t m_on_lua_state_init;

		bool m_is_all_mods_loaded{};

	public:
		lua_manager(lua_State* game_lua_state, folder config_folder, folder plugins_data_folder, folder plugins_folder, on_lua_state_init_t on_lua_state_init = nullptr);
		~lua_manager();

		template<typename T>
		inline void init()
		{
			init_lua_state();

			load_all_modules<T>();

			lua::window::deserialize();
		}

		void init_lua_state();
		void init_lua_api();

		template<typename T>
		void load_fallback_module();
		lua_module* get_fallback_module();

		template<typename T>
		void load_all_modules();
		void unload_all_modules();

		void update_file_watch_reload_modules();

		inline auto get_module_count() const
		{
			return m_modules.size();
		}

		static std::optional<module_info> get_module_info(const std::filesystem::path& module_path);

		void draw_menu_bar_callbacks();
		void always_draw_independent_gui();
		void draw_independent_gui();

		bool module_exists(const std::string& module_guid);

		void unload_module(const std::string& module_guid);

		template<typename T>
		load_module_result load_module(const module_info& module_info, bool ignore_failed_to_load = false);
	};

	inline lua_manager* g_lua_manager;
} // namespace big
