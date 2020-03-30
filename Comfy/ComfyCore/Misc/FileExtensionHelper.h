#pragma once
#include "CoreTypes.h"

namespace Comfy::Utilities
{
	class FileExtensionHelper
	{
	public:
		static constexpr char ExtensionSeparator = '.';
		static constexpr char DefaultSeparator = ';';

		static bool DoesAnyExtensionMatch(const std::string_view inputExtension, const std::string_view packedExtensions, const char packedSeparator = DefaultSeparator);
		static std::string_view GetExtensionSubstring(const std::string_view filePath);

	private:
		static bool IterateComparePackedExtensions(const std::string_view packedExtensions, const char packedSeparator, const std::string_view inputExtension);
	};
}
