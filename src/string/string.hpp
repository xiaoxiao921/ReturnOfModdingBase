#pragma once

#include <locale>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace big::string
{
	inline bool starts_with(const char* pre, const char* str)
	{
		return strncmp(pre, str, strlen(pre)) == 0;
	}

	template<typename T>
	inline std::enable_if_t<std::is_same_v<T, std::string>, T> get_text_value(std::string text)
	{
		return text;
	}

	template<typename T>
	inline std::enable_if_t<!std::is_same_v<T, std::string>, T> get_text_value(std::string text)
	{
		T value = (T)0;
		std::stringstream(text) >> value;
		return value;
	}

	template<typename T = std::string>
	inline std::vector<T> split(const std::string& text, const char delim, size_t max_splits = std::string::npos)
	{
		std::vector<T> result;
		std::string str;
		std::stringstream ss(text);
		size_t splits = 0;

		max_splits--;

		while (std::getline(ss, str, delim))
		{
			if (splits < max_splits)
			{
				result.push_back(get_text_value<T>(str));
				splits++;
			}
			else
			{
				std::string remaining;
				if (std::getline(ss, remaining, '\0'))
				{
					str += delim + remaining;
				}
				result.push_back(get_text_value<T>(str));
				break;
			}
		}

		return result;
	}

	template<typename T = std::string>
	inline std::vector<T> split(const std::string& text, const std::string& delim, size_t max_splits = std::string::npos)
	{
		std::vector<T> result;
		size_t start  = 0, end;
		size_t splits = 0;

		max_splits--;

		while ((end = text.find(delim, start)) != std::string::npos && splits < max_splits)
		{
			result.push_back(get_text_value<T>(text.substr(start, end - start)));
			start = end + delim.length();
			splits++;
		}

		// Add the remaining part
		result.push_back(get_text_value<T>(text.substr(start)));

		return result;
	}

	inline std::string replace(const std::string& original, const std::string& old_value, const std::string& new_value)
	{
		std::string result = original;
		size_t pos         = 0;

		while ((pos = result.find(old_value, pos)) != std::string::npos)
		{
			result.replace(pos, old_value.length(), new_value);
			pos += new_value.length();
		}

		return result;
	}

	inline std::string to_lower(const std::string& input)
	{
		std::string result = input;

		// Use the std::locale to ensure proper case conversion for the current locale
		std::locale loc;

		for (size_t i = 0; i < result.length(); ++i)
		{
			result[i] = std::tolower(result[i], loc);
		}

		return result;
	}

	inline std::string to_upper(const std::string& input)
	{
		std::string result = input;

		// Use the std::locale to ensure proper case conversion for the current locale
		std::locale loc;

		for (size_t i = 0; i < result.length(); ++i)
		{
			result[i] = std::toupper(result[i], loc);
		}

		return result;
	}

	// trim from start (in place)
	inline void ltrim(std::string& s)
	{
		s.erase(s.begin(),
		        std::find_if(s.begin(),
		                     s.end(),
		                     [](unsigned char ch)
		                     {
			                     return !std::isspace(ch);
		                     }));
	}

	// trim from end (in place)
	inline void rtrim(std::string& s)
	{
		s.erase(std::find_if(s.rbegin(),
		                     s.rend(),
		                     [](unsigned char ch)
		                     {
			                     return !std::isspace(ch);
		                     })
		            .base(),
		        s.end());
	}

	// trim from both ends (in place)
	inline void trim(std::string& s)
	{
		rtrim(s);
		ltrim(s);
	}
} // namespace big::string
