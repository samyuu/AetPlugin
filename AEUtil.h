#pragma once
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

	inline AEGP_ColorVal ColorRGB8(uint32_t inputColor)
	{
		struct RGB8 { uint8_t R, G, B; };
		const RGB8 colorRGB8 = *reinterpret_cast<const RGB8*>(&inputColor);

		constexpr float rgb8ToFloat = static_cast<float>(std::numeric_limits<uint8_t>::max());
		constexpr float aeAlpha = 1.0f;

		return AEGP_ColorVal { aeAlpha, colorRGB8.R / rgb8ToFloat, colorRGB8.G / rgb8ToFloat, colorRGB8.B / rgb8ToFloat };
	}
}
