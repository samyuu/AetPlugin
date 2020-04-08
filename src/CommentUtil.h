#pragma once
#include "AEUtil.h"
#include "Misc/StringHelper.h"
#include <optional>
#include <charconv>

namespace AetPlugin::CommentUtil
{
	static constexpr std::string_view AetSetID = "#AetSet";

	namespace Keys
	{
		static constexpr std::string_view AetSet = "Set";
		static constexpr std::string_view Scene = "Scene";
		static constexpr std::string_view SprID = "SprID";
	}

	using Buffer = std::array<char, AEGP_MAX_RQITEM_COMMENT_SIZE>;

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
				static_cast<int>(AetSetID.size()),
				AetSetID.data(),
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

			if (!Comfy::Utilities::StartsWith(buffer, AetSetID) || (buffer.size() < (AetSetID.size() + std::strlen("::x: { }"))))
				return result;

			const std::string_view postIDBuffer = buffer.substr(AetSetID.size() + std::strlen("::"));
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

			constexpr std::string_view bracketsStart = ": { ", bracketsEnd = " }";
			if (!Comfy::Utilities::StartsWith(postKeyBuffer, bracketsStart) || !Comfy::Utilities::EndsWith(postKeyBuffer, bracketsEnd))
				return result;

			const std::string_view valueBuffer = postKeyBuffer.substr(bracketsStart.size(), postKeyBuffer.size() - bracketsStart.size() - bracketsEnd.size());

			result.Key = keyBuffer;
			result.Value = valueBuffer;

			return result;
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
}
