#pragma once
#include "acceptable_value_base.hpp"

#include <string>

namespace toml_v2
{
	class config_description
	{
	public:
		std::string m_description;

		acceptable_value_base m_acceptable_values;

		config_description(std::string_view description, acceptable_value_base acceptableValues = {})
		{
			m_acceptable_values = acceptableValues;
			m_description       = description;
		}
	};
} // namespace toml_v2
