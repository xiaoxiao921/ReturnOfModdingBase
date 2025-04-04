#pragma once

#include "file_manager/file_manager.hpp"
#include "rom/rom.hpp"
#include "toml.hpp"

#include <ankerl/unordered_dense.h>

// clang-format off
#include <AsyncLogger/Logger.hpp>
using namespace al;
// clang-format on

namespace lua::window
{
	// mod guid -> window name -> is the window open
	inline ankerl::unordered_dense::map<std::string, ankerl::unordered_dense::map<std::string, bool>> is_open;

	inline void serialize()
	{
		try
		{
			const auto& data     = is_open;
			const auto file_name = rom::g_project_name + "-" + rom::g_project_name + "-" + "Windows.cfg";
			const auto file_path = big::g_file_manager.get_project_folder("config").get_path() / file_name;

			std::ofstream ofs(file_path);
			if (ofs.is_open())
			{
				toml::table output_table;

				for (const auto& [mod_guid, window_states] : data)
				{
					output_table.emplace(mod_guid, toml::table());
					auto window_states_table = output_table.get_as<toml::table>(mod_guid);

					for (const auto& [window_name, is_open] : window_states)
					{
						window_states_table->emplace(window_name, is_open);
					}
				}

				ofs << output_table;

				ofs.close();
				LOG(INFO) << "Window States Serialization successful.";
			}
			else
			{
				LOG(WARNING) << "Error: Unable to open lua::window file for serialization.";
			}
		}
		catch (const std::exception& e)
		{
			LOG(INFO) << "Failed serialize window states: " << e.what();
		}
	}

	inline void deserialize()
	{
		try
		{
			auto& data           = is_open;
			const auto file_name = rom::g_project_name + "-" + rom::g_project_name + "-" + "Windows.cfg";
			const auto file_path = big::g_file_manager.get_project_folder("config").get_path() / file_name;

			const auto config = toml::parse_file(file_path.c_str());

			for (const auto& [mod_guid, window_states] : config)
			{
				if (window_states.is_table())
				{
					for (const auto& [window_name, is_open] : *window_states.as_table())
					{
						if (is_open.is_boolean())
						{
							data[mod_guid.str().data()][window_name.str().data()] = is_open.as_boolean()->get();
						}
						else
						{
							LOG(WARNING) << "is_open wasnt a bool";
						}
					}
				}
				else
				{
					LOG(WARNING) << "window_states wasnt a table";
				}
			}
		}
		catch (const std::exception& e)
		{
			LOG(INFO) << "Failed deserialize window states: " << e.what();
		}
	}
} // namespace lua::window
