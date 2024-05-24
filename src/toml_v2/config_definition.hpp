#pragma once
#include <AsyncLogger/Logger.hpp>
#include <map>
#include <string/string.hpp>
using namespace al;
#include <string>

namespace toml_v2
{
	class config_definition
	{
		static inline auto _invalidConfigChars = std::to_array({'=', '\n', '\t', '\\', '"', '\'', '[', ']'});

	public:
		std::string m_key;
		std::string m_section;

		config_definition(std::string_view section, std::string_view key)
		{
			CheckInvalidConfigChars(section, "section");
			CheckInvalidConfigChars(key, "key");
			m_key     = key;
			m_section = section;
		}

		static inline void CheckInvalidConfigChars(std::string_view val, std::string_view name)
		{
			if (val.empty())
			{
				LOG(ERROR) << "invalid empty val";
				return;
			}

			std::string val_trimed = val.data();
			big::string::trim(val_trimed);
			if (val != val_trimed)
			{
				LOG(ERROR) << "Cannot use whitespace characters at start or end of section and key names: " << val << " | " << name;
				return;
			}

			for (const auto c : val)
			{
				for (const auto cc : _invalidConfigChars)
				{
					if (c == cc)
					{
						LOG(ERROR) << R"(Cannot use any of the following characters in section and key names: = \n \t \ "" ' [ ])";
						return;
					}
				}
			}
		}

		// Define comparison operator for std::map
		bool operator<(const config_definition& other) const
		{
			if (m_section < other.m_section)
			{
				return true;
			}
			if (m_section > other.m_section)
			{
				return false;
			}
			return m_key < other.m_key;
		}
	};
} // namespace toml_v2
