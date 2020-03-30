#pragma once
#include "Misc/StringParseHelper.h"

namespace Comfy::Utilities::StringParsing
{
	constexpr size_t MaximumNestedProperties = 16;

	class TextDatabaseParser
	{
	private:
		struct ParserState
		{
			std::array<std::string_view, MaximumNestedProperties> PropertiesBuffer;

			int CurrentPropertyIndex;
			std::string_view CurrentProperty;
			std::string_view CurrentValueString;
		} state;

	protected:
		inline void StateParseNewLinePropertiesAndValue(std::string_view line)
		{
			const char* currentLinePosition = line.data();

			state.PropertiesBuffer = {};
			for (auto& property : state.PropertiesBuffer)
			{
				property = StringParsing::GetPropertyAdvanceToNextProperty(currentLinePosition);
				if (property.empty())
					break;
			}

			state.CurrentValueString = StringParsing::GetValue(currentLinePosition);

			state.CurrentPropertyIndex = 0;
			AdvanceProperty();
		}

	protected:
		inline std::string_view ParseValueString()
		{
			return state.CurrentValueString;
		}

		template <typename T>
		inline T ParseValueString()
		{
			return StringParsing::ParseType<T>(ParseValueString());
		}

		template <typename T>
		inline T ParseEnumValueString()
		{
			return static_cast<T>(StringParsing::ParseType<uint32_t>(ParseValueString()));
		}

		inline void AdvanceProperty()
		{
			state.CurrentProperty = state.PropertiesBuffer[state.CurrentPropertyIndex++];
		}

		inline bool IsLastProperty()
		{
			return state.PropertiesBuffer[state.CurrentPropertyIndex - 1].empty();
		}

		inline bool CompareProperty(std::string_view property)
		{
			if (property == state.CurrentProperty)
			{
				AdvanceProperty();
				return true;
			}

			return false;
		}

		template <typename T>
		inline bool TryParseLength(std::vector<T>& vector, std::string_view lengthIdentifier = "length")
		{
			if (CompareProperty(lengthIdentifier))
			{
				vector.resize(ParseValueString<uint32_t>());
				return true;
			}

			return false;
		}

		inline uint32_t ParseAdvanceIndexProperty()
		{
			auto index = StringParsing::ParseType<uint32_t>(state.CurrentProperty);
			AdvanceProperty();
			return index;
		}

		template <typename T>
		inline void ParseCommaSeparatedArray(std::string_view commaSeparatedData, T* outputValues, size_t valueCount)
		{
			size_t dataIndex = 0, lastCommaIndex = 0;
			for (int64_t i = 0; i < static_cast<int64_t>(commaSeparatedData.size()); i++)
			{
				if (commaSeparatedData[i] == ',')
				{
					std::string_view dataString = commaSeparatedData.substr(lastCommaIndex, i - lastCommaIndex);
					lastCommaIndex = i + 1;

					outputValues[dataIndex++] = StringParsing::ParseType<T>(dataString);
				}
			}

			outputValues[dataIndex] = StringParsing::ParseType<T>(commaSeparatedData.substr(lastCommaIndex));
		}

		template <typename T>
		inline void ParseCommaSeparatedArray(T* outputValues, size_t valueCount)
		{
			return ParseCommaSeparatedArray<T>(state.CurrentValueString, outputValues, valueCount);
		}

		template <typename T>
		inline T ParseAdvanceCommaSeparatedValueString()
		{
			auto valueSubString = state.CurrentValueString;
			for (int64_t i = 0; i < static_cast<int64_t>(valueSubString.size()); i++)
			{
				if ((i + 1) == valueSubString.size())
					break;

				if (valueSubString[i] == ',')
				{
					valueSubString = valueSubString.substr(0, i);
					break;
				}
			}

			state.CurrentValueString = state.CurrentValueString.substr(valueSubString.size());

			if (!state.CurrentValueString.empty() && state.CurrentValueString.front() == ',')
				state.CurrentValueString = state.CurrentValueString.substr(1);

			auto value = StringParsing::ParseType<T>(valueSubString);
			return value;
		}

		template <typename T, size_t TSize, bool TParentheses = true>
		inline std::array<T, TSize> ParseCommaSeparatedArray()
		{
			// NOTE: Optionally remove surrounding parentheses
			std::string_view commaSeparatedData = TParentheses ? (state.CurrentValueString.substr(1, state.CurrentValueString.size() - 2)) : state.CurrentValueString;

			std::array<T, TSize> rawData = {};
			ParseCommaSeparatedArray<T>(commaSeparatedData, rawData.data(), TSize);
			return rawData;
		}
	};
}
