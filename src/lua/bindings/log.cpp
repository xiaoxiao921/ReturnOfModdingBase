#pragma once
#include "log.hpp"

#include "lua/lua_module.hpp"

#include <logger/logger.hpp>

namespace lua::log
{
	static void log_internal_no_guid_prefix(sol::variadic_args& args, sol::this_environment& env, al::eLogLevel level)
	{
		std::stringstream data;

		size_t i                        = 0;
		const size_t last_element_index = args.size() - 1;
		for (const auto& arg : args)
		{
			data << env.env.value()["_rom_tostring"](arg).get<const char*>();

			if (i != last_element_index)
			{
				data << '\t';
			}

			i++;
		}

		LOG(level) << data.str();
	}

	static void log_internal(sol::variadic_args& args, sol::this_environment& env, al::eLogLevel level)
	{
		std::stringstream data;

		size_t i                        = 0;
		const size_t last_element_index = args.size() - 1;
		for (const auto& arg : args)
		{
			data << env.env.value()["_rom_tostring"](arg).get<const char*>();

			if (i != last_element_index)
			{
				data << '\t';
			}

			i++;
		}

		LOG(level) << big::lua_module::guid_from(env) << ": " << data.str();
	}

	// Lua API: Table
	// Name: log
	// Table containing functions for printing to console / log file.

	// Lua API: Function
	// Table: log
	// Name: info
	// Param: args: any
	// Logs an informational message.
	static void info(sol::variadic_args args, sol::this_environment env)
	{
		log_internal(args, env, INFO);
	}

	// Lua API: Function
	// Table: log
	// Name: warning
	// Param: args: any
	// Logs a warning message.
	static void warning(sol::variadic_args args, sol::this_environment env)
	{
		log_internal(args, env, WARNING);
	}

	// Lua API: Function
	// Table: log
	// Name: debug
	// Param: args: any
	// Logs a debug message.
	static void debug(sol::variadic_args args, sol::this_environment env)
	{
		log_internal(args, env, DEBUG);
	}

	// Lua API: Function
	// Table: log
	// Name: error
	// Param: arg: any
	// Param: level: integer
	// Logs an error message. This is a mirror of lua classic `error` function.

	// Lua API: Function
	// Table: log
	// Name: refresh_filters
	// Refresh the log filters (Console and File) from the config file.
	static void refresh_filters()
	{
		if (big::g_log)
		{
			big::g_log->refresh_log_filter_values_from_config();
		}
	}

	void bind(sol::state_view& state, sol::table& lua_ext)
	{
		state["_rom_tostring"] = state["tostring"];

		// Lua API: Function
		// Table: _G
		// Name: print_raw
		// Param: args: any
		// Logs an informational message without the mod guid prefix.
		state["_rom_print_raw"] = [](sol::variadic_args args, sol::this_environment env)
		{
				log_internal_no_guid_prefix(args, env, INFO);
		};
		// Lua API: Function
		// Table: _G
		// Name: print
		// Param: args: any
		// Logs an informational message.
		state["print"] = info;

		auto ns       = lua_ext.create_named("log");
		ns["info"]    = info;
		ns["warning"] = warning;
		ns["debug"]   = debug;
		ns["error"]   = state["error"];

		ns["refresh_filters"] = refresh_filters;
	}
} // namespace lua::log
