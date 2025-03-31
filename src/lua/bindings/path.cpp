#pragma once
#include "path.hpp"

#include "directory_watcher/directory_watcher.hpp"
#include "lua/lua_manager.hpp"

#include <filesystem>

// clang-format off
#include <AsyncLogger/Logger.hpp>
using namespace al;
// clang-format on

namespace lua::path
{
	// Lua API: Table
	// Name: path
	// Table containing helpers for manipulating file or directory paths

	// Lua API: Function
	// Table: path
	// Name: combine
	// Param: path: string: Any amount of string is accepted.
	// Returns: string: Returns the combined path
	// Combines strings into a path.
	static std::string combine(sol::variadic_args args)
	{
		try
		{
			std::filesystem::path res{};

			for (const auto& arg : args)
			{
				if (arg.is<std::string>())
				{
					if (res.empty())
					{
						res = arg.as<std::string>();
					}
					else
					{
						res /= arg.as<std::string>();
					}
				}
			}

			return res.string();
		}
		catch (const std::exception& e)
		{
			LOG(WARNING) << e.what();
		}

		return "";
	}

	// Lua API: Function
	// Table: path
	// Name: get_parent
	// Param: path: string: The path for which to retrieve the parent directory.
	// Returns: string: Returns the parent path
	// Retrieves the parent directory of the specified path, including both absolute and relative paths.
	static std::string get_parent(const std::string& path)
	{
		try
		{
			std::filesystem::path res = path;
			return (char*)res.parent_path().u8string().c_str();
		}
		catch (const std::exception& e)
		{
			LOG(WARNING) << e.what();
		}

		return "";
	}

	// Lua API: Function
	// Table: path
	// Name: get_directories
	// Param: root_path: string: The path to the directory to search.
	// Returns: string table: Returns the names of subdirectories under the given root_path
	static sol::as_table_t<std::vector<std::string>> get_directories(const std::string& root_path)
	{
		std::vector<std::string> res;

		try
		{
			for (const auto& entry : std::filesystem::recursive_directory_iterator(root_path, std::filesystem::directory_options::skip_permission_denied | std::filesystem::directory_options::follow_directory_symlink))
			{
				if (!entry.is_directory())
				{
					continue;
				}

				res.push_back((char*)entry.path().u8string().c_str());
			}
		}
		catch (const std::exception& e)
		{
			LOG(WARNING) << e.what();
		}

		return res;
	}

	// Lua API: Function
	// Table: path
	// Name: get_files
	// Param: root_path: string: The path to the directory to search.
	// Returns: string table: Returns the names of all the files under the given root_path
	static sol::as_table_t<std::vector<std::string>> get_files(const std::string& root_path)
	{
		std::vector<std::string> res;

		try
		{
			for (const auto& entry : std::filesystem::recursive_directory_iterator(root_path, std::filesystem::directory_options::skip_permission_denied | std::filesystem::directory_options::follow_directory_symlink))
			{
				if (entry.is_directory())
				{
					continue;
				}

				res.push_back((char*)entry.path().u8string().c_str());
			}
		}
		catch (const std::exception& e)
		{
			LOG(WARNING) << e.what();
		}

		return res;
	}

	// Lua API: Function
	// Table: path
	// Name: filename
	// Param: path: string: The path for which to retrieve the filename.
	// Returns: string: Returns the filename identified by the path.
	static std::string filename(const std::string& path)
	{
		try
		{
			std::filesystem::path res = path;
			return (char*)res.filename().u8string().c_str();
		}
		catch (const std::exception& e)
		{
			LOG(WARNING) << e.what();
		}

		return "";
	}

	// Lua API: Function
	// Table: path
	// Name: stem
	// Param: path: string: The path for which to retrieve the stem.
	// Returns: string: Returns the stem of the filename identified by the path (i.e. the filename without the final extension).
	static std::string stem(const std::string& path)
	{
		try
		{
			std::filesystem::path res = path;
			return (char*)res.stem().u8string().c_str();
		}
		catch (const std::exception& e)
		{
			LOG(WARNING) << e.what();
		}

		return "";
	}

	// Lua API: Function
	// Table: path
	// Name: create_directory
	// Param: path: string: The path to the new directory to create.
	// Returns: boolean: true if a directory was newly created for the directory p resolves to, false otherwise.
	static bool create_directory(const std::string& path)
	{
		try
		{
			return std::filesystem::create_directories(path);
		}
		catch (const std::exception& e)
		{
			LOG(WARNING) << e.what();
		}
		catch (...)
		{
			LOG(WARNING) << "Unknown exception while creating directory " << path;
		}

		return false;
	}

	// Lua API: Function
	// Table: path
	// Name: exists
	// Param: path: string: The path to check.
	// Returns: boolean: true if the path exists, false otherwise.
	static bool exists(const std::string& path)
	{
		try
		{
			return std::filesystem::exists(path);
		}
		catch (const std::exception& e)
		{
			LOG(WARNING) << e.what();
		}
		catch (...)
		{
			LOG(WARNING) << "Unknown exception while checking existence of path " << path;
		}

		return false;
	}

	// Lua API: Function
	// Table: path
	// Name: add_file_watcher
	// Param: path: string: The path to add file watcher.
	// Param: callback: function: callback that match signature function ( file_path ).
	// Registers a callback that will be called when a file changes.
	// **Example Usage:**
	// ```lua
	// path.add_file_watcher(_ENV["!config_mod_folder_path"], function (file_path)
	// 		log.info(file_path)
	// end)
	// ```
	void add_file_watcher(std::string directory, sol::protected_function callback, sol::this_environment env)
	{
		auto mdl = big::lua_module::this_from(env);
		if (!mdl)
		{
			return;
		}
		if (!callback.valid())
		{
			return;
		}
		bool need_create = false;
		{
			std::shared_lock lock(mdl->m_file_watcher_mutex);
			need_create = mdl->m_data.m_file_watchers.count(directory) == 0;
			LOG(INFO) << "Add file watcher: " << directory;
		}
		{
			std::unique_lock lock(mdl->m_file_watcher_mutex);
			mdl->m_data.m_file_watchers[directory].push_back(callback);
		}
		if (need_create)
		{
			std::thread(
			    [directory, mdl]
			    {
				    std::vector<big::directory_watcher> watchers;
				    watchers.emplace_back(directory);
				    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory, std::filesystem::directory_options::skip_permission_denied | std::filesystem::directory_options::follow_directory_symlink))
				    {
					    if (!entry.is_directory())
					    {
						    continue;
					    }

					    watchers.emplace_back(entry.path());
				    }

				    while (true)
				    {
					    {
						    std::shared_lock lock(mdl->m_file_watcher_mutex);
						    if (mdl->m_data.m_file_watchers.count(directory) == 0)
						    {
							    break;
						    }

						    for (auto& watcher : watchers)
						    {
							    auto modified_files = watcher.check();
							    for (const auto& file_path : modified_files)
							    {
								    std::lock_guard<std::mutex> lock(big::g_lua_manager->m_to_do_file_callback_lock);
								    std::string file_name = file_path.string().substr(directory.size() + 1);

								    big::g_lua_manager->m_to_do_file_callback_queue.push(std::make_tuple(mdl, directory, file_name));
							    }
						    }
					    }
					    using namespace std::chrono_literals;
					    std::this_thread::sleep_for(500ms);
				    }
			    })
			    .detach();
		}
	}

	void bind(sol::table& state)
	{
		auto ns = state.create_named("path");

		ns["combine"]          = combine;
		ns["get_parent"]       = get_parent;
		ns["get_directories"]  = get_directories;
		ns["get_files"]        = get_files;
		ns["filename"]         = filename;
		ns["stem"]             = stem;
		ns["create_directory"] = create_directory;
		ns["exists"]           = exists;
		ns["add_file_watcher"] = add_file_watcher;
	}
} // namespace lua::path
