#pragma once
#include "toml_lua_v2.hpp"

#include "toml_v2/config_file.hpp"

namespace lua::toml_lua_v2
{
	static toml_v2::config_file::config_entry_base* bind_entry_bool(toml_v2::config_file& self, const std::string& section, const std::string& key, bool default_value, const std::string& description, sol::this_environment env)
	{
		return (toml_v2::config_file::config_entry_base*)self.bind(section, key, default_value, description);
	}

	static toml_v2::config_file::config_entry_base* bind_entry_double(toml_v2::config_file& self, const std::string& section, const std::string& key, double default_value, const std::string& description, sol::this_environment env)
	{
		return (toml_v2::config_file::config_entry_base*)self.bind(section, key, default_value, description);
	}

	static toml_v2::config_file::config_entry_base* bind_entry_string(toml_v2::config_file& self, const std::string& section, const std::string& key, const std::string& default_value, const std::string& description, sol::this_environment env)
	{
		return (toml_v2::config_file::config_entry_base*)self.bind(section, key, default_value, description);
	}

	static sol::object get_value(toml_v2::config_file::config_entry_base& self, sol::this_state env)
	{
		const auto& t = self.type();
		if (t == typeid(bool))
		{
			return sol::make_object(env.L, self.get_value_base<bool>());
		}
		else if (t == typeid(double))
		{
			return sol::make_object(env.L, self.get_value_base<double>());
		}
		else if (t == typeid(std::string))
		{
			return sol::make_object(env.L, self.get_value_base<std::string>());
		}

		return sol::lua_nil;
	}

	static void set_value_bool(toml_v2::config_file::config_entry_base& self, bool new_value, sol::this_environment env)
	{
		self.set_value_base(new_value);
	}

	static void set_value_double(toml_v2::config_file::config_entry_base& self, double new_value, sol::this_environment env)
	{
		self.set_value_base(new_value);
	}

	static void set_value_string(toml_v2::config_file::config_entry_base& self, const std::string& new_value, sol::this_environment env)
	{
		self.set_value_base(new_value);
	}

	void bind(sol::table& state)
	{
		// Lua API: Table
		// Name: config
		// Table containing helpers for mod configs.
		sol::table ns = state.create_named("config");

		// Lua API: Field
		// Table: config
		// Field: config_files: table of config.config_file instances
		// All currently active config_file instances.
		ns["config_files"] = &toml_v2::config_file::g_config_files;

		// Lua API: Class
		// Name: config.config_file
		// A helper class to handle persistent data.

		// Lua API: Constructor
		// Class: config.config_file
		// Param: config_path: string: Full path to a file that contains settings. The file will be created as needed.
		// Param: save_on_init: bool: If the config file/directory doesn't exist, create it immediately.
		// Create a new config file at the specified config path.
		auto config_file_ut = ns.new_usertype<toml_v2::config_file>("config_file",
		                                                            sol::meta_function::construct,
		                                                            sol::factories(
		                                                                // dot syntax, no "self" value
		                                                                // passed in
		                                                                [](const std::string& config_path, bool save_on_init, sol::this_environment env)
		                                                                {
			                                                                return std::make_unique<toml_v2::config_file>(config_path, save_on_init, big::lua_module::guid_from(env));
		                                                                },
		                                                                // -- colon syntax, passes in the
		                                                                // "self" value as first argument implicitly
		                                                                [](sol::object, const std::string& config_path, bool save_on_init, sol::this_environment env)
		                                                                {
			                                                                return std::make_unique<toml_v2::config_file>(config_path, save_on_init, big::lua_module::guid_from(env));
		                                                                }));

		// Lua API: Field
		// Class: config.config_file
		// Field: owner_guid: string
		// The owner GUID of this config file.
		config_file_ut["owner_guid"] = &toml_v2::config_file::m_owner_guid;

		// Lua API: Field
		// Class: config.config_file
		// Field: config_file_path: string
		// The file path of this config file.
		config_file_ut["config_file_path"] = sol::property(
		    [](const toml_v2::config_file& self) -> std::string
		    {
			    return (char*)self.m_config_file_path.u8string().c_str();
		    });

		// Lua API: Field
		// Class: config.config_file
		// Field: entries: table<config_definition, config_entry>
		// All config entries of the config file.
		config_file_ut["entries"] = &toml_v2::config_file::m_entries;

		// Lua API: Function
		// Class: config.config_file
		// Name: bind
		// Param: section: string: Section/category/group of the setting. Settings are grouped by this.
		// Param: key: string: Name of the setting.
		// Param: default_value: bool or number or string: Value of the setting if the setting was not created yet.
		// Param: description: string: Simple description of the setting shown to the user.
		// Returns: config_entry: new config_entry object.
		// Create a new setting. The setting is saved to drive and loaded automatically.
		// Each section and key pair can be used to add only one setting,
		// trying to add a second setting will throw an exception.
		config_file_ut.set_function("bind", sol::overload(bind_entry_bool, bind_entry_double, bind_entry_string));

		// Lua API: Function
		// Class: config.config_file
		// Name: save
		// Writes the config to disk.
		config_file_ut.set_function("save", &toml_v2::config_file::save);

		// Lua API: Function
		// Class: config.config_file
		// Name: reload
		// Reloads the config from disk. Unsaved changes are lost.
		config_file_ut.set_function("reload", &toml_v2::config_file::reload);

		// Lua API: Class
		// Name: config.config_entry
		// Provides access to a single setting inside of a config_file.
		auto config_entry_ut = ns.new_usertype<toml_v2::config_file::config_entry_base>("config_entry", sol::no_constructor);

		// Lua API: Function
		// Class: config.config_entry
		// Name: get
		// Returns: val: bool or double or string. Value of this setting
		config_entry_ut.set_function("get", get_value);

		// Lua API: Function
		// Class: config.config_entry
		// Name: set
		// Param: new_value: bool or double or string: New value of this setting.
		config_entry_ut.set_function("set", sol::overload(set_value_bool, set_value_double, set_value_string));

		// Lua API: Class
		// Name: config.config_definition
		// Section and key of a setting.
		auto config_definition_ut = ns.new_usertype<toml_v2::config_definition>("config_definition", sol::no_constructor);

		// Lua API: Field
		// Class: config.config_definition
		// Field: section: string
		// Group of the setting. All settings within a config file are grouped by this.
		config_definition_ut["section"] = &toml_v2::config_definition::m_section;

		// Lua API: Field
		// Class: config.config_definition
		// Field: key: string
		// Name of the setting.
		config_definition_ut["key"] = &toml_v2::config_definition::m_key;
	}
} // namespace lua::toml_lua_v2
