#pragma once
#include "Types.h"
#include <intrin.h>

namespace Comfy::Utilities
{
	inline int16_t ByteSwapI16(int16_t value) { return _byteswap_ushort(value); }
	inline uint16_t ByteSwapU16(uint16_t value) { return _byteswap_ushort(value); }

	inline int32_t ByteSwapI32(int32_t value) { return _byteswap_ulong(value); }
	inline uint32_t ByteSwapU32(uint32_t value) { return _byteswap_ulong(value); }

	inline int64_t ByteSwapI64(int64_t value) { return _byteswap_uint64(value); }
	inline uint64_t ByteSwapU64(uint64_t value) { return _byteswap_uint64(value); }

	inline float ByteSwapF32(float value) { uint32_t result = ByteSwapU32(*reinterpret_cast<uint32_t*>(&value)); return *reinterpret_cast<float*>(&result); }
	inline double ByteSwapF64(double value) { uint64_t result = ByteSwapU64(*reinterpret_cast<uint64_t*>(&value)); return *reinterpret_cast<double*>(&result); }
}
