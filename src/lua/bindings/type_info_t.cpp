#include "type_info_t.hpp"

#include <unordered_map>

namespace lua::memory
{
	static std::unordered_map<std::string, type_info_t> string_to_type_info_id;

	type_info_t get_type_info_from_string(const std::string& s)
	{
		const auto it = string_to_type_info_id.find(s);
		if (it != string_to_type_info_id.end())
		{
			return it->second;
		}

		if ((s.contains("const") && s.contains("char") && s.contains("*")) || s.contains("string"))
		{
			return {type_info_t::string_};
		}
		else if (s.contains("bool"))
		{
			return {type_info_t::boolean_};
		}
		else if (s.contains("ptr") || s.contains("pointer") || s.contains("*"))
		{
			// passing lua::memory::pointer
			return {type_info_t::ptr_};
		}
		else if (s.contains("float"))
		{
			return {type_info_t::float_};
		}
		else if (s.contains("double"))
		{
			return {type_info_t::double_};
		}
		else
		{
			return {type_info_t::integer_};
		}
	}

	void add_type_info_from_string(const std::string& s, type_info_feeder_t type_info_feed)
	{
		string_to_type_info_id[s] = type_info_t{.m_val = type_info_t::custom_type_start_, .m_custom = type_info_feed};
	}
} // namespace lua::memory
