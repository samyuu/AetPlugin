#pragma once
#include "AEUtil.h"
#include "entry.h"
#include "Types.h"
#include "CoreTypes.h"

#ifndef ConCat
#define ConCat(a, b) a ## b
#endif /* ConCat */

namespace AetPlugin
{
	extern AEGP_PluginID PluginID;
	extern SPBasicSuite* BasicPicaSuite;
	extern AEGP_Command ExportAetSetCommand;

	struct SuitesData
	{
		AEGP_SuiteHandler Handler = { BasicPicaSuite };

#define DeclareSuiteMember(suiteName) ConCat(AEGP_, suiteName)* suiteName = Handler.suiteName()

		DeclareSuiteMember(FIMSuite3); // AE 10.0
		DeclareSuiteMember(CommandSuite1); // AE 5.0
		DeclareSuiteMember(RegisterSuite5); // AE 10.0
		DeclareSuiteMember(ProjSuite5); // AE 10.0
		DeclareSuiteMember(ItemSuite1); // AE 5.0
		DeclareSuiteMember(FootageSuite5); // AE 10.0
		DeclareSuiteMember(CompSuite7); // AE 9.0
		DeclareSuiteMember(LayerSuite1); // AE 5.0
		DeclareSuiteMember(LayerSuite3); // AE 6.0
		DeclareSuiteMember(StreamSuite4); // AE 9
		DeclareSuiteMember(DynamicStreamSuite4); // AE 9.0
		DeclareSuiteMember(KeyframeSuite3); // AE 6.5
		DeclareSuiteMember(UtilitySuite3); // AE 10.0

#undef DeclareSuiteMember
	};
}

// NOTE: This entry point is exported through the PiPL (.r file)
extern "C" DllExport AEGP_PluginInitFuncPrototype EntryPointFunc;
