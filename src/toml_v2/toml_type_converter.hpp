#pragma once
#include <any>
#include <charconv>
#include <logger/logger.hpp>
#include <map>
#include <string>
#include <typeindex>

namespace toml_v2
{
	class toml_type_converter
	{
	public:
		template<class T, class F>
		static inline std::pair<const std::type_index, std::function<std::string(const std::any&)>> to_any_visitor_ser(const F& f)
		{
			return {std::type_index(typeid(T)),
			        [g = f](const std::any& a)
			        {
				        if constexpr (std::is_void_v<T>)
				        {
					        return g();
				        }
				        else
				        {
					        return g(std::any_cast<T const&>(a));
				        }
			        }};
		}

		static inline std::unordered_map<std::type_index, std::function<std::string(const std::any&)>> any_visitor_ser{
		    to_any_visitor_ser<const char*>(
		        [](const char* x) -> std::string
		        {
			        return x;
		        }),
		    to_any_visitor_ser<std::string>(
		        [](std::string x) -> std::string
		        {
			        return x;
		        }),
		    to_any_visitor_ser<bool>(
		        [](bool x) -> std::string
		        {
			        return x ? "true" : "false";
		        }),
		    to_any_visitor_ser<uint8_t>(
		        [](uint8_t x) -> std::string
		        {
			        // use std format instead of to_string for making it locale invariant:
			        // C locale might be set by rombase lib user and
			        // will cause issues on config files shared between users with diff locales.

			        return std::format("{}", x);
		        }),
		    to_any_visitor_ser<int8_t>(
		        [](int8_t x) -> std::string
		        {
			        return std::format("{}", x);
		        }),
		    to_any_visitor_ser<int16_t>(
		        [](int16_t x) -> std::string
		        {
			        return std::format("{}", x);
		        }),
		    to_any_visitor_ser<uint16_t>(
		        [](uint16_t x) -> std::string
		        {
			        return std::format("{}", x);
		        }),
		    to_any_visitor_ser<int32_t>(
		        [](int32_t x) -> std::string
		        {
			        return std::format("{}", x);
		        }),
		    to_any_visitor_ser<uint32_t>(
		        [](uint32_t x) -> std::string
		        {
			        return std::format("{}", x);
		        }),
		    to_any_visitor_ser<int64_t>(
		        [](int64_t x) -> std::string
		        {
			        return std::format("{}", x);
		        }),
		    to_any_visitor_ser<uint64_t>(
		        [](uint64_t x) -> std::string
		        {
			        return std::format("{}", x);
		        }),
		    to_any_visitor_ser<float>(
		        [](float x) -> std::string
		        {
			        return std::format("{}", x);
		        }),
		    to_any_visitor_ser<double>(
		        [](double x) -> std::string
		        {
			        return std::format("{}", x);
		        }),
		    to_any_visitor_ser<void>(
		        []() -> std::string
		        {
			        return "{}";
		        }),
		};

		static inline std::string convert_to_string(std::any& val)
		{
			// https://en.cppreference.com/w/cpp/utility/any/type

			if (const auto it = any_visitor_ser.find(std::type_index(val.type())); it != any_visitor_ser.end())
			{
				try
				{
					return it->second(val);
				}
				catch (const std::exception& e)
				{
					LOG(WARNING) << e.what();
				}

				return "";
			}

			LOGF(WARNING, "Unregistered type: {}", val.type().name());

			return "";
		}

		template<class T, class F>
		static inline std::pair<const std::type_index, std::function<std::any(const std::string&)>> to_any_visitor_deser(const F& f)
		{
			return {std::type_index(typeid(T)),
			        [g = f](const std::string& a)
			        {
				        if constexpr (std::is_void_v<T>)
				        {
					        return g();
				        }
				        else
				        {
					        return g(a);
				        }
			        }};
		}

		static inline std::unordered_map<std::type_index, std::function<std::any(const std::string&)>> any_visitor_deser{
		    to_any_visitor_deser<std::string>(
		        [](const std::string& x) -> std::any
		        {
			        return std::string(x);
		        }),
		    to_any_visitor_deser<bool>(
		        [](const std::string& x) -> std::any
		        {
			        return x == "true" ? true : false;
		        }),
		    to_any_visitor_deser<int8_t>(
		        [](const std::string& x) -> std::any
		        {
			        // use from_chars instead of to_string for making it locale invariant:
			        // C locale might be set by rombase lib user and
			        // will cause issues on config files shared between users with diff locales.

			        int8_t result{};
			        auto [ptr, ec] = std::from_chars(x.data(), x.data() + x.size(), result);
			        return result;
		        }),
		    to_any_visitor_deser<uint8_t>(
		        [](const std::string& x) -> std::any
		        {
			        uint8_t result{};
			        auto [ptr, ec] = std::from_chars(x.data(), x.data() + x.size(), result);
			        return result;
		        }),
		    to_any_visitor_deser<int16_t>(
		        [](const std::string& x) -> std::any
		        {
			        int16_t result{};
			        auto [ptr, ec] = std::from_chars(x.data(), x.data() + x.size(), result);
			        return result;
		        }),
		    to_any_visitor_deser<uint16_t>(
		        [](const std::string& x) -> std::any
		        {
			        uint16_t result{};
			        auto [ptr, ec] = std::from_chars(x.data(), x.data() + x.size(), result);
			        return result;
		        }),
		    to_any_visitor_deser<int32_t>(
		        [](const std::string& x) -> std::any
		        {
			        int32_t result{};
			        auto [ptr, ec] = std::from_chars(x.data(), x.data() + x.size(), result);
			        return result;
		        }),
		    to_any_visitor_deser<uint32_t>(
		        [](const std::string& x) -> std::any
		        {
			        uint32_t result{};
			        auto [ptr, ec] = std::from_chars(x.data(), x.data() + x.size(), result);
			        return result;
		        }),
		    to_any_visitor_deser<int64_t>(
		        [](const std::string& x) -> std::any
		        {
			        int64_t result{};
			        auto [ptr, ec] = std::from_chars(x.data(), x.data() + x.size(), result);
			        return result;
		        }),
		    to_any_visitor_deser<uint64_t>(
		        [](const std::string& x) -> std::any
		        {
			        uint64_t result{};
			        auto [ptr, ec] = std::from_chars(x.data(), x.data() + x.size(), result);
			        return result;
		        }),
		    to_any_visitor_deser<float>(
		        [](const std::string& x) -> std::any
		        {
			        float result{};
			        auto [ptr, ec] = std::from_chars(x.data(), x.data() + x.size(), result);
			        return result;
		        }),
		    to_any_visitor_deser<double>(
		        [](const std::string& x) -> std::any
		        {
			        double result{};
			        auto [ptr, ec] = std::from_chars(x.data(), x.data() + x.size(), result);
			        return result;
		        }),
		    to_any_visitor_deser<void>(
		        []() -> std::any
		        {
			        return "{}";
		        }),
		};

		static inline std::optional<std::any> convert_to_value(std::any& default_value, const std::string& value)
		{
			// https://en.cppreference.com/w/cpp/utility/any/type

			if (const auto it = any_visitor_deser.find(std::type_index(default_value.type())); it != any_visitor_deser.end())
			{
				try
				{
					return it->second(value);
				}
				catch (const std::exception& e)
				{
					LOG(WARNING) << e.what();
				}
			}

			LOGF(WARNING, "Unregistered type: {}", default_value.type().name());

			return {};
		}
	};
} // namespace toml_v2
