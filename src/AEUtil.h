#pragma once
#include "Types.h"
#include "CoreTypes.h"
#include "AEConfig.h"
#include "AE_GeneralPlug.h"
#include "AE_Effect.h"
#include "AE_EffectUI.h"
#include "AE_EffectCB.h"
#include "AE_AdvEffectSuites.h"
#include "AE_EffectCBSuites.h"
#include "AEGP_SuiteHandler.h"
#include "AE_Macros.h"
#include <algorithm>
#include <numeric>

namespace AEUtil
{
	struct RGB8 { uint8_t R, G, B, A; };
	constexpr float RGB8ToFloat = static_cast<float>(std::numeric_limits<uint8_t>::max());

	static constexpr A_Ratio OneToOneRatio = { 1, 1 };
	static constexpr float FixedPoint = 10000.0f;

	inline const A_UTF16Char* UTF16Cast(const wchar_t* value)
	{
		return reinterpret_cast<const A_UTF16Char*>(value);
	}

	inline const wchar_t* WCast(const A_UTF16Char* value)
	{
		return reinterpret_cast<const wchar_t*>(value);
	}

	inline std::wstring MoveFreeUTF16String(AEGP_MemorySuite1* memorySuite1, AEGP_MemHandle memHandle)
	{
		void* underlyingData;
		memorySuite1->AEGP_LockMemHandle(memHandle, &underlyingData);

		const auto underlyingString = reinterpret_cast<const A_UTF16Char*>(underlyingData);
		const auto result = std::wstring(WCast(underlyingString));

		memorySuite1->AEGP_UnlockMemHandle(memHandle);
		memorySuite1->AEGP_FreeMemHandle(memHandle);

		return result;
	}

	inline AEGP_ColorVal ColorRGB8(uint32_t inputColor)
	{
		const RGB8 colorRGB8 = *reinterpret_cast<const RGB8*>(&inputColor);
		return AEGP_ColorVal
		{
			static_cast<float>(1.0f),
			static_cast<float>(colorRGB8.R / RGB8ToFloat),
			static_cast<float>(colorRGB8.G / RGB8ToFloat),
			static_cast<float>(colorRGB8.B / RGB8ToFloat),
		};
	}

	inline uint32_t ColorRGB8(AEGP_ColorVal inputColor)
	{
		const RGB8 colorRGB8 =
		{
			static_cast<uint8_t>(inputColor.redF / RGB8ToFloat),
			static_cast<uint8_t>(inputColor.greenF / RGB8ToFloat),
			static_cast<uint8_t>(inputColor.blueF / RGB8ToFloat),
			static_cast<uint8_t>(0x00),
		};
		return *reinterpret_cast<const uint32_t*>(&colorRGB8);
	}
}
