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
		sol::table ns = state.create_named("toml_v2");

		auto config_file_ut = ns.new_usertype<toml_v2::config_file>("config_file", sol::constructors<toml_v2::config_file(std::string_view, bool, std::string_view)>());

		config_file_ut.set_function("bind", sol::overload(bind_entry_bool, bind_entry_double, bind_entry_string));
		config_file_ut.set_function("save", &toml_v2::config_file::save);
		config_file_ut.set_function("reload", &toml_v2::config_file::reload);

		auto config_entry_ut = ns.new_usertype<toml_v2::config_file::config_entry_base>("config_entry", sol::no_constructor);

		config_entry_ut.set_function("get", get_value);
		config_entry_ut.set_function("set", sol::overload(set_value_bool, set_value_double, set_value_string));
	}
} // namespace lua::toml_lua_v2
