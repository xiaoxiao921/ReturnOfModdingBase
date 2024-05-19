#include "config_file.hpp"

toml_v2::config_file::config_file(std::string_view config_path, bool save_on_init, std::string_view owner_guid)
{
	m_owner_guid = owner_guid;

	if (config_path.empty())
	{
		LOG(FATAL) << "config_path cannot be empty";
		return;
	}

	if (!config_path.ends_with(".cfg"))
	{
		LOGF(WARNING, "It's recommended to use `.cfg` as the file extension (Owner GUID: {}) (Current file path: {}). The mod manager will pick it up and make it show nicely inside the mod manager UI.", owner_guid, config_path);
	}

	m_config_file_path = std::filesystem::absolute(config_path);

	if (std::filesystem::exists(m_config_file_path))
	{
		reload();
	}
	else if (save_on_init)
	{
		save();
	}

	g_config_files.push_back(this);
}

void toml_v2::config_file::save()
{
	try
	{
		std::filesystem::create_directories(m_config_file_path.parent_path());
	}
	catch (const std::exception& e)
	{
		LOG(FATAL) << "Failed writing config file: " << e.what();
		return;
	}

	std::ofstream writer(m_config_file_path, std::ios::trunc);

	if (!writer.is_open())
	{
		LOG(FATAL) << "failed open";
		return;
	}

	if (m_owner_guid.size())
	{
		writer << "## Settings file was created by plugin " << m_owner_guid << std::endl;
	}

	struct all_config_entry_t
	{
		config_definition m_key;
		std::shared_ptr<config_entry_base> m_entry;
		std::string m_value;
	};

	std::map<config_definition, all_config_entry_t> allConfigEntries;
	for (const auto& [k, v] : m_entries)
	{
		allConfigEntries.emplace(k, all_config_entry_t{.m_key = k, .m_entry = v, .m_value = v->get_serialized_value()});
	}
	for (const auto& [k, v] : m_orphaned_entries)
	{
		allConfigEntries.emplace(k, all_config_entry_t{.m_key = k, .m_entry = nullptr, .m_value = v});
	}

	std::unordered_set<std::string> already_written_section_headers;
	for (const auto& [k, v] : allConfigEntries)
	{
		if (!already_written_section_headers.contains(k.m_section))
		{
			writer << std::endl;
			writer << std::format("[{}]", k.m_section) << std::endl;
			already_written_section_headers.insert(k.m_section);
		}

		writer << std::endl;
		if (v.m_entry)
		{
			v.m_entry->write_description(writer);
			writer << std::endl;
		}

		writer << std::format("{} = {}", v.m_key.m_key, v.m_value) << std::endl;
	}

	writer.close();
}
