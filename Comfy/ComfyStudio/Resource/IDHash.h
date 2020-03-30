#pragma once
#include "IDTypes.h"
#include "CoreTypes.h"

namespace Comfy
{
	constexpr uint32_t MurmurHashSeed = 0xDEADBEEF;
	constexpr uint32_t MurmurHashM = 0x7FD652AD;
	constexpr uint32_t MurmurHashR = 0x10;

	constexpr uint32_t MurmurHash(std::string_view string)
	{
		size_t dataSize = string.size();
		const char* data = string.data();

		uint32_t hash = MurmurHashSeed;
		while (dataSize >= sizeof(uint32_t))
		{
			hash += static_cast<uint32_t>(
				(static_cast<uint8_t>(data[0]) << 0) |
				(static_cast<uint8_t>(data[1]) << 8) |
				(static_cast<uint8_t>(data[2]) << 16) |
				(static_cast<uint8_t>(data[3]) << 24));

			hash *= MurmurHashM;
			hash ^= hash >> MurmurHashR;

			data += sizeof(uint32_t);
			dataSize -= sizeof(uint32_t);
		}

		switch (dataSize)
		{
		case 3:
			hash += static_cast<uint32_t>(data[2] << 16);
		case 2:
			hash += static_cast<uint32_t>(data[1] << 8);
		case 1:
			hash += static_cast<uint32_t>(data[0] << 0);
			hash *= MurmurHashM;
			hash ^= hash >> MurmurHashR;
			break;
		}

		hash *= MurmurHashM;
		hash ^= hash >> 10;
		hash *= MurmurHashM;
		hash ^= hash >> 17;

		return hash;
	}

	template <typename IDType>
	constexpr IDType HashIDString(std::string_view string)
	{
		return static_cast<IDType>(MurmurHash(string));
	}
}
