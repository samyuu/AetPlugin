#include "AEConfig.h"

#include "entry.h"
#include "AE_GeneralPlug.h"
#include "AE_Effect.h"
#include "AE_EffectUI.h"
#include "AE_EffectCB.h"
#include "AE_AdvEffectSuites.h"
#include "AE_EffectCBSuites.h"
#include "AEGP_SuiteHandler.h"

// NOTE: Extra data used by Editor Components to avoid additional allocations and reduce complexity
struct AetImportGuiExtraData
{
	AEGP_CompH AE_Comp = nullptr;
	AEGP_ItemH AE_CompItem = nullptr;

	AEGP_FootageH AE_Footage = nullptr;
	AEGP_ItemH AE_FootageItem = nullptr;

	AEGP_LayerH AE_Layer = nullptr;
};

using GuiExtraData = AetImportGuiExtraData;
