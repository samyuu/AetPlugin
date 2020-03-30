#pragma once
#include "Types.h"

namespace Comfy::FileSystem
{
	enum class PtrMode : uint8_t
	{
		Mode32Bit,
		Mode64Bit,
	};

	enum class Endianness : uint16_t
	{
		Little = 'LE',
		Big = 'BE',

		// TODO: C++20 std::endian please come to rescue :PeepoHug:
		Native = (true) ? Little : Big,
	};
}
