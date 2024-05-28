#pragma once
#include "config_definition.hpp"
#include "config_description.hpp"
#include "toml_type_converter.hpp"

#include <any>
#include <filesystem>
#include <fstream>
#include <map>
#include <string/string.hpp>
#include <string>
#include <typeindex>
#include <unordered_set>

namespace toml_v2
{
	class config_file
	{
	public:
		class config_entry_base
		{
		public:
			config_file* m_config_file;
			config_definition m_definition;
			config_description m_description;

		private:
			std::shared_ptr<std::any> m_default_value;
			std::shared_ptr<std::any> m_boxed_value;

		public:

			std::function<void(config_entry_base*)> m_setting_changed;

			config_entry_base(config_file* configFile, config_definition definition, const std::any& default_value, config_description description) :
			    m_definition(definition),
			    m_description(description)
			{
				m_config_file   = configFile;
				m_default_value = std::make_shared<std::any>(default_value);
				m_boxed_value   = std::make_shared<std::any>(default_value);

				configFile->m_on_setting_changed.push_back(
				    [this](config_entry_base* cfg_entry)
				    {
					    if (cfg_entry == this && m_setting_changed)
					    {
						    m_setting_changed(cfg_entry);
					    }
				    });
			}

			auto& type()
			{
				return m_boxed_value->type();
			}

			template<typename T>
			T get_value_base()
			{
				return std::any_cast<T>(*m_boxed_value);
			}

			template<typename T>
			void set_value_base(T new_value)
			{
				// TODO: clamp value if needed before assigning.

				if (m_boxed_value->type() == typeid(T) && get_value_base<T>() == new_value)
				{
					return;
				}

				*m_boxed_value = new_value;
				OnSettingChanged();
			}

			std::string get_serialized_value()
			{
				return toml_type_converter::convert_to_string(*m_boxed_value);
			}

			void set_serialized_value(const std::string& value)
			{
				auto opt_res = toml_type_converter::convert_to_value(*m_default_value, value);
				if (opt_res)
				{
					// TODO: clamp value if needed before assigning.

					*m_boxed_value = *opt_res;
					OnSettingChanged();
				}
			}

			void OnSettingChanged()
			{
				m_config_file->on_setting_changed(this);
			}

			template<typename T>
			void write_description(T& stream_)
			{
				if (m_description.m_description.size())
				{
					stream_ << "## " << big::string::replace(m_description.m_description, "\n", "\n## ") << std::endl;
				}

				if (type() == typeid(std::string))
				{
					stream_ << "# Setting type: string" << std::endl;
				}
				else
				{
					stream_ << "# Setting type: " << type().name() << std::endl;
				}

				stream_ << "# Default value: " << toml_type_converter::convert_to_string(*m_default_value);

				// todo acceptable values
			}
		};

		template<typename ValueType>
		class config_entry : public config_entry_base
		{
		public:
			config_entry(config_file* configFile, config_definition definition, ValueType default_value, config_description description) :
			    config_entry_base(configFile, definition, std::any(default_value), description)
			{
			}

			ValueType get_value()
			{
				return get_value_base<ValueType>();
			}

			void set_value(ValueType new_value)
			{
				set_value_base(new_value);
			}
		};

		template<>
		class config_entry<const char*> : public config_entry_base
		{
		public:
			config_entry(config_file* configFile, config_definition definition, const char* default_value, config_description description) :
			    config_entry_base(configFile, definition, std::any(std::string(default_value)), description)
			{
			}

			std::string get_value()
			{
				return get_value_base<std::string>();
			}

			void set_value(std::string_view new_value)
			{
				set_value_base<std::string>(std::string(new_value));
			}
		};

	public:
		std::string m_owner_guid;

	private:
		std::mutex m_io_lock;

	public:
		std::map<config_definition, std::shared_ptr<config_entry_base>> m_entries;
		std::map<config_definition, std::string> m_orphaned_entries;

		std::filesystem::path m_config_file_path;

		bool m_save_on_config_set = true;

		std::vector<std::function<void()>> m_on_config_reloaded;
		std::vector<std::function<void(config_entry_base*)>> m_on_setting_changed;

		static inline std::vector<toml_v2::config_file*> g_config_files;

		config_file(std::string_view config_path, bool save_on_init, std::string_view owner_guid);

		~config_file()
		{
			std::erase_if(g_config_files,
			              [&](auto& cfg)
			              {
				              return cfg == this;
			              });
		}

		config_entry_base* try_get_entry(config_definition& key)
		{
			const auto it_entry = m_entries.find(key);
			if (it_entry != m_entries.end())
			{
				return it_entry->second.get();
			}

			return nullptr;
		}

		config_entry_base* operator[](config_definition& key)
		{
			return try_get_entry(key);
		}

		bool contains(config_definition& key) const
		{
			return m_entries.contains(key);
		}

		void remove(config_definition& key)
		{
			if (m_entries.erase(key))
			{
				if (m_save_on_config_set)
				{
					save();
				}
			}
		}

		int count() const
		{
			return m_entries.size();
		}

		void clear()
		{
			m_entries.clear();
		}

		void reload()
		{
			m_orphaned_entries.clear();

			std::string current_section = "";

			std::ifstream cfg_file_stream(m_config_file_path);

			if (!cfg_file_stream.is_open())
			{
				return;
			}

			std::string line;
			while (std::getline(cfg_file_stream, line))
			{
				big::string::trim(line);

				// comment
				if (line.starts_with("#"))
				{
					continue;
				}

				// section
				if (line.starts_with("[") && line.ends_with("]"))
				{
					current_section = line.substr(1, line.size() - 2);
					continue;
				}

				auto split = big::string::split(line, '=', 2);
				if (split.size() != 2)
				{
					// empty / invalid line
					continue;
				}

				auto& currentKey = split[0];
				big::string::trim(currentKey);

				auto& currentValue = split[1];
				big::string::trim(currentValue);

				auto definition = config_definition(current_section, currentKey);

				auto it_entry = m_entries.find(definition);
				if (it_entry != m_entries.end())
				{
					it_entry->second->set_serialized_value(currentValue);
				}
				else
				{
					m_orphaned_entries.emplace(definition, currentValue);
				}
			}

			cfg_file_stream.close();

			on_config_reloaded();
		}

		void save();

		template<typename ValueType>
		config_entry<ValueType>* bind(const std::string& section, const std::string& key, ValueType defaultValue, const std::string& description)
		{
			auto config_def     = config_definition(section, key);
			auto existing_entry = try_get_entry(config_def);
			if (existing_entry)
			{
				return (config_entry<ValueType>*)existing_entry;
			}

			auto entry = std::make_shared<config_entry<ValueType>>(this, config_def, defaultValue, config_description(description));

			m_entries.emplace(config_def, entry);

			auto homeless_value = m_orphaned_entries.find(config_def);
			if (homeless_value != m_orphaned_entries.end())
			{
				entry->set_serialized_value(homeless_value->second);
				m_orphaned_entries.erase(homeless_value);
			}

			if (m_save_on_config_set)
			{
				save();
			}

			return entry.get();
		}

		void on_setting_changed(config_entry_base* changed_entry_base)
		{
			if (m_save_on_config_set)
			{
				save();
			}

			for (auto& ev : m_on_setting_changed)
			{
				ev(changed_entry_base);
			}
		}

		void on_config_reloaded()
		{
			for (auto& ev : m_on_config_reloaded)
			{
				ev();
			}
		}
	};
} // namespace toml_v2
