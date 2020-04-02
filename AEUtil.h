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
}
