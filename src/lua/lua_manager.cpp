#include "lua_manager.hpp"

#include "bindings/gui.hpp"
#include "bindings/imgui.hpp"
#include "bindings/log.hpp"
#include "bindings/memory.hpp"
#include "bindings/path.hpp"
#include "bindings/paths.hpp"
#include "bindings/toml/toml_lua.hpp"
#include "file_manager/file_manager.hpp"
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
			LOG(FATAL) << "Failed reading manifest.json: " << e.what();
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
				LOG(FATAL) << "Invalid dependency string " << dep << " inside the following manifest: " << manifest_path << ". Example format: AuthorName-ModName-1.0.0";
			}
		}

		const std::string folder_name = (char*)current_folder.filename().u8string().c_str();
		const auto sep_count          = std::ranges::count(folder_name, '-');
		if (sep_count != 1)
		{
			LOGF(FATAL,
			     "Bad folder name ({}) for the following mod: {}. Should be the following format: AuthorName-ModName",
			     folder_name,
			     manifest.name);
		}

		std::vector<std::string> lua_file_entries;
		for (const auto& entry : std::filesystem::recursive_directory_iterator(current_folder, std::filesystem::directory_options::skip_permission_denied))
		{
			if (entry.exists() && entry.path().extension() == ".lua")
			{
				std::string lua_file_entry  = (char*)entry.path().filename().u8string().c_str();
				lua_file_entry             += std::to_string(
                    std::chrono::duration_cast<std::chrono::milliseconds>(entry.last_write_time().time_since_epoch()).count());
				lua_file_entries.push_back(lua_file_entry);
			}
		}

		std::sort(lua_file_entries.begin(), lua_file_entries.end());
		std::string final_hash = "";
		for (const auto& file_entry : lua_file_entries)
		{
			final_hash += file_entry;
		}

		const std::string guid = folder_name;
		return {{
		    .m_lua_file_entries_hash = final_hash,
		    .m_path                  = module_path,
		    .m_folder_path           = current_folder,
		    .m_guid                  = guid,
		    .m_guid_with_version     = guid + "-" + manifest.version_number,
		    .m_manifest              = manifest,
		}};
	}

	lua_manager::lua_manager(lua_State* game_lua_state, folder config_folder, folder plugins_data_folder, folder plugins_folder, on_lua_state_init_t on_lua_state_init) :
	    m_state(game_lua_state),
	    m_config_folder(config_folder),
	    m_plugins_data_folder(plugins_data_folder),
	    m_plugins_folder(plugins_folder),
	    m_on_lua_state_init(on_lua_state_init)
	{
		g_lua_manager = this;
	}

	lua_manager::~lua_manager()
	{
		lua::window::serialize();

		unload_all_modules();

		g_lua_manager = nullptr;
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
			LOG(FATAL) << ex.what();
		}
		else
		{
			LOG(FATAL) << description;
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
		LOG(FATAL) << "Lua is in a panic state and will now abort() the application";
		if (maybe_msg)
		{
			const std::string& msg = maybe_msg.value();
			LOG(FATAL) << "error message: " << msg;
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
		LOG(FATAL) << msg;
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

	static auto g_lua_file_watcher_last_time = std::chrono::high_resolution_clock::now();

	void lua_manager::update_file_watch_reload_modules()
	{
		std::scoped_lock guard(m_module_lock);

		using namespace std::chrono_literals;

		const auto time_now = std::chrono::high_resolution_clock::now();
		if ((time_now - g_lua_file_watcher_last_time) > 500ms)
		{
			g_lua_file_watcher_last_time = time_now;

			std::unordered_set<std::string> already_reloaded_this_frame;
			for (const auto& entry : std::filesystem::recursive_directory_iterator(m_plugins_folder.get_path(), std::filesystem::directory_options::skip_permission_denied))
			{
				if (entry.path().extension() == ".lua")
				{
					const auto module_info = get_module_info(entry.path());
					if (module_info)
					{
						if (already_reloaded_this_frame.contains(module_info.value().m_guid))
						{
							continue;
						}

						for (const auto& already_loaded_module : m_modules)
						{
							if (already_loaded_module->guid() == module_info.value().m_guid)
							{
								if (already_loaded_module->update_lua_file_entries(module_info.value().m_lua_file_entries_hash))
								{
									already_loaded_module->cleanup();
									already_loaded_module->load_and_call_plugin(m_state);
									already_reloaded_this_frame.insert(module_info.value().m_guid);
									break;
								}
							}
						}
					}
				}
			}
		}
	}
} // namespace big
