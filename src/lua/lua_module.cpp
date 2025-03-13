#include "lua_module.hpp"

#include "file_manager/file_manager.hpp"
#include "lua_manager.hpp"
#include "rom/rom.hpp"

// clang-format off
#include <AsyncLogger/Logger.hpp>
using namespace al;

// clang-format on

namespace big
{
	void lua_module::init()
	{
		auto ns = get_PLUGIN_table(m_env);

		// Lua API: Table
		// Name: _ENV - Plugin Specific Global Table
		// Each mod/plugin have their own global table containing helpers, such as:
		// - Their own guid
		//
		// - Path to their own folder inside `config`: Used for data that must persist between sessions that can be manipulated by the user.
		//
		// - Path to their own folder inside `plugins_data`: Used for data that must persist between sessions but not be manipulated by the user.
		//
		// - Path to their own folder inside `plugins`: Location of .lua, README, manifest.json files.
		//
		// You can access other mods helpers through the `{LUA_API_NAMESPACE}.mods[OTHER_MOD_GUID]` table.
		//
		// **Example Usage:**
		//
		// ```lua
		// print(_ENV._PLUGIN.guid)
		//
		// for n in pairs({LUA_API_NAMESPACE}.mods[_ENV._PLUGIN.guid]) do
		//     log.info(n)
		// end
		// ```

		// Lua API: Field
		// Table: _ENV - Plugin Specific Global Table
		// Field: _PLUGIN.guid: string
		// Guid of the mod.
		ns["guid"] = m_info.m_guid;

		// Lua API: Field
		// Table: _ENV - Plugin Specific Global Table
		// Field: _PLUGIN.version: string
		// Version of the mod.
		ns["version"] = m_info.m_manifest.version_number;

		// Lua API: Field
		// Table: _ENV - Plugin Specific Global Table
		// Field: _PLUGIN.dependencies: table<string>
		// Dependencies of the mod.
		ns["dependencies"] = m_info.m_manifest.dependencies;

		// Lua API: Field
		// Table: _ENV - Plugin Specific Global Table
		// Field: _PLUGIN.dependencies_no_version_number: table<string>
		// Dependencies of the mod without the version numbers.
		ns["dependencies_no_version_number"] = m_info.m_manifest.dependencies_no_version_number;

		// Lua API: Field
		// Table: _ENV - Plugin Specific Global Table
		// Field: _PLUGIN.config_mod_folder_path: string
		// Path to the mod folder inside `config`
		auto config_mod_folder_path = g_file_manager.get_project_folder("config").get_path() / m_info.m_guid;
		auto config_mod_folder_path_string = std::string(reinterpret_cast<const char*>(config_mod_folder_path.u8string().c_str()));
		ns["config_mod_folder_path"] = config_mod_folder_path_string;

		// Lua API: Field
		// Table: _ENV - Plugin Specific Global Table
		// Field: _PLUGIN.plugins_data_mod_folder_path: string
		// Path to the mod folder inside `plugins_data`
		auto plugins_data_mod_folder_path = g_file_manager.get_project_folder("plugins_data").get_path() / m_info.m_guid;
		auto plugins_data_mod_folder_path_string =
		    std::string(reinterpret_cast<const char*>(plugins_data_mod_folder_path.u8string().c_str()));
		ns["plugins_data_mod_folder_path"] = plugins_data_mod_folder_path_string;

		// Lua API: Field
		// Table: _ENV - Plugin Specific Global Table
		// Field: _PLUGIN.plugins_mod_folder_path: string
		// Path to the mod folder inside `plugins`
		auto plugins_mod_folder_path_string = std::string(reinterpret_cast<const char*>(m_info.m_folder_path.u8string().c_str()));
		ns["plugins_mod_folder_path"] = plugins_mod_folder_path_string;

		// Lua API: Field
		// Table: _ENV - Plugin Specific Global Table
		// Field: _PLUGIN.this: lua_module*
		ns["this"] = this;
	}

	lua_module::lua_module(const module_info& module_info, sol::state_view& state) :
	    m_info(module_info),
	    m_env(state, sol::create, state.globals())
	{
		init();
	}

	lua_module::lua_module(const module_info& module_info, sol::environment& env) :
	    m_info(module_info),
	    m_env(env)
	{
		init();
	}

	void lua_module::cleanup()
	{
		for (auto memory : m_data.m_allocated_memory)
		{
			delete[] memory;
		}

		m_data = {};
	}

	lua_module::~lua_module()
	{
		cleanup();
	}

	const std::filesystem::path& lua_module::path() const
	{
		return m_info.m_path;
	}

	const ts::v1::manifest& lua_module::manifest() const
	{
		return m_info.m_manifest;
	}

	const std::string& lua_module::guid() const
	{
		return m_info.m_guid;
	}

	sol::environment& lua_module::env()
	{
		return m_env;
	}

	load_module_result lua_module::load_and_call_plugin(sol::state_view& state)
	{
		LOG(INFO) << "Loading " << m_info.m_guid_with_version;

		auto result = state.safe_script_file(m_info.m_path.string(), m_env, &sol::script_pass_on_error, sol::load_mode::text);

		if (!result.valid())
		{
			LOG(ERROR) << m_info.m_guid_with_version << " failed to load: " << result.get<sol::error>().what();
			Logger::FlushQueue();

			m_error_count++;

			return load_module_result::FAILED_TO_LOAD;
		}
		else
		{
			LOG(INFO) << "Loaded " << m_info.m_guid_with_version;

			// Lua API: Field
			// Table: mods
			// Field: [Mod GUID]: string
			// Each mod once loaded will have a key in this table, the key will be their guid string and the value their `_ENV`.
			if (rom::g_lua_api_namespace.size())
			{
				state.traverse_set(rom::g_lua_api_namespace, "mods", m_info.m_guid, m_env);
			}
			else
			{
				state.traverse_set("mods", m_info.m_guid, m_env);
			}
		}

		return load_module_result::SUCCESS;
	}

	std::string lua_module::guid_from(sol::this_environment this_env)
	{
		sol::environment& env            = this_env;
		sol::optional<std::string> _guid = get_PLUGIN_table(env)["guid"];
		if (_guid)
		{
			return _guid.value();
		}

		return g_lua_manager->get_fallback_module()->guid();
	}

	big::lua_module* lua_module::this_from(sol::this_environment this_env)
	{
		sol::environment& env                 = this_env;
		sol::optional<big::lua_module*> _this = get_PLUGIN_table(env)["this"];

		// That's weird.
		if (_this && _this.value())
		{
			return _this.value();
		}

		return g_lua_manager->get_fallback_module();
	}
} // namespace big
