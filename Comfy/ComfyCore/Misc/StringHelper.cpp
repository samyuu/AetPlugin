#include "StringHelper.h"
#include "Core/Win32/ComfyWindows.h"
#include <algorithm>

namespace Comfy::Utilities
{
	namespace
	{
		constexpr bool IsWhiteSpace(const char character)
		{
			return character == ' ' || character == '\t' || character == '\n' || character == '\r';
		}

		bool CaseInsenitiveComparison(const int a, const int b)
		{
			return tolower(a) == tolower(b);
		}
	}

	void TrimLeft(std::string& string)
	{
		string.erase(string.begin(), std::find_if(string.begin(), string.end(), [](const char character) { return !IsWhiteSpace(character); }));
	}

	void TrimRight(std::string& string)
	{
		string.erase(std::find_if(string.rbegin(), string.rend(), [](const char character) { return !IsWhiteSpace(character); }).base(), string.end());
	}

	void Trim(std::string& string)
	{
		TrimLeft(string);
		TrimRight(string);
	}

	bool MatchesInsensitive(std::string_view stringA, std::string_view stringB)
	{
		return std::equal(stringA.begin(), stringA.end(), stringB.begin(), stringB.end(), CaseInsenitiveComparison);
	}

	bool MatchesInsensitive(std::wstring_view stringA, std::wstring_view stringB)
	{
		return std::equal(stringA.begin(), stringA.end(), stringB.begin(), stringB.end(), CaseInsenitiveComparison);
	}

	bool Contains(std::string_view stringA, std::string_view stringB)
	{
		return stringA.find(stringB) != std::string::npos;
	}

	bool Contains(std::wstring_view stringA, std::wstring_view stringB)
	{
		return stringA.find(stringB) != std::string::npos;
	}

	bool StartsWith(std::string_view string, char suffix)
	{
		return !string.empty() && string.front() == suffix;
	}

	bool StartsWith(std::wstring_view string, wchar_t suffix)
	{
		return !string.empty() && string.front() == suffix;
	}

	bool StartsWith(std::string_view string, std::string_view prefix)
	{
		return string.find(prefix) == 0;
	}

	bool StartsWith(std::wstring_view string, std::wstring_view prefix)
	{
		return string.find(prefix) == 0;
	}

	bool StartsWithInsensitive(std::string_view string, std::string_view prefix)
	{
		return std::equal(prefix.begin(), prefix.end(), string.begin());
	}

	bool StartsWithInsensitive(std::wstring_view string, std::wstring_view prefix)
	{
		return std::equal(prefix.begin(), prefix.end(), string.begin());
	}

	bool EndsWith(std::string_view string, char suffix)
	{
		return !string.empty() && string.back() == suffix;
	}

	bool EndsWith(std::wstring_view string, wchar_t suffix)
	{
		return !string.empty() && string.back() == suffix;
	}

	bool EndsWith(std::string_view string, std::string_view suffix)
	{
		if (suffix.size() > string.size())
			return false;

		return std::equal(string.rbegin(), string.rbegin() + suffix.size(), suffix.rbegin(), suffix.rend());
	}

	bool EndsWith(std::wstring_view string, std::wstring_view suffix)
	{
		if (suffix.size() > string.size())
			return false;

		return std::equal(string.rbegin(), string.rbegin() + suffix.size(), suffix.rbegin(), suffix.rend());
	}

	bool EndsWithInsensitive(std::string_view string, std::string_view suffix)
	{
		if (suffix.size() > string.size())
			return false;

		return std::equal(string.rbegin(), string.rbegin() + suffix.size(), suffix.rbegin(), suffix.rend(), CaseInsenitiveComparison);
	}

	bool EndsWithInsensitive(std::wstring_view string, std::wstring_view suffix)
	{
		if (suffix.size() > string.size())
			return false;

		return std::equal(string.rbegin(), string.rbegin() + suffix.size(), suffix.rbegin(), suffix.rend(), CaseInsenitiveComparison);
	}

	std::wstring Utf8ToUtf16(std::string_view string)
	{
		std::wstring utf16String;

		const int utf16Length = ::MultiByteToWideChar(CP_UTF8, NULL, string.data(), static_cast<int>(string.size() + 1), nullptr, 0) - 1;
		if (utf16Length > 0)
		{
			utf16String.resize(utf16Length);
			::MultiByteToWideChar(CP_UTF8, NULL, string.data(), static_cast<int>(string.size()), utf16String.data(), utf16Length);
		}

		return utf16String;
	}

	std::string Utf16ToUtf8(std::wstring_view string)
	{
		std::string utf8String;

		const int utf8Length = ::WideCharToMultiByte(CP_UTF8, NULL, string.data(), static_cast<int>(string.size() + 1), nullptr, 0, nullptr, nullptr) - 1;
		if (utf8Length > 0)
		{
			utf8String.resize(utf8Length);
			::WideCharToMultiByte(CP_UTF8, NULL, string.data(), static_cast<int>(string.size()), utf8String.data(), utf8Length, nullptr, nullptr);
		}

		return utf8String;
	}
}
