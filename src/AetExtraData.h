#pragma once
#include "AEConfig.h"
#include "AE_GeneralPlug.h"
#include "AE_Effect.h"
#include "AE_EffectUI.h"
#include "AE_EffectCB.h"
#include "AE_AdvEffectSuites.h"
#include "AE_EffectCBSuites.h"
#include "AEGP_SuiteHandler.h"
#include <Graphics/Auth2D/Aet/AetSet.h>
#include <unordered_map>

namespace AetPlugin
{
	using namespace Comfy::Graphics;

	struct AetExtraData
	{
	public:
		// NOTE: Explicitly disallow copying to avoid accidentally creating a copy of the result of AetExtraDataMapper::Get() instead of a reference
		AetExtraData() = default;
		AetExtraData(const AetExtraData& other) = delete;
		AetExtraData(AetExtraData&& other) = default;
		~AetExtraData() = default;

		AetExtraData& operator=(const AetExtraData& other) = delete;

	public:
		AEGP_CompH AE_Comp = nullptr;
		AEGP_ItemH AE_CompItem = nullptr;

		AEGP_FootageH AE_Footage = nullptr;
		AEGP_ItemH AE_FootageItem = nullptr;

		AEGP_LayerH AE_Layer = nullptr;
	};

	class AetExtraDataMapper : NonCopyable
	{
	public:
		AetExtraDataMapper() = default;
		~AetExtraDataMapper() = default;

	public:
		inline AetExtraData& Get(const Aet::Layer& object) { return map[&object]; }
		inline AetExtraData& Get(const Aet::Composition& object) { return map[&object]; }
		inline AetExtraData& Get(const Aet::Video& object) { return map[&object]; }
		inline AetExtraData& Get(const Aet::Audio& object) { return map[&object]; }

	private:
		std::unordered_map<const void*, AetExtraData> map;
	};
}
