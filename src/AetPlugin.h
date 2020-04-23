#pragma once
#include "AEUtil.h"
#include "CommentUtil.h"
#include "entry.h"
#include "Types.h"
#include "CoreTypes.h"

#ifndef ConCat
#define ConCat(a, b) a ## b
#endif /* ConCat */

namespace AetPlugin
{
	using LogLevel = uint32_t;
	enum LogLevelEnum : LogLevel
	{
		LogLevel_None = 0,
		LogLevel_Info = (1 << 0),
		LogLevel_Warning = (1 << 1),
		LogLevel_Error = (1 << 2),
		LogLevel_All = (LogLevel_Info | LogLevel_Warning | LogLevel_Error),
	};

	constexpr std::array LogLevelsToCheck =
	{
		std::make_pair(LogLevel_Info, "Info"),
		std::make_pair(LogLevel_Warning, "Warning"),
		std::make_pair(LogLevel_Error, "Error"),
	};

	constexpr std::string_view AetPrefix = "aet_";
	constexpr std::wstring_view AetPrefixW = L"aet_";

	constexpr std::string_view SprPrefix = "spr_";
	constexpr std::wstring_view SprPrefixW = L"spr_";

	constexpr std::string_view SprTexPrefix = "sprtex_";
	constexpr std::wstring_view SprTexPrefixW = L"sprtex_";

	struct PluginStateData
	{
		AEGP_PluginID PluginID = -1;
		void* MainWindowHandle = nullptr;

		SPBasicSuite* BasicPicaSuite = nullptr;
		AEGP_Command ExportAetSetCommand = -1;
	};

	extern PluginStateData EvilGlobalState;

	struct SuitesData
	{
		AEGP_SuiteHandler Handler = { EvilGlobalState.BasicPicaSuite };

#define DeclareSuiteMember(suiteName) ConCat(AEGP_, suiteName)* suiteName = Handler.suiteName()

		DeclareSuiteMember(FIMSuite3); // AE 10.0
		DeclareSuiteMember(CommandSuite1); // AE 5.0
		DeclareSuiteMember(RegisterSuite5); // AE 10.0
		DeclareSuiteMember(ProjSuite5); // AE 10.0
		DeclareSuiteMember(ItemSuite1); // AE 5.0
		DeclareSuiteMember(ItemSuite8); // AE 9.0
		DeclareSuiteMember(FootageSuite5); // AE 10.0
		DeclareSuiteMember(CompSuite7); // AE 9.0
		DeclareSuiteMember(LayerSuite1); // AE 5.0
		DeclareSuiteMember(LayerSuite7); // AE 10.0
		DeclareSuiteMember(StreamSuite4); // AE 9
		DeclareSuiteMember(DynamicStreamSuite4); // AE 9.0
		DeclareSuiteMember(KeyframeSuite3); // AE 6.5
		DeclareSuiteMember(UtilitySuite3); // AE 10.0
		DeclareSuiteMember(MemorySuite1); // AE 5.0

#undef DeclareSuiteMember
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
				- {set_name}_{scene_name_0}
			- {scene_name_1}
				- data
					- audio
					- comp
					- video
				- {set_name}_{scene_name_1}
		*/

		namespace Names
		{
			static constexpr const char* SetData = "data";
			static constexpr const char* SceneRootPrefix = "_scene";
			static constexpr const char* SceneData = "data";
			static constexpr const char* SceneVideo = "video";
			static constexpr const char* SceneVideoDB = "video_db";
			static constexpr const char* SceneAudio = "audio";
			static constexpr const char* SceneComp = "comp";
		};
	};
}

// NOTE: This entry point is exported through the PiPL (.r file)
extern "C" DllExport AEGP_PluginInitFuncPrototype EntryPointFunc;
