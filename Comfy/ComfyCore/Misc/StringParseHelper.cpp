#include "StringParseHelper.h"
#include "StringHelper.h"

namespace Comfy::Utilities::StringParsing
{
	std::string_view GetLine(const char* textBuffer)
	{
		const char* startOfLine = textBuffer;
		const char* endOfLine = textBuffer;

		while (*endOfLine != '\0' && *endOfLine != '\r' && *endOfLine != '\n')
			endOfLine++;

		const size_t length = endOfLine - startOfLine;
		return std::string_view(startOfLine, length);
	}

	std::string_view GetWord(const char* textBuffer)
	{
		const char* startOfWord = textBuffer;
		const char* endOfWord = textBuffer;

		while (*endOfWord != '\0' && *endOfWord != ' ' && *endOfWord != '\r' && *endOfWord != '\n')
			endOfWord++;

		const size_t length = endOfWord - startOfWord;
		return std::string_view(startOfWord, length);
	}

	std::string_view GetProperty(const char* textBuffer)
	{
		const char* startOfProperty = textBuffer;
		const char* endOfProperty = textBuffer;

		while (*endOfProperty != '\0' && *endOfProperty != '.' && *endOfProperty != '=')
			endOfProperty++;

		const size_t lineLength = endOfProperty - startOfProperty;
		return std::string_view(startOfProperty, lineLength);
	}

	std::string_view GetValue(const char* textBuffer)
	{
		const char* startOfValue = textBuffer;
		while (*startOfValue != '\0' && *startOfValue != '=')
			startOfValue++;

		if (*startOfValue == '=')
			startOfValue++;

		const char* endOfValue = startOfValue;
		while (*endOfValue != '\0' && *endOfValue != '\r' && *endOfValue != '\n')
			endOfValue++;

		const size_t length = endOfValue - startOfValue;
		return std::string_view(startOfValue, length);
	}

	void AdvanceToNextLine(const char*& textBuffer)
	{
		while (*textBuffer != '\0')
		{
			if (*textBuffer == '\r')
			{
				textBuffer++;

				if (*textBuffer == '\n')
					textBuffer++;

				return;
			}

			if (*textBuffer == '\n')
			{
				textBuffer++;
				return;
			}

			textBuffer++;
		}
	}

	void AdvanceToStartOfLine(const char*& textBuffer, const char* bufferStart)
	{
		// NOTE: Check for buffer start because we can't rely on null termination
		while (textBuffer > bufferStart)
		{
			if (*textBuffer == '\n')
			{
				*textBuffer++;
				return;
			}

			textBuffer--;
		}
	}

	void AdvanceToStartOfPreviousLine(const char*& textBuffer, const char* bufferStart)
	{
		if (textBuffer > bufferStart)
			textBuffer--;
		if (textBuffer > bufferStart && *textBuffer == '\n')
			textBuffer--;
		if (textBuffer > bufferStart && *textBuffer == '\r')
			textBuffer--;

		AdvanceToStartOfLine(textBuffer, bufferStart);
	}

	void AdvanceToNextProperty(const char*& textBuffer)
	{
		while (*textBuffer != '\0' && *textBuffer != '=')
		{
			if (*textBuffer == '.')
			{
				textBuffer++;
				return;
			}

			textBuffer++;
		}
	}

	std::string_view GetLineAdvanceToNextLine(const char*& textBuffer)
	{
		const auto line = GetLine(textBuffer);
		AdvanceToNextLine(textBuffer);
		return line;
	}

	std::string_view AdvanceToStartOfPreviousLineGetLine(const char*& textBuffer, const char* bufferStart)
	{
		AdvanceToStartOfPreviousLine(textBuffer, bufferStart);
		const auto line = GetLine(textBuffer);
		return line;
	}

	std::string_view GetPropertyAdvanceToNextProperty(const char*& textBuffer)
	{
		const auto property = GetProperty(textBuffer);
		AdvanceToNextProperty(textBuffer);
		return property;
	}


	bool IsComment(std::string_view line, char identifier)
	{
		return !line.empty() && line.front() == identifier;
	}

	std::string_view GetLineAdvanceToNonCommentLine(const char*& textBuffer)
	{
		std::string_view line;

		do
			line = StringParsing::GetLineAdvanceToNextLine(textBuffer);
		while (IsComment(line));

		return line;
	}

	std::string_view AdvanceToStartOfPreviousLineGetNonCommentLine(const char*& textBuffer, const char* bufferStart)
	{
		std::string_view line;

		do
			line = StringParsing::AdvanceToStartOfPreviousLineGetLine(textBuffer, bufferStart);
		while (IsComment(line) && textBuffer > bufferStart);

		return line;
	}

	bool ParseBool(std::string_view string)
	{
		if (MatchesInsensitive(string, "true"))
			return true;
		else if (MatchesInsensitive(string, "false"))
			return false;
		return false;
	}
}
