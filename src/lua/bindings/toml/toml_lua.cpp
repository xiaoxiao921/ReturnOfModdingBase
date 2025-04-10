/*
https://github.com/LebJe/toml.lua/releases/tag/0.4.0

MIT License

Copyright(c) 2024 Jeff Lebrun

Permission is hereby granted,
free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and / or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions :

The above copyright notice and this permission notice shall be included in all copies
or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS",
WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "sol/forward.hpp"

#include <cstddef>
#include <exception>
#include <fstream>
#include <iostream>
#include <lua/bindings/toml/DataTypes/DateAndTime/dateAndTime.hpp>
#include <lua/bindings/toml/DataTypes/TOMLInt/TOMLInt.hpp>
#include <lua/bindings/toml/decoding/decoding.hpp>
#include <lua/bindings/toml/encoding/encoding.hpp>
#include <lua/bindings/toml/utilities/utilities.hpp>
#include <optional>
#include <string>
#include <filesystem>
#include <chrono>
#include <toml.hpp>

template<class T>
inline int tomlTo(sol::state_view state, toml::format_flags flags)
{
	auto L = state.lua_state();

	toml::table* table = nullptr;

	if (auto t = sol::stack::get<std::optional<sol::table>>(L, 1))
	{
		table = tomlTableFromLuaTable(*t);
	}
	else if (auto s = sol::stack::get<std::optional<std::string>>(L, 1))
	{
		try
		{
			table = new toml::table(toml::parse(*s));
		}
		catch (toml::parse_error& e)
		{
			auto table = state.create_table();
			parseErrorToTable(e, table);
			sol::stack::push(state.lua_state(), table);
			return lua_error(state.lua_state());
		}
	}
	else
	{
		return luaL_argerror(
		    L,
		    1,
		    std::string(std::string("A string containing a TOML document, or a table with strings as keys "
		                            "should be the first argument, not")
		                + solLuaDataTypeToString(sol::type_of(L, 1)))
		        .c_str());
	}

	std::stringstream ss;

	ss << T(*table, flags);
	return sol::stack::push(L, ss.str());
};

#ifdef __cplusplus
extern "C"
{
#endif

	int encode(lua_State* L)
	{
		sol::state_view state(L);

		if (auto table = sol::stack::get<std::optional<sol::table>>(L, 1))
		{
			auto flags = tableToFormatFlags(sol::stack::check_get<sol::table>(L, 2));

			toml::table* tomlTable;

			try
			{
				tomlTable = tomlTableFromLuaTable(*table);
			}
			catch (std::exception& e)
			{
				return luaL_error(L, (std::string("An error occurred during encoding: ") + e.what()).c_str());
			}

			std::stringstream ss;

			ss << toml::toml_formatter(*tomlTable, flags);

			return sol::stack::push(L, ss.str());
		}
		else
		{
			return luaL_argerror(
			    L,
			    1,
			    std::string(std::string("A Lua table with strings as keys should be the first argument, not ") + solLuaDataTypeToString(sol::type_of(L, 1)))
			        .c_str());
		}
	}

	int encodeToFile(lua_State* L)
	{
		sol::state_view state(L);

		if (auto table = sol::stack::get<std::optional<sol::table>>(L, 1))
		{
			std::string filePath;
			bool overwrite = false;

			if (auto fp = sol::stack::get<std::optional<std::string>>(L, 2))
			{
				filePath = *fp;
			}
			else if (auto fileAndOptions = sol::stack::get<std::optional<sol::table>>(L, 2))
			{
				if ((*fileAndOptions)["file"].get_type() == sol::type::string)
				{
					filePath = (*fileAndOptions)["file"].get<std::string>();
				}
				else
				{
					return luaL_argerror(L,
					                     2,
					                     "The key \"file\" in the second argument to `toml.encodeToFile` is either "
					                     "missing, or it's value is invalid");
				}
				overwrite = (*fileAndOptions)["overwrite"].get_or(false);
			}
			else
			{
				return luaL_argerror(L,
				                     2,
				                     std::string(std::string("A file path (string), or a table should be the second "
				                                             "argument, not ")
				                                 + solLuaDataTypeToString(sol::type_of(L, 2)))
				                         .c_str());
			}
			auto flags = tableToFormatFlags(sol::stack::check_get<sol::table>(L, 3));

			toml::table* tomlTable;

			try
			{
				tomlTable = tomlTableFromLuaTable(*table);
			}
			catch (std::exception& e)
			{
				return luaL_error(L, (std::string("An error occurred during encoding: ") + e.what()).c_str());
			}

			std::ofstream ofs = std::ofstream(filePath, overwrite ? std::ios::out : std::ios::app);

			if (ofs.is_open())
			{
				ofs << toml::toml_formatter(*tomlTable, flags);
				ofs.close();
			}
			else
			{
				return luaL_error(L, (std::string("The file \"") + filePath + "\" cannot be opened for writing.").c_str());
			}
			
			std::filesystem::path filePathPath(filePath);
			std::filesystem::file_time_type ftime = std::filesystem::last_write_time(filePathPath);
			auto systemTime = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
			std::time_t timestamp = std::chrono::system_clock::to_time_t(systemTime);
			return sol::stack::push(state.lua_state(), timestamp);
		}
		else
		{
			return luaL_argerror(
			    L,
			    1,
			    std::string(std::string("A Lua table with strings as keys should be the first argument, not ") + solLuaDataTypeToString(sol::type_of(L, 1)))
			        .c_str());
		}

		return 0;
	}

	int decode(lua_State* L)
	{
		sol::state_view state(L);
		auto res = getTableFromStringInState(state);

		try
		{
			try
			{
				auto tomlTable = std::get<toml::table*>(res);

				auto luaTable = state.create_table();

				auto options = tableToOptions(sol::stack::check_get<sol::table>(L, 2));
				tomlToLuaTable(tomlTable, luaTable, options);

				return luaTable.push();
			}
			catch (std::bad_variant_access)
			{
				return std::get<int>(res);
			}
		}
		catch (std::exception& e)
		{
			return luaL_error(L, (std::string("An error occurred during decoding: ") + e.what()).c_str());
		}
	}

	int decodeFromFile(lua_State* L)
	{
		sol::state_view state(L);

		if (auto filePath = sol::stack::get<std::optional<std::string>>(L, 1))
		{
			auto options = tableToOptions(sol::stack::check_get<sol::table>(L, 2));

			try
			{
				toml::table tomlTable = toml::parse_file(*filePath);
				auto luaTable         = state.create_table();
				tomlToLuaTable(&tomlTable, luaTable, options);

				return luaTable.push();
			}
			catch (toml::parse_error& e)
			{
				auto source = e.source();

				auto table = state.create_table();

				parseErrorToTable(e, table);

				sol::stack::push(state.lua_state(), table);
				return lua_error(state.lua_state());
			}
			catch (std::exception& e)
			{
				return luaL_error(L, (std::string("An error occurred during decoding: ") + e.what()).c_str());
			}
		}
		else
		{
			return luaL_argerror(L,
			                     1,
			                     std::string(std::string("A file path (string) should be the first argument, not ") + solLuaDataTypeToString(sol::type_of(L, 1)))
			                         .c_str());
		}
	}

	int toJSON(lua_State* L)
	{
		auto flags = tableToFormatFlags(sol::stack::check_get<sol::table>(L, 2));
		return tomlTo<toml::json_formatter>(sol::state_view(L), flags);
	}

	int toYAML(lua_State* L)
	{
		auto flags = tableToFormatFlags(sol::stack::check_get<sol::table>(L, 2));
		return tomlTo<toml::yaml_formatter>(sol::state_view(L), flags);
	}

#ifdef __cplusplus
}
#endif

namespace lua::toml_lua
{
	void bind(sol::table& state)
	{
		sol::table module = state.create_named("toml");

		// Setup functions.

		module["encode"]         = &encode;
		module["encodeToFile"]   = &encodeToFile;
		module["decode"]         = &decode;
		module["decodeFromFile"] = &decodeFromFile;
		module["toJSON"]         = &toJSON;
		module["toYAML"]         = &toYAML;

		// Setup formatting flags

		auto formattingTable = module.create_named("formatting");

		formattingTable.new_enum<toml::value_flags>("int", {{"binary", toml::value_flags::format_as_binary}, {"hexadecimal", toml::value_flags::format_as_hexadecimal}, {"octal", toml::value_flags::format_as_octal}});

		// Setup UserType - Int

		sol::usertype<TOMLInt> tomlInt = module.new_usertype<TOMLInt>("Int", sol::constructors<TOMLInt(int64_t), TOMLInt(int64_t, uint16_t)>());

		tomlInt["int"]   = sol::property(&TOMLInt::getInt, &TOMLInt::setInt);
		tomlInt["flags"] = sol::property(&TOMLInt::getFlags, &TOMLInt::setFlags);

		// Setup UserType - Date

		sol::usertype<TOMLDate> tomlDate = module.new_usertype<TOMLDate>("Date", sol::constructors<TOMLDate(uint16_t, uint8_t, uint8_t)>());

		tomlDate["year"]  = sol::property(&TOMLDate::getYear, &TOMLDate::setYear);
		tomlDate["month"] = sol::property(&TOMLDate::getMonth, &TOMLDate::setMonth);
		tomlDate["day"]   = sol::property(&TOMLDate::getDay, &TOMLDate::setDay);

		// Setup UserType - Time

		sol::usertype<TOMLTime> tomlTime = module.new_usertype<TOMLTime>("Time", sol::constructors<TOMLTime(uint8_t, uint8_t, uint8_t, uint16_t)>());

		tomlTime["hour"]       = sol::property(&TOMLTime::getHour, &TOMLTime::setHour);
		tomlTime["minute"]     = sol::property(&TOMLTime::getMinute, &TOMLTime::setMinute);
		tomlTime["second"]     = sol::property(&TOMLTime::getSecond, &TOMLTime::setSecond);
		tomlTime["nanoSecond"] = sol::property(&TOMLTime::getNanoSecond, &TOMLTime::setNanoSecond);

		// Setup UserType - TimeOffset

		sol::usertype<TOMLTimeOffset> tomlTimeOffset = module.new_usertype<TOMLTimeOffset>("TimeOffset", sol::constructors<TOMLTimeOffset(int8_t, int8_t)>());

		tomlTimeOffset["minutes"] = sol::property(&TOMLTimeOffset::minutes);

		// Setup UserType - DateTime

		sol::usertype<TOMLDateTime> tomlDateTime = module.new_usertype<TOMLDateTime>("DateTime", sol::constructors<TOMLDateTime(TOMLDate, TOMLTime), TOMLDateTime(TOMLDate, TOMLTime, TOMLTimeOffset)>());

		tomlDateTime["date"]       = sol::property(&TOMLDateTime::getDate, &TOMLDateTime::setDate);
		tomlDateTime["time"]       = sol::property(&TOMLDateTime::getTime, &TOMLDateTime::setTime);
		tomlDateTime["timeOffset"] = sol::property(&TOMLDateTime::getTimeOffset, &TOMLDateTime::setTimeOffset);
	}
} // namespace lua::toml_lua
