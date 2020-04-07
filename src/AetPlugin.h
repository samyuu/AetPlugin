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
	constexpr std::string_view AetPrefix = "aet_";
	constexpr std::wstring_view AetPrefixW = L"aet_";

	constexpr std::string_view SprPrefix = "spr_";
	constexpr std::wstring_view SprPrefixW = L"spr_";

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
		DeclareSuiteMember(ItemSuite8); // AE 9.0
		DeclareSuiteMember(FootageSuite5); // AE 10.0
		DeclareSuiteMember(CompSuite7); // AE 9.0
		DeclareSuiteMember(LayerSuite1); // AE 5.0
		DeclareSuiteMember(LayerSuite3); // AE 6.0
		DeclareSuiteMember(StreamSuite4); // AE 9
		DeclareSuiteMember(DynamicStreamSuite4); // AE 9.0
		DeclareSuiteMember(KeyframeSuite3); // AE 6.5
		DeclareSuiteMember(UtilitySuite3); // AE 10.0
		DeclareSuiteMember(MemorySuite1); // AE 5.0

#undef DeclareSuiteMember
	};

	namespace ProjectStructure
	{
		// NOTE: Currently:
		/*
		- root
			- data
				- audio
				- comp
				- video
			- {set_name}_{scene_name_0}
			- {set_name}_{scene_name_1}
		*/

		// NOTE: But it might be better to:
		/*
		- {set_name}
			- {scene_name_0}
				- data
					- audio
					- comp
					- video
				- {set_name}_{scene_name_0}

		- {set_name}
			- {scene_name_1}
				- data
					- audio
					- comp
					- video
				- {set_name}_{scene_name_1}
		*/

		namespace Names
		{
			static constexpr const char* Root = "root";
			static constexpr const char* Data = "data";
			static constexpr const char* Video = "video";
			static constexpr const char* Audio = "audio";
			static constexpr const char* Comp = "comp";
		};
	};

	namespace Comment
	{
		static constexpr std::string_view Scene = "Scene";
		static constexpr std::string_view SprID = "SprID";

		using Buffer = std::array<char, AEGP_MAX_RQITEM_COMMENT_SIZE>;

		inline Buffer Format(std::string_view propertyID, std::string_view property)
		{
			Buffer buffer;
			sprintf(buffer.data(), "#AetSet.%.*s: { %.*s }", static_cast<int>(propertyID.size()), propertyID.data(), static_cast<int>(property.size()), property.data());
			return buffer;
		}

		inline void Set(const SuitesData& suites, std::string_view propertyID, std::string_view property, AEGP_ItemH item)
		{
			const auto buffer = Format(propertyID, property);
			if (item != nullptr)
				suites.ItemSuite8->AEGP_SetItemComment(item, buffer.data());
		}
	};
}

// NOTE: This entry point is exported through the PiPL (.r file)
extern "C" DllExport AEGP_PluginInitFuncPrototype EntryPointFunc;
