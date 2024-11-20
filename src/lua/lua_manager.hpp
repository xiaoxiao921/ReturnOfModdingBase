#pragma once
#include "bindings/runtime_func_t.hpp"
#include "load_module_result.hpp"
#include "lua_module.hpp"
#include "module_info.hpp"
#include "rom/rom.hpp"

#include <file_manager/folder.hpp>
#include <lua/bindings/imgui_window.hpp>
#include <mutex>
#include <stack>
#include <thunderstore/v1/manifest.hpp>
#include <unordered_set>

// clang-format off
#include <AsyncLogger/Logger.hpp>
using namespace al;

// clang-format on

namespace big
{
	class lua_manager
	{
	private:
		sol::state_view m_state;

	private:
		std::recursive_mutex m_to_reload_lock;
		std::queue<lua_module*> m_to_reload_queue;
		std::unordered_set<std::string> m_to_reload_duplicate_checker;
		std::unordered_set<std::wstring> m_to_reload_duplicate_checker_2;

	public:

		std::recursive_mutex m_module_lock;
		std::vector<std::unique_ptr<lua_module>> m_modules;

		std::vector<std::string> m_modules_loading_order;

		folder m_config_folder;
		folder m_plugins_data_folder;
		folder m_plugins_folder;

		// non owning map
		std::unordered_map<uintptr_t, lua::memory::runtime_func_t*> m_target_func_ptr_to_dynamic_hook;

	public:
		using on_lua_state_init_t  = std::function<void(sol::state_view&, sol::table&)>;
		using get_env_for_module_t = std::function<sol::environment(sol::state_view&)>;

	private:
		on_lua_state_init_t m_on_lua_state_init;

		bool m_is_all_mods_loaded{};

		get_env_for_module_t m_get_env_for_module;

	public:
		lua_manager(lua_State* game_lua_state, folder config_folder, folder plugins_data_folder, folder plugins_folder, on_lua_state_init_t on_lua_state_init = nullptr, get_env_for_module_t get_env_for_module = nullptr);
		~lua_manager();

	private:
		void init_file_watcher(const std::filesystem::path& directory);

	public:
		lua_State* lua_state()
		{
			return m_state.lua_state();
		}

		bool is_hot_reloading() const
		{
			return m_to_reload_queue.size();
		}

		void process_file_watcher_queue();

		template<typename T>
		inline void init()
		{
			init_lua_state();

			load_all_modules<T>();

			lua::window::deserialize();

			init_file_watcher(m_plugins_folder.get_path());
		}

		void init_lua_state();
		void init_lua_api();

		template<typename T>
		void load_fallback_module()
		{
			try
			{
				auto tmp_path  = std::filesystem::temp_directory_path();
				tmp_path      /= rom::g_project_name + "_fallback_module.lua";
				std::ofstream ofs(tmp_path);
				ofs << "#\n";
				ofs.close();

				const module_info mod_info = {
				    .m_path              = tmp_path,
				    .m_folder_path       = m_plugins_folder.get_path(),
				    .m_guid              = rom::g_project_name + "-GLOBAL",
				    .m_guid_with_version = rom::g_project_name + "-GLOBAL-1.0.0",
				    .m_manifest = {.name = "GLOBAL", .version_number = "1.0.0", .version = semver::version(1, 0, 0), .website_url = "", .description = "Fallback module"},
				};
				const auto load_result = load_module<T>(mod_info, true);
			}
			catch (const std::exception& e)
			{
				LOG(ERROR) << e.what();
			}
			catch (...)
			{
				LOG(ERROR) << "Unknown exception while trying to create fallback module";
			}
		}

		lua_module* get_fallback_module();

	private:
		inline bool topological_sort_visit(const std::string& node, std::stack<std::string>& stack, std::vector<std::string>& sorted_list, const std::function<std::vector<std::string>(const std::string&)>& dependency_selector, std::unordered_set<std::string>& visited, std::unordered_set<std::string>& sorted)
		{
			if (visited.contains(node))
			{
				if (!sorted.contains(node))
				{
					return false;
				}
			}
			else
			{
				visited.insert(node);
				stack.push(node);
				for (const auto& dep : dependency_selector(node))
				{
					if (!topological_sort_visit(dep, stack, sorted_list, dependency_selector, visited, sorted))
					{
						return false;
					}
				}

				sorted.insert(node);
				sorted_list.push_back(node);

				stack.pop();
			}

			return true;
		}

		inline std::vector<std::string> topological_sort(std::vector<std::string>& nodes, const std::function<std::vector<std::string>(const std::string&)>& dependency_selector)
		{
			std::vector<std::string> sorted_list;

			std::unordered_set<std::string> visited;
			std::unordered_set<std::string> sorted;

			for (const auto& input : nodes)
			{
				std::stack<std::string> current_stack;
				if (!topological_sort_visit(input, current_stack, sorted_list, dependency_selector, visited, sorted))
				{
					LOG(ERROR) << "Cyclic Dependency: " << input;
					while (!current_stack.empty())
					{
						LOG(ERROR) << current_stack.top();
						current_stack.pop();
					}
				}
			}

			return sorted_list;
		}

	public:

		template<typename T>
		void load_all_modules()
		{
			load_fallback_module<T>();

			// m_on_after_fallback_module_loaded()

			// Map for lexicographical ordering.
			std::map<std::string, module_info> module_guid_to_module_info{};

			// Get all the modules from the folder.
			for (const auto& entry : std::filesystem::recursive_directory_iterator(m_plugins_folder.get_path(), std::filesystem::directory_options::skip_permission_denied | std::filesystem::directory_options::follow_directory_symlink))
			{
				if (entry.path().filename() == "main.lua")
				{
					const auto module_info = get_module_info(entry.path());
					if (module_info)
					{
						const auto& guid = module_info.value().m_guid;

						if (module_guid_to_module_info.contains(guid))
						{
							if (module_info.value().m_manifest.version > module_guid_to_module_info[guid].m_manifest.version)
							{
								LOG(INFO) << "Found a more recent version of " << guid << " ("
								          << module_info.value().m_manifest.version << " > "
								          << module_guid_to_module_info[guid].m_manifest.version << "): Using that instead.";

								module_guid_to_module_info[guid] = module_info.value();
							}
						}
						else
						{
							module_guid_to_module_info.insert({guid, module_info.value()});
						}
					}
				}
			}

			// Get all the guids to prepare for sorting depending on their dependencies.
			std::vector<std::string> module_guids;
			for (const auto& [guid, info] : module_guid_to_module_info)
			{
				module_guids.push_back(guid);
			}

			// Sort depending on module dependencies.
			const auto sorted_modules = topological_sort(module_guids,
			                                             [&](const std::string& guid)
			                                             {
				                                             if (module_guid_to_module_info.contains(guid))
				                                             {
					                                             return module_guid_to_module_info[guid].m_manifest.dependencies_no_version_number;
				                                             }
				                                             return std::vector<std::string>();
			                                             });


			/*for (const auto& guid : sorted_modules)
			{
				LOG(DEBUG) << guid;
			}*/

			std::unordered_set<std::string> missing_modules;
			for (const auto& guid : sorted_modules)
			{
				const auto mod_loader_name = rom::g_project_name + "-" + rom::g_project_name;

				bool not_missing_dependency = true;
				for (const auto& dependency : module_guid_to_module_info[guid].m_manifest.dependencies_no_version_number)
				{
					// The mod loader is not a lua module,
					// but might be put as a dependency in the mod manifest,
					// don't mark the mod as unloadable because of that.
					if (dependency.contains(mod_loader_name))
					{
						continue;
					}

					if (missing_modules.contains(dependency))
					{
						LOG(WARNING) << "Can't load " << guid << " because it's missing " << dependency;
						not_missing_dependency = false;
					}
				}

				if (not_missing_dependency)
				{
					const auto& module_info = module_guid_to_module_info[guid];
					const auto load_result  = load_module<T>(module_info, false);
					if (load_result == load_module_result::FILE_MISSING)
					{
						// Don't log the fact that the mod loader failed to load, it's normal (see comment above)
						if (!guid.contains(mod_loader_name))
						{
							LOG(WARNING) << guid << " (file path: "
							             << reinterpret_cast<const char*>(module_info.m_path.u8string().c_str()) << " does not exist in the filesystem. Not loading it.";
						}

						missing_modules.insert(guid);
					}
				}
			}

			std::scoped_lock guard(m_module_lock);

			for (const auto& module : m_modules)
			{
				m_modules_loading_order.push_back(module->guid());
			}

			for (const auto& module : m_modules)
			{
				for (const auto& cb : module->m_data.m_on_all_mods_loaded_callbacks)
				{
					cb();
				}
			}

			m_is_all_mods_loaded = true;
		}

		void unload_all_modules();

		inline auto get_module_count() const
		{
			return m_modules.size();
		}

		static std::optional<module_info> get_module_info(const std::filesystem::path& module_path);

		void draw_menu_bar_callbacks();
		void always_draw_independent_gui();
		void draw_independent_gui();

		std::shared_ptr<lua::memory::runtime_func_t> get_existing_dynamic_hook(const uintptr_t target_func_ptr);
		bool dynamic_hook_pre_callbacks(const uintptr_t target_func_ptr, lua::memory::type_info_t return_type, lua::memory::runtime_func_t::return_value_t* return_value, std::vector<lua::memory::type_info_t> param_types, const lua::memory::runtime_func_t::parameters_t* params, const uint8_t param_count);
		void dynamic_hook_post_callbacks(const uintptr_t target_func_ptr, lua::memory::type_info_t return_type, lua::memory::runtime_func_t::return_value_t* return_value, std::vector<lua::memory::type_info_t> param_types, const lua::memory::runtime_func_t::parameters_t* params, const uint8_t param_count);
		sol::object to_lua(const lua::memory::runtime_func_t::parameters_t* params, const uint8_t i, const std::vector<lua::memory::type_info_t>& param_types);
		sol::object to_lua(lua::memory::runtime_func_t::return_value_t* return_value, const lua::memory::type_info_t return_value_type);

		bool module_exists(const std::string& module_guid);

		void unload_module(const std::string& module_guid);

		template<typename T>
		load_module_result load_module(const module_info& module_info, bool is_fallback_module, bool ignore_failed_to_load = false)
		{
			if (!std::filesystem::exists(module_info.m_path))
			{
				return load_module_result::FILE_MISSING;
			}

			std::scoped_lock guard(m_module_lock);
			for (const auto& module : m_modules)
			{
				if (module->guid() == module_info.m_guid)
				{
					LOG(WARNING) << "Module with the guid " << module_info.m_guid << " already loaded.";
					return load_module_result::ALREADY_LOADED;
				}
			}

			const auto module_index = m_modules.size();

			if (is_fallback_module || !m_get_env_for_module)
			{
				m_modules.push_back(std::make_unique<T>(module_info, m_state));
			}
			else
			{
				sol::environment env = m_get_env_for_module(m_state);
				m_modules.push_back(std::make_unique<T>(module_info, env));
			}

			const auto load_result = m_modules[module_index]->load_and_call_plugin(m_state);
			if (load_result == load_module_result::SUCCESS || (load_result == load_module_result::FAILED_TO_LOAD && ignore_failed_to_load))
			{
				if (m_is_all_mods_loaded)
				{
					for (const auto& cb : m_modules[module_index]->m_data.m_on_all_mods_loaded_callbacks)
					{
						cb();
					}
				}
			}
			else
			{
				m_modules.pop_back();
			}

			return load_result;
		}
	};

	inline lua_manager* g_lua_manager;
} // namespace big
