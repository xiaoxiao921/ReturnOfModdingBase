#pragma once
#include "acceptable_value_base.hpp"

#include <logger/logger.hpp>
#include <string>

namespace toml_v2
{
	class config_description
	{
	public:
		config_description(std::string_view description, acceptable_value_base acceptableValues = {})
		{
			m_acceptable_values = acceptableValues;
			m_description       = description;
		}

		std::string m_description;

		acceptable_value_base m_acceptable_values;
	};
} // namespace toml_v2
