#pragma once
#include "AetPlugin.h"
#include "Graphics/Auth2D/Aet/AetSet.h"

namespace AetPlugin
{
	using namespace Comfy;
	using namespace Comfy::Graphics;

	class AetExporter : NonCopyable
	{
	public:
		AetExporter(std::wstring_view workingDirectory);
		~AetExporter();

		UniquePtr<Aet::AetSet> ExportAetSet();

	protected:
		SuitesData suites;

	protected:
		struct AEItemData
		{
			std::string Name;
			std::string Comment;

			AEGP_ItemH Handle;
			AEGP_ItemFlags Flags;
			AEGP_ItemType Type;
			std::pair<A_long, A_long> Dimensions;
			AEItemData* Parent;
		};

		struct WorkingProjectData
		{
			A_long Index;
			AEGP_ProjectH Handle;
			A_char Name[AEGP_MAX_PROJ_NAME_SIZE];
			AEGP_MemHandle Path;
			AEGP_ItemH RootFolder;

			std::vector<AEItemData> Items;

		} workingProject = {};

		void SetupWorkingProjectData();

	protected:
		struct WorkingDirectoryData
		{
			std::wstring ImportDirectory;
		} workingDirectory;

		struct WorkingAetData
		{
			Aet::AetSet* Set = nullptr;
		} workingSet;

		void SetupWorkingSetData(Aet::AetSet& set);

	protected:
		struct WorkingSceneData
		{
			Aet::Scene* Scene;
			// TODO:
		} workingScene = {};

		void SetupWorkingSceneData();
	};
}
