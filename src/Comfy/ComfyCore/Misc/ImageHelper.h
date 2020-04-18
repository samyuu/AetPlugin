#pragma once
#include "Types.h"
#include "CoreTypes.h"

namespace Comfy::Utilities
{
	void ReadImage(std::string_view filePath, ivec2& outSize, UniquePtr<uint8_t[]>& outRGBAPixels);
	void WritePNG(std::string_view filePath, ivec2 size, const void* rgbaPixels);
}
