#pragma once
#include <logger/logger.hpp>
#include <string>

namespace big::string_conversions
{
	inline std::string utf16_to_utf8(const std::wstring& utf16)
	{
		std::string res;
		const int sizeRequired = WideCharToMultiByte(CP_UTF8, 0, (const wchar_t*)utf16.data(), (int)utf16.size(), NULL, 0, NULL, NULL);
		if (sizeRequired != 0)
		{
			res = std::string(sizeRequired, 0);
			WideCharToMultiByte(CP_UTF8, 0, (const wchar_t*)utf16.data(), (int)utf16.size(), res.data(), sizeRequired, NULL, NULL);
		}
		return res;
	}

	inline std::wstring utf8_to_utf16(const std::string& utf8) noexcept
	{
		std::wstring utf16;
		const int sizeRequired = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), nullptr, 0);
		if (sizeRequired != 0)
		{
			utf16 = std::wstring(sizeRequired, 0);
			MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), utf16.data(), sizeRequired);
		}
		return utf16;
	}

	inline std::string utf_16_to_code_page(uint32_t code_page, std::wstring_view input)
	{
		if (input.empty())
		{
			return {};
		}

		const auto size = WideCharToMultiByte(code_page, 0, input.data(), static_cast<int>(input.size()), nullptr, 0, nullptr, nullptr);

		std::string output(size, '\0');

		if (size
		    != WideCharToMultiByte(code_page,
		                           0,
		                           input.data(),
		                           static_cast<int>(input.size()),
		                           output.data(),
		                           static_cast<int>(output.size()),
		                           nullptr,
		                           nullptr))
		{
			const auto error_code = GetLastError();
			LOG(WARNING) << "WideCharToMultiByte Error in String " << error_code;
			return {};
		}

		return output;
	}
} // namespace big::string_conversions
