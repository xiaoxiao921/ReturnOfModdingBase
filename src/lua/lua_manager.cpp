#include "lua_manager.hpp"

#include "bindings/gui.hpp"
#include "bindings/imgui.hpp"
#include "bindings/log.hpp"
#include "bindings/memory.hpp"
#include "bindings/path.hpp"
#include "bindings/paths.hpp"
#include "bindings/toml/toml_lua.hpp"
#include "bindings/toml_v2/toml_lua_v2.hpp"
#include "directory_watcher/directory_watcher.hpp"
#include "file_manager/file_manager.hpp"
#include "logger/logger.hpp"
#include "string/string.hpp"

namespace big
{
	std::optional<module_info> lua_manager::get_module_info(const std::filesystem::path& module_path)
	{
		constexpr auto thunderstore_manifest_json_file_name = "manifest.json";
		std::filesystem::path manifest_path;
		std::filesystem::path current_folder = module_path.parent_path();
		std::filesystem::path root_folder    = g_file_manager.get_base_dir();
		while (true)
		{
			if (current_folder == root_folder)
			{
				break;
			}

			const auto potential_manifest_path = current_folder / thunderstore_manifest_json_file_name;
			if (std::filesystem::exists(potential_manifest_path))
			{
				manifest_path = potential_manifest_path;
				break;
			}

			if (current_folder.has_parent_path())
			{
				current_folder = current_folder.parent_path();
			}
			else
			{
				break;
			}
		}

		if (!std::filesystem::exists(manifest_path))
		{
			LOG(WARNING) << "No manifest path, can't load " << reinterpret_cast<const char*>(module_path.u8string().c_str());
			return {};
		}

		ts::v1::manifest manifest;
		try
		{
			std::ifstream manifest_file(manifest_path);
			nlohmann::json manifest_json = nlohmann::json::parse(manifest_file, nullptr, false, true);

			manifest = manifest_json.get<ts::v1::manifest>();

			manifest.version = semver::version::parse(manifest.version_number);
		}
		catch (const std::exception& e)
		{
			LOG(ERROR) << "Failed reading manifest.json: " << e.what();
			return {};
		}

		for (const auto& dep : manifest.dependencies)
		{
			const auto splitted = big::string::split(dep, '-');
			if (splitted.size() == 3)
			{
				manifest.dependencies_no_version_number.push_back(splitted[0] + '-' + splitted[1]);
			}
			else
			{
				LOG(ERROR) << "Invalid dependency string " << dep << " inside the following manifest: " << manifest_path << ". Example format: AuthorName-ModName-1.0.0";
			}
		}

		std::string folder_name = (char*)current_folder.filename().u8string().c_str();
		const auto sep_count    = std::ranges::count(folder_name, '-');
		if (sep_count != 1)
		{
			LOGF(ERROR,
			     "Bad folder name ({}) for the following mod: {}. Should be the following format: AuthorName-ModName",
			     folder_name,
			     manifest.name);
		}

		if (sep_count > 1)
		{
			auto remove_extra_separators = [](std::string& input)
			{
				size_t sep_count = 0;
				for (size_t i = 0; i < input.size(); i++)
				{
					if (input[i] == '-')
					{
						sep_count++;

						// is second '-' separator
						if (sep_count == 2)
						{
							input.resize(i);
							LOG(WARNING) << "Using folder name as " << input;
							return;
						}
					}
				}
			};

			remove_extra_separators(folder_name);
		}

		const std::string guid = folder_name;
		return {{
		    .m_path              = module_path,
		    .m_folder_path       = current_folder,
		    .m_guid              = guid,
		    .m_guid_with_version = guid + "-" + manifest.version_number,
		    .m_manifest          = manifest,
		}};
	}

	lua_manager::lua_manager(lua_State* game_lua_state, folder config_folder, folder plugins_data_folder, folder plugins_folder, on_lua_state_init_t on_lua_state_init, get_env_for_module_t get_env_for_module) :
	    m_state(game_lua_state),
	    m_config_folder(config_folder),
	    m_plugins_data_folder(plugins_data_folder),
	    m_plugins_folder(plugins_folder),
	    m_on_lua_state_init(on_lua_state_init),
	    m_get_env_for_module(get_env_for_module)
	{
		g_lua_manager = this;
	}

	lua_manager::~lua_manager()
	{
		lua::window::serialize();

		unload_all_modules();

		g_lua_manager = nullptr;
	}

	void lua_manager::init_file_watcher(const std::filesystem::path& directory)
	{
		std::thread(
		    [directory]
		    {
			    std::vector<big::directory_watcher> watchers;
			    watchers.emplace_back(directory);
			    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory, std::filesystem::directory_options::skip_permission_denied))
			    {
				    if (!entry.is_directory())
				    {
					    continue;
				    }

				    watchers.emplace_back(entry.path());
			    }

			    while (g_lua_manager)
			    {
				    for (auto& watcher : watchers)
				    {
					    auto modified_files = watcher.check();
					    for (const auto& file_path : modified_files)
					    {
						    auto fullPath = file_path.wstring();
						    if (fullPath.ends_with(L".lua") && !g_lua_manager->m_to_reload_duplicate_checker_2.contains(fullPath))
						    {
							    const auto mod_info = get_module_info(fullPath);
							    if (mod_info.has_value())
							    {
								    g_lua_manager->m_to_reload_duplicate_checker_2.insert(fullPath);

								    std::scoped_lock l(g_lua_manager->m_module_lock);
								    for (const auto& mod : g_lua_manager->m_modules)
								    {
									    if (mod->guid() == mod_info->m_guid
									        && !g_lua_manager->m_to_reload_duplicate_checker.contains(mod->guid()))
									    {
										    std::scoped_lock l(g_lua_manager->m_to_reload_lock);
										    g_lua_manager->m_to_reload_queue.push(mod.get());
										    g_lua_manager->m_to_reload_duplicate_checker.insert(mod->guid());

										    break;
									    }
								    }
							    }
						    }
					    }
				    }

				    using namespace std::chrono_literals;
				    std::this_thread::sleep_for(500ms);
			    }
		    })
		    .detach();
	}

	void lua_manager::process_file_watcher_queue()
	{
		std::scoped_lock l(m_to_reload_lock);
		bool entered = false;
		while (m_to_reload_queue.size())
		{
			entered = true;

			std::scoped_lock l(m_module_lock);

			auto& mod = m_to_reload_queue.front();

			mod->cleanup();
			mod->load_and_call_plugin(m_state);

			m_to_reload_queue.pop();
		}

		if (entered)
		{
			m_to_reload_duplicate_checker.clear();
			m_to_reload_duplicate_checker_2.clear();
		}
	}

	// https://sol2.readthedocs.io/en/latest/exceptions.html
	static int exception_handler(lua_State* L, sol::optional<const std::exception&> maybe_exception, sol::string_view description)
	{
		// L is the lua state, which you can wrap in a state_view if necessary
		// maybe_exception will contain exception, if it exists
		// description will either be the what() of the exception or a description saying that we hit the general-case catch(...)
		if (maybe_exception)
		{
			const std::exception& ex = *maybe_exception;
			LOG(ERROR) << ex.what();
		}
		else
		{
			LOG(ERROR) << description;
		}
		Logger::FlushQueue();

		// you must push 1 element onto the stack to be
		// transported through as the error object in Lua
		// note that Lua -- and 99.5% of all Lua users and libraries -- expects a string
		// so we push a single string (in our case, the description of the error)
		return sol::stack::push(L, description);
	}

	static void panic_handler(sol::optional<std::string> maybe_msg)
	{
		LOG(ERROR) << "Lua is in a panic state and will now abort() the application";
		if (maybe_msg)
		{
			const std::string& msg = maybe_msg.value();
			LOG(ERROR) << "error message: " << msg;
		}
		Logger::FlushQueue();

		// When this function exits, Lua will exhibit default behavior and abort()
	}

	static int traceback_error_handler(lua_State* L)
	{
		std::string msg = "An unknown error has triggered the error handler";
		sol::optional<sol::string_view> maybetopmsg = sol::stack::unqualified_check_get<sol::string_view>(L, 1, &sol::no_panic);
		if (maybetopmsg)
		{
			const sol::string_view& topmsg = maybetopmsg.value();
			msg.assign(topmsg.data(), topmsg.size());
		}
		luaL_traceback(L, L, msg.c_str(), 1);
		sol::optional<sol::string_view> maybetraceback = sol::stack::unqualified_check_get<sol::string_view>(L, -1, &sol::no_panic);
		if (maybetraceback)
		{
			const sol::string_view& traceback = maybetraceback.value();
			msg.assign(traceback.data(), traceback.size());
		}
		LOG(ERROR) << msg;
		return sol::stack::push(L, msg);
	}

	void lua_manager::init_lua_state()
	{
		m_state.set_exception_handler(exception_handler);
		m_state.set_panic(sol::c_call<decltype(&panic_handler), &panic_handler>);
		lua_CFunction traceback_function = sol::c_call<decltype(&traceback_error_handler), &traceback_error_handler>;
		sol::protected_function::set_default_handler(sol::object(m_state.lua_state(), sol::in_place, traceback_function));

		init_lua_api();
	}

	void lua_manager::init_lua_api()
	{
		sol::table global_table = m_state.globals();
		sol::table lua_ext = rom::g_lua_api_namespace.size() ? m_state.create_named_table(rom::g_lua_api_namespace) : global_table;

		sol::table mods = lua_ext.create_named("mods");

		// Lua API: Function
		// Table: mods
		// Name: loading_order
		// Returns: table<int, string>: Table containing the order in which mods are loaded by the mod loader.
		mods["loading_order"] = std::ref(g_lua_manager->m_modules_loading_order);

		// Lua API: Function
		// Table: mods
		// Name: on_all_mods_loaded
		// Param: callback: function: callback that will be called once all mods are loaded. The callback function should match signature func()
		// Registers a callback that will be called once all mods are loaded. Will be called instantly if mods are already loaded and that you are just hot-reloading your mod.
		mods["on_all_mods_loaded"] = [](sol::protected_function cb, sol::this_environment env)
		{
			big::lua_module* mdl = big::lua_module::this_from(env);
			if (mdl)
			{
				mdl->m_data.m_on_all_mods_loaded_callbacks.push_back(cb);
			}
		};

		// Let's keep that list sorted the same as the solution file explorer
		lua::toml_lua::bind(lua_ext);
		lua::toml_lua_v2::bind(lua_ext);
		lua::gui::bind(lua_ext);
		lua::imgui::bind(lua_ext);
		lua::log::bind(m_state, lua_ext);
		lua::memory::bind(m_state);
		lua::path::bind(lua_ext);
		lua::paths::bind(lua_ext);

		if (m_on_lua_state_init)
		{
			m_on_lua_state_init(m_state, lua_ext);
		}
	}

	static void imgui_text(const char* fmt, const std::string& str)
	{
		if (str.size())
		{
			ImGui::Text(fmt, str.c_str());
		}
	}

	void lua_manager::draw_menu_bar_callbacks()
	{
		std::scoped_lock guard(m_module_lock);

		for (const auto& module : m_modules)
		{
			if (ImGui::BeginMenu(module->guid().c_str()))
			{
				if (ImGui::BeginMenu("Mod Info"))
				{
					const auto& manifest = module->manifest();
					imgui_text("Version: %s", manifest.version_number);
					imgui_text("Website URL: %s", manifest.website_url);
					imgui_text("Description: %s", manifest.description);
					if (manifest.dependencies.size())
					{
						int i = 0;
						for (const auto& dependency : manifest.dependencies)
						{
							imgui_text(std::format("Dependency[{}]: %s", i++).c_str(), dependency);
						}
					}

					ImGui::EndMenu();
				}

				for (const auto& element : module->m_data.m_menu_bar_callbacks)
				{
					element->draw();
				}

				ImGui::EndMenu();
			}
		}
	}

	void lua_manager::always_draw_independent_gui()
	{
		std::scoped_lock guard(m_module_lock);

		for (const auto& module : m_modules)
		{
			for (const auto& element : module->m_data.m_always_draw_independent_gui)
			{
				element->draw();
			}
		}
	}

	void lua_manager::draw_independent_gui()
	{
		std::scoped_lock guard(m_module_lock);

		for (const auto& module : m_modules)
		{
			for (const auto& element : module->m_data.m_independent_gui)
			{
				element->draw();
			}
		}
	}

	bool lua_manager::dynamic_hook_pre_callbacks(const uintptr_t target_func_ptr, sol::object return_value, std::vector<sol::object> args)
	{
		std::scoped_lock guard(m_module_lock);

		bool call_orig_if_true = true;

		for (const auto& module : m_modules)
		{
			for (const auto& cb : module->m_data.m_dynamic_hook_pre_callbacks[target_func_ptr])
			{
				const auto new_call_orig_if_true = cb(return_value, sol::as_args(args));

				if (call_orig_if_true && new_call_orig_if_true.valid() && new_call_orig_if_true.get_type() == sol::type::boolean
				    && new_call_orig_if_true.get<bool>() == false)
				{
					call_orig_if_true = false;
				}
			}
		}

		return call_orig_if_true;
	}

	void lua_manager::dynamic_hook_post_callbacks(const uintptr_t target_func_ptr, sol::object return_value, std::vector<sol::object> args)
	{
		std::scoped_lock guard(m_module_lock);

		for (const auto& module : m_modules)
		{
			for (const auto& cb : module->m_data.m_dynamic_hook_post_callbacks[target_func_ptr])
			{
				cb(return_value, sol::as_args(args));
			}
		}
	}

	void lua_manager::unload_module(const std::string& module_guid)
	{
		std::scoped_lock guard(m_module_lock);

		std::erase_if(m_modules,
		              [&](auto& module)
		              {
			              return module_guid == module->guid();
		              });
	}

	bool lua_manager::module_exists(const std::string& module_guid)
	{
		std::scoped_lock guard(m_module_lock);

		for (const auto& module : m_modules)
		{
			if (module->guid() == module_guid)
			{
				return true;
			}
		}

		return false;
	}

	lua_module* lua_manager::get_fallback_module()
	{
		return m_modules[0].get();
	}

	void lua_manager::unload_all_modules()
	{
		std::scoped_lock guard(m_module_lock);

		m_modules.clear();
	}
} // namespace big
