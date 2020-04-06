#pragma once
#include "Types.h"
#include "CoreTypes.h"
#include <charconv>

namespace Comfy::Utilities::StringParsing
{
	std::string_view GetLine(const char* textBuffer);
	std::string_view GetWord(const char* textBuffer);
	std::string_view GetProperty(const char* textBuffer);
	std::string_view GetValue(const char* textBuffer);

	void AdvanceToNextLine(const char*& textBuffer);

	void AdvanceToStartOfLine(const char*& textBuffer, const char* bufferStart = nullptr);
	void AdvanceToStartOfPreviousLine(const char*& textBuffer, const char* bufferStart = nullptr);

	void AdvanceToNextProperty(const char*& textBuffer);

	std::string_view GetLineAdvanceToNextLine(const char*& textBuffer);
	std::string_view AdvanceToStartOfPreviousLineGetLine(const char*& textBuffer, const char* bufferStart = nullptr);

	std::string_view GetPropertyAdvanceToNextProperty(const char*& textBuffer);

	bool IsComment(std::string_view line, char identifier = '#');

	std::string_view GetLineAdvanceToNonCommentLine(const char*& textBuffer);
	std::string_view AdvanceToStartOfPreviousLineGetNonCommentLine(const char*& textBuffer, const char* bufferStart = nullptr);

	bool ParseBool(std::string_view string);

	template <typename T>
	T ParseType(std::string_view string)
	{
		if constexpr (std::is_same<T, bool>::value)
		{
			return ParseBool(string);
		}
		else
		{
			T value = {};
			auto result = std::from_chars(string.data(), string.data() + string.size(), value);
			return value;
		}
	}

	template <typename T, size_t Size>
	std::array<T, Size> ParseTypeArray(std::string_view string)
	{
		std::array<T, Size> value;

		for (size_t i = 0; i < Size; i++)
		{
			auto word = GetWord(string.data());
			value[i] = ParseType<T>(word);

			if (i + 1 < Size)
				string = string.substr(word.size() + 1);
		}

		return value;
	}
}
