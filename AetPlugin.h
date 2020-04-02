#include "AEUtil.h"
#include "entry.h"
#include "Types.h"
#include "CoreTypes.h"

namespace AetPlugin
{
	extern AEGP_PluginID GlobalPluginID;
	extern SPBasicSuite* GlobalBasicPicaSuite;
}

// NOTE: This entry point is exported through the PiPL (.r file)
extern "C" DllExport AEGP_PluginInitFuncPrototype EntryPointFunc;
