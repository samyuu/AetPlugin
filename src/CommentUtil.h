#pragma once
#include "AEUtil.h"
#include "FormatUtil.h"
#include "Misc/StringHelper.h"
#include <optional>
#include <charconv>

namespace AetPlugin::CommentUtil
{
	static constexpr std::string_view AetID = "#Aet";

	namespace Keys
	{
		// NOTE: #Aet::Set: { %set_name%, %aet_set_id%, %spr_set_id% }
		static constexpr std::string_view AetSet = "Set";
		// NOTE: #Aet::Scene: { %scene_name%, %scene_id% }
		static constexpr std::string_view AetScene = "Scene";
		// NOTE: #Aet::Spr: { %spr_id%, ... }
		static constexpr std::string_view Spr = "Spr";
	}

	// NOTE: Enough space to store "#Aet::Spr: { 0x00000000, ... }"
	using Buffer = std::array<char, 8192>;

	struct Property
	{
		std::string_view Key = {};
		std::string_view Value = {};
		std::optional<size_t> KeyIndex = {};

		Property() = default;
		Property(std::string_view key, std::string_view value) : Key(key), Value(value) {}
		Property(std::string_view key, std::string_view value, std::optional<size_t> keyIndex) : Key(key), Value(value), KeyIndex(keyIndex) {}
	};

	namespace Detail
	{
		inline Buffer Format(const Property& property)
		{
			std::array<char, 24> indexBuffer = { "" };
			if (property.KeyIndex.has_value())
				sprintf(indexBuffer.data(), "[%zu]", property.KeyIndex.value());

			Buffer commentBuffer;
			sprintf(commentBuffer.data(), "%.*s::%.*s%s: { %.*s }",
				static_cast<int>(AetID.size()),
				AetID.data(),
				static_cast<int>(property.Key.size()),
				property.Key.data(),
				indexBuffer.data(),
				static_cast<int>(property.Value.size()),
				property.Value.data());
			return commentBuffer;
		}

		inline Property Parse(const std::string_view buffer)
		{
			Property result = {};

			const auto trimmedBuffer = FormatUtil::Trim(buffer);
			if (!Comfy::Utilities::StartsWith(trimmedBuffer, AetID) || (trimmedBuffer.size() < (AetID.size() + std::strlen("::x: {}"))))
				return result;

			const std::string_view postIDBuffer = trimmedBuffer.substr(AetID.size() + std::strlen("::"));
			const std::string_view keyBuffer = postIDBuffer.substr(0, std::min(postIDBuffer.find(':'), postIDBuffer.find('[')));

			if (keyBuffer.empty())
				return result;

			std::string_view postKeyBuffer = postIDBuffer.substr(keyBuffer.size());
			if (postKeyBuffer.front() == '[')
			{
				auto indexEndPos = postKeyBuffer.find(']');
				if (indexEndPos == std::string_view::npos)
					return result;

				const auto indexBuffer = postKeyBuffer.substr(1, indexEndPos - 1);
				if (indexBuffer.empty())
					return result;

				unsigned long long parsedIndex = {};
				if (const auto parseResult = std::from_chars(indexBuffer.data(), indexBuffer.data() + indexBuffer.size(), parsedIndex); parseResult.ec == std::errc::invalid_argument)
					return result;

				result.KeyIndex = static_cast<size_t>(parsedIndex);
				postKeyBuffer = postKeyBuffer.substr(indexBuffer.size() + std::strlen("[]"));
			}

			constexpr std::string_view bracketsStart = ": {", bracketsEnd = "}";
			if (!Comfy::Utilities::StartsWith(postKeyBuffer, bracketsStart) || !Comfy::Utilities::EndsWith(postKeyBuffer, bracketsEnd))
				return result;

			const std::string_view valueBuffer = FormatUtil::Trim(postKeyBuffer.substr(bracketsStart.size(), postKeyBuffer.size() - bracketsStart.size() - bracketsEnd.size()));

			result.Key = keyBuffer;
			result.Value = valueBuffer;

			return result;
		}

		inline std::string_view AdvanceCommaSeparateList(std::string_view& inOutList)
		{
			if (inOutList.empty())
				return inOutList.substr(0, 0);

			if (const auto nextComma = inOutList.find(','); nextComma == std::string_view::npos)
			{
				const auto subString = inOutList.substr(0);
				inOutList = inOutList.substr(inOutList.size() - 1, 0);
				return subString;
			}
			else
			{
				const auto subString = inOutList.substr(0, nextComma);
				inOutList = inOutList.substr(subString.length() + 1);
				return subString;
			}
		}
	}

	inline void Set(AEGP_ItemSuite8* itemSuite8, AEGP_ItemH item, const Property& property)
	{
		if (item == nullptr)
			return;

		const auto buffer = Detail::Format(property);
		itemSuite8->AEGP_SetItemComment(item, buffer.data());
	}

	inline Property Get(AEGP_ItemSuite8* itemSuite8, AEGP_ItemH item, Buffer& outBuffer)
	{
		if (item == nullptr)
			return {};

		itemSuite8->AEGP_GetItemComment(item, static_cast<A_u_long>(outBuffer.size()), outBuffer.data());
		return Detail::Parse(outBuffer.data());
	}

	inline uint32_t ParseID(std::string_view commentValue)
	{
		const auto preHexTrimSize = commentValue.size();
		commentValue = FormatUtil::StripPrefixIfExists(commentValue, FormatUtil::HexPrefix);

		uint32_t resultID = {};
		const auto result = std::from_chars(commentValue.data(), commentValue.data() + commentValue.size(), resultID, (commentValue.size() == preHexTrimSize) ? 10 : 16);;

		return (result.ec != std::errc::invalid_argument) ? resultID : 0xFFFFFFFF;
	}

	template <typename Func>
	inline void IterateCommaSeparateList(const std::string_view commaSeparateList, Func func)
	{
		std::string_view readHead = commaSeparateList;
		const char* endOfList = commaSeparateList.data() + commaSeparateList.size() - 1;

		while (true)
		{
			const auto currentItem = Detail::AdvanceCommaSeparateList(readHead);
			const auto itemTrimmed = FormatUtil::Trim(currentItem);

			func(itemTrimmed);

			if (readHead.data() >= endOfList)
				return;
		}
	}

	template <size_t Size>
	inline std::array<std::string_view, Size> ParseArray(std::string_view commaSeparateList)
	{
		std::array<std::string_view, Size> result = {};

		size_t index = 0;
		IterateCommaSeparateList(commaSeparateList, [&](const auto& item)
		{
			if (index < Size)
				result[index] = item;
			index++;
		});

		return result;
	}
}
