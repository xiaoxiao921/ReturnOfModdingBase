#pragma once
#include "lua/sol_include.hpp"

#include <string>

namespace lua::memory
{
	typedef sol::object (*type_info_feeder_t)(lua_State* state_, char* arg_ptr);

	struct type_info_t
	{
		enum type_info_id_t
		{
			none_,
			boolean_,
			string_,
			integer_,
			ptr_,
			float_,
			double_,
			custom_type_start_,
		};

		type_info_id_t m_val        = none_;
		type_info_feeder_t m_custom = nullptr;
	};

	type_info_t get_type_info_from_string(const std::string& s);

	void add_type_info_from_string(const std::string& s, type_info_feeder_t type_info_feed);
} // namespace lua::memory
