#include "FileExtensionHelper.h"
#include "StringHelper.h"
#include <assert.h>

namespace Comfy::Utilities
{
	bool FileExtensionHelper::DoesAnyExtensionMatch(const std::string_view inputExtension, const std::string_view packedExtensions, const char packedSeparator)
	{
		// NOTE: Invalid packed extension
		assert(!packedExtensions.empty() && packedExtensions.back() != packedSeparator);

		if (inputExtension.size() > packedExtensions.size())
			return false;
		
		return IterateComparePackedExtensions(packedExtensions, packedSeparator, inputExtension);
	}

	std::string_view FileExtensionHelper::GetExtensionSubstring(const std::string_view filePath)
	{
		if (filePath.empty())
			return {};

		const size_t extensionIndex = filePath.find_last_of(ExtensionSeparator);
		if (extensionIndex == std::string::npos)
			return {};

		return filePath.substr(extensionIndex, filePath.size() - extensionIndex);
	}

	bool FileExtensionHelper::IterateComparePackedExtensions(const std::string_view packedExtensions, const char packedSeparator, const std::string_view inputExtension)
	{
		size_t lastSeparatorIndex = 0;
		for (size_t i = 0; i < packedExtensions.size(); i++)
		{
			const bool isLastCharacter = (i + 1 == packedExtensions.size());

			if (packedExtensions[i] == packedSeparator || isLastCharacter)
			{
				const size_t separatorIndex = i;
				const size_t extensionSize = isLastCharacter ? 
					(separatorIndex - packedExtensions.size()) : 
					(separatorIndex - lastSeparatorIndex);

				const bool isFirstExtension = (lastSeparatorIndex == 0);
				const auto subString = isFirstExtension ?
					packedExtensions.substr(lastSeparatorIndex, extensionSize) :
					packedExtensions.substr(lastSeparatorIndex + 1, extensionSize - 1);

				lastSeparatorIndex = separatorIndex;

				if (MatchesInsensitive(inputExtension, subString))
					return true;
			}
		}

		return false;
	}
}
