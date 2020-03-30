#pragma once
#include "CoreTypes.h"

namespace Comfy
{
	namespace Utilities
	{
		void TrimLeft(std::string& string);
		void TrimRight(std::string& string);
		void Trim(std::string& string);

		bool MatchesInsensitive(std::string_view stringA, std::string_view stringB);
		bool MatchesInsensitive(std::wstring_view stringA, std::wstring_view stringB);

		bool Contains(std::string_view stringA, std::string_view stringB);
		bool Contains(std::wstring_view stringA, std::wstring_view stringB);

		bool StartsWith(std::string_view string, char suffix);
		bool StartsWith(std::wstring_view string, wchar_t suffix);

		bool StartsWith(std::string_view string, std::string_view prefix);
		bool StartsWith(std::wstring_view string, std::wstring_view prefix);

		bool StartsWithInsensitive(std::string_view string, std::string_view prefix);
		bool StartsWithInsensitive(std::wstring_view string, std::wstring_view prefix);

		bool EndsWith(std::string_view string, char suffix);
		bool EndsWith(std::wstring_view string, wchar_t suffix);

		bool EndsWith(std::string_view string, std::string_view suffix);
		bool EndsWith(std::wstring_view string, std::wstring_view suffix);

		bool EndsWithInsensitive(std::string_view string, std::string_view suffix);
		bool EndsWithInsensitive(std::wstring_view string, std::wstring_view suffix);

		std::wstring Utf8ToUtf16(std::string_view string);
		std::string Utf16ToUtf8(std::wstring_view string);
	}

	using namespace Utilities;
}
