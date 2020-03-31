#include "AEConfig.h"

#include "entry.h"
#include "AE_GeneralPlug.h"
#include "AE_Effect.h"
#include "AE_EffectUI.h"
#include "AE_EffectCB.h"
#include "AE_AdvEffectSuites.h"
#include "AE_EffectCBSuites.h"
#include "AEGP_SuiteHandler.h"
#include "AE_Macros.h"

#include "Types.h"
#include "CoreTypes.h"

namespace AetPlugin
{
	extern AEGP_PluginID GlobalPluginID;
	extern SPBasicSuite* GlobalBasicPicaSuite;
}

// NOTE: This entry point is exported through the PiPL (.r file)
extern "C" DllExport AEGP_PluginInitFuncPrototype EntryPointFunc;
