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
			CommentUtil::Property CommentProperty;

			AEGP_ItemH Handle;
			AEGP_ItemFlags Flags;
			AEGP_ItemType Type;
			std::pair<A_long, A_long> Dimensions;
			AEItemData* Parent;

			bool IsParentOf(const AEItemData& parent) const;
		};

		struct WorkingProjectData
		{
			A_long Index;
			AEGP_ProjectH Handle;
			A_char Name[AEGP_MAX_PROJ_NAME_SIZE];
			std::wstring Path;
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
			AEItemData* Folder;
			std::vector<AEItemData*> SceneComps;
		} workingSet;

		void SetupWorkingSetData(Aet::AetSet& set);

	protected:
		struct WorkingSceneData
		{
			Aet::Scene* Scene;
			AEItemData* AESceneComp;
		} workingScene = {};

		void SetupWorkingSceneData(AEItemData* sceneComp);

	protected:
		void ExportScene();
		void ExportAllCompositions();
		void ExportAllFootage();
	};
}
