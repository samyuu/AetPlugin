#pragma once
#include "Types.h"
#include "CoreTypes.h"

namespace Comfy::FileSystem::Crypto
{
	static constexpr size_t IVSize = 16;
	static constexpr size_t KeySize = 16;

	bool Win32DecryptAesEcb(const uint8_t* encryptedData, uint8_t* decryptedData, size_t dataSize, std::array<uint8_t, KeySize> key);
	bool Win32DecryptAesCbc(const uint8_t* encryptedData, uint8_t* decryptedData, size_t dataSize, std::array<uint8_t, KeySize> key, std::array<uint8_t, IVSize> iv);
}
