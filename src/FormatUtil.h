#pragma once
#include <Types.h>
#include <CoreTypes.h>

namespace AetPlugin::FormatUtil
{
	template <typename FlagsType, typename FlagsArray>
	inline std::string FormatFlags(const FlagsType flags, const FlagsArray& flagsToCheck)
	{
		static constexpr std::string_view separator = " | ";

		std::string result;
		{
			for (const auto&[mask, name] : flagsToCheck)
				if (flags & mask) { result += name; result += separator; }

			if (!result.empty())
				result[result.size() - separator.size()] = '\0';
			else
				result += "None";
		}
		return result;
	}
}
