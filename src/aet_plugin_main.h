#pragma once
#include "core_types.h"
#include "core_string.h"
#include <AEConfig.h>
#include <AE_GeneralPlug.h>
#include <AEGP_SuiteHandler.h>
#include <entry.h>

namespace Comfy {}
namespace AetPlugin { using namespace Comfy; }

// NOTE: This entry point is exported through the PiPL (.r file)
extern "C" DllExport AEGP_PluginInitFuncPrototype EntryPointFunc;

namespace AetPlugin
{
	using LogLevelFlags = u32;
	enum LogLevelFlagsEnum : LogLevelFlags
	{
		LogLevelFlags_None = 0,
		LogLevelFlags_Info = (1 << 0),
		LogLevelFlags_Warning = (1 << 1),
		LogLevelFlags_Error = (1 << 2),
		LogLevelFlags_All = (LogLevelFlags_Info | LogLevelFlags_Warning | LogLevelFlags_Error),
	};

	constexpr cstr LogLevelNames[] = { "Info", "Warning", "Error", };

	// NOTE: Used as a "namespace" for all data stored using the PersistentDataSuite
	constexpr cstr PersistentDataPluginSectionKey = "AetPlugin";

	constexpr cstr AetPrefix = "aet_";
	constexpr cstr SprPrefix = "spr_";
	constexpr cstr SprTexPrefix = "sprtex_";

	struct PluginState
	{
		AEGP_PluginID PluginID = -1;
		void* MainWindowHandle = nullptr;

		SPBasicSuite* BasicPicaSuite = nullptr;
		AEGP_Command ExportAetSetCommand = -1;
	};

	inline PluginState Global = {};

	struct SuitesData
	{
		AEGP_SuiteHandler Handler = AEGP_SuiteHandler { Global.BasicPicaSuite };

#define CONCAT_SUITE(a, b) a##b
#define DECLARE_SUITE_MEMBER(suiteName) CONCAT_SUITE(AEGP_, suiteName)* suiteName = Handler.suiteName()

		DECLARE_SUITE_MEMBER(FIMSuite3); // AE 10.0
		DECLARE_SUITE_MEMBER(CommandSuite1); // AE 5.0
		DECLARE_SUITE_MEMBER(RegisterSuite5); // AE 10.0
		DECLARE_SUITE_MEMBER(ProjSuite5); // AE 10.0
		DECLARE_SUITE_MEMBER(ItemSuite1); // AE 5.0
		DECLARE_SUITE_MEMBER(ItemSuite8); // AE 9.0
		DECLARE_SUITE_MEMBER(FootageSuite5); // AE 10.0
		DECLARE_SUITE_MEMBER(CompSuite7); // AE 9.0
		DECLARE_SUITE_MEMBER(LayerSuite1); // AE 5.0
		DECLARE_SUITE_MEMBER(LayerSuite7); // AE 10.0
		DECLARE_SUITE_MEMBER(StreamSuite4); // AE 9
		DECLARE_SUITE_MEMBER(DynamicStreamSuite4); // AE 9.0
		DECLARE_SUITE_MEMBER(KeyframeSuite3); // AE 6.5
		DECLARE_SUITE_MEMBER(UtilitySuite3); // AE 10.0
		DECLARE_SUITE_MEMBER(MemorySuite1); // AE 5.0
		DECLARE_SUITE_MEMBER(PersistentDataSuite3); // AE 10.0

#undef DeclareSuiteMember
#undef CONCAT_SUITE
	};

	namespace ProjectStructure
	{
		/*
		- {set_name}
			- {scene_name_0}
				- data
					- audio
					- comp
					- video
					- (video_db)
				- {set_name}_{scene_name_0}
			- {scene_name_1}
				- data
					- audio
					- comp
					- video
					- (video_db)
				- {set_name}_{scene_name_1}
		*/

		namespace Names
		{
			constexpr cstr SetData = "data";
			constexpr cstr SceneRootPrefix = "_scene";
			constexpr cstr SceneData = "data";
			constexpr cstr SceneVideo = "video";
			constexpr cstr SceneVideoDB = "video_db";
			constexpr cstr SceneAudio = "audio";
			constexpr cstr SceneComp = "comp";

			constexpr cstr UnreferencedSpritesComp = "video_db_comp";
			constexpr cstr UnreferencedSpritesLayer = "unreferenced_sprites";
		};
	};
}
