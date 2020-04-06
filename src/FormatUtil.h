#pragma once
#include "Types.h"
#include "Misc/StringHelper.h"
#include "FileSystem/FileHelper.h"
#include "Graphics/Auth2D/Aet/AetSet.h"

namespace FormatUtil
{
	inline std::string ToLower(std::string_view value)
	{
		auto lowerCaseString = std::string(value);
		for (char& character : lowerCaseString)
			character = static_cast<char>(tolower(character));
		return lowerCaseString;
	}

	inline std::string ToUpper(std::string_view value)
	{
		auto lowerCaseString = std::string(value);
		for (char& character : lowerCaseString)
			character = static_cast<char>(toupper(character));
		return lowerCaseString;
	}

	inline std::string_view StripPrefixIfExists(std::string_view stringInput, std::string_view prefix)
	{
		if (Comfy::Utilities::StartsWithInsensitive(stringInput, prefix))
			return stringInput.substr(prefix.size(), stringInput.size() - prefix.size());

		return stringInput;
	}

	inline std::string_view StripSuffixIfExists(std::string_view stringInput, std::string_view suffix)
	{
		if (Comfy::Utilities::EndsWithInsensitive(stringInput, suffix))
			return stringInput.substr(0, stringInput.size() - suffix.size());

		return stringInput;
	}

	inline std::string_view StripFileExtension(std::string_view stringInput)
	{
		const auto extension = Comfy::FileSystem::GetFileExtension(stringInput);
		return StripSuffixIfExists(stringInput, extension);
	}
}
