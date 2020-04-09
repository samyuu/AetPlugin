#include "AetExport.h"
#include "FormatUtil.h"
#include "Graphics/Auth2D/Aet/AetMgr.h"
#include "Misc/StringHelper.h"
#include "Resource/IDHash.h"
#include <filesystem>

namespace AetPlugin
{
	AetExporter::AetExporter(std::wstring_view workingDirectory)
	{
		this->workingDirectory.ImportDirectory = workingDirectory;
	}

	AetExporter::~AetExporter()
	{
	}

	UniquePtr<Aet::AetSet> AetExporter::ExportAetSet()
	{
		auto set = MakeUnique<Aet::AetSet>();

		if (set == nullptr)
			return nullptr;

		SetupWorkingProjectData();
		SetupWorkingSetData(*set);

		for (auto sceneComp : workingSet.SceneComps)
		{
			SetupWorkingSceneData(sceneComp);
			ExportScene();
		}

		return set;
	}

	void AetExporter::SetupWorkingProjectData()
	{
		A_long projectCount;
		suites.ProjSuite5->AEGP_GetNumProjects(&projectCount);

		workingProject.Index = 0;
		suites.ProjSuite5->AEGP_GetProjectByIndex(workingProject.Index, &workingProject.Handle);
		suites.ProjSuite5->AEGP_GetProjectName(workingProject.Handle, workingProject.Name);

		AEGP_MemHandle pathHandle;
		suites.ProjSuite5->AEGP_GetProjectPath(workingProject.Handle, &pathHandle);
		workingProject.Path = AEUtil::MoveFreeUTF16String(suites.MemorySuite1, pathHandle);

		suites.ProjSuite5->AEGP_GetProjectRootFolder(workingProject.Handle, &workingProject.RootFolder);

		AEGP_ItemH firstItem;
		suites.ItemSuite8->AEGP_GetFirstProjItem(workingProject.Handle, &firstItem);

		AEGP_ItemH currentItem, previousItem = firstItem;
		while (true)
		{
			if (suites.ItemSuite8->AEGP_GetNextProjItem(workingProject.Handle, previousItem, &currentItem); currentItem == nullptr)
				break;

			previousItem = currentItem;

			auto& item = workingProject.Items.emplace_back();
			item.ItemHandle = currentItem;

			A_char nameBuffer[AEGP_MAX_ITEM_NAME_SIZE];
			suites.ItemSuite1->AEGP_GetItemName(item.ItemHandle, nameBuffer);
			item.Name = nameBuffer;

			A_u_long commentSize;
			suites.ItemSuite8->AEGP_GetItemCommentLength(item.ItemHandle, &commentSize);
			if (commentSize > 0)
			{
				item.Comment.resize(commentSize);
				suites.ItemSuite8->AEGP_GetItemComment(item.ItemHandle, commentSize + 1, item.Comment.data());
				item.CommentProperty = CommentUtil::Detail::Parse(item.Comment);
			}

			suites.ItemSuite8->AEGP_GetItemFlags(item.ItemHandle, &item.Flags);
			suites.ItemSuite8->AEGP_GetItemType(item.ItemHandle, &item.Type);
			suites.ItemSuite8->AEGP_GetItemDimensions(item.ItemHandle, &item.Dimensions.first, &item.Dimensions.second);

			if (item.Type == AEGP_ItemType_COMP)
				suites.CompSuite7->AEGP_GetCompFromItem(item.ItemHandle, &item.CompHandle);
		}

		for (auto& item : workingProject.Items)
		{
			AEGP_ItemH parentHandle;
			suites.ItemSuite8->AEGP_GetItemParentFolder(item.ItemHandle, &parentHandle);

			auto foundParent = std::find_if(workingProject.Items.begin(), workingProject.Items.end(), [&](auto& i) { return i.ItemHandle == parentHandle; });
			item.Parent = (foundParent != workingProject.Items.end()) ? &(*foundParent) : nullptr;
		}
	}

	void AetExporter::SetupWorkingSetData(Aet::AetSet& set)
	{
		workingSet.Set = &set;

		auto foundSetFolder = std::find_if(workingProject.Items.begin(), workingProject.Items.end(), [&](const AEItemData& item)
		{
			return item.CommentProperty.Key == CommentUtil::Keys::AetSet;
		});

		if (foundSetFolder == workingProject.Items.end())
		{
			// TODO: Do some error handling
			set.Name = AetPrefix;
			set.Name += FormatUtil::ToSnakeCaseLower(FormatUtil::StripFileExtension(workingProject.Name));
			workingSet.Folder = nullptr;
		}
		else
		{
			set.Name = foundSetFolder->CommentProperty.Value;
			workingSet.Folder = &(*foundSetFolder);
		}

		workingSet.SceneComps.reserve(2);
		for (auto& item : workingProject.Items)
		{
			if (item.Type == AEGP_ItemType_COMP && item.CommentProperty.Key == CommentUtil::Keys::Scene)
				workingSet.SceneComps.push_back(&item);
		}

		std::sort(workingSet.SceneComps.begin(), workingSet.SceneComps.end(), [&](const auto& sceneA, const auto& sceneB)
		{
			return sceneA->CommentProperty.KeyIndex.value_or(0) < sceneB->CommentProperty.KeyIndex.value_or(0);
		});
	}

	void AetExporter::SetupWorkingSceneData(AEItemData* sceneComp)
	{
		workingScene.Scene = workingSet.Set->GetScenes().emplace_back(MakeRef<Aet::Scene>()).get();
		workingScene.AESceneComp = sceneComp;
	}

	void AetExporter::ExportScene()
	{
		auto& scene = *workingScene.Scene;
		auto& aeSceneComp = *workingScene.AESceneComp;

		scene.Name = aeSceneComp.CommentProperty.Value;

		A_FpLong compFPS;
		suites.CompSuite7->AEGP_GetCompFramerate(aeSceneComp.CompHandle, &compFPS);
		scene.FrameRate = static_cast<frame_t>(compFPS);

		A_Time compWorkAreaStart, compWorkAreaDuration;
		suites.CompSuite7->AEGP_GetCompWorkAreaStart(aeSceneComp.CompHandle, &compWorkAreaStart);
		suites.CompSuite7->AEGP_GetCompWorkAreaDuration(aeSceneComp.CompHandle, &compWorkAreaDuration);
		scene.StartFrame = AEUtil::AETimeToFrame(compWorkAreaStart, scene.FrameRate);
		scene.EndFrame = scene.StartFrame + AEUtil::AETimeToFrame(compWorkAreaDuration, scene.FrameRate);
		
		AEGP_ColorVal compBGColor;
		suites.CompSuite7->AEGP_GetCompBGColor(aeSceneComp.CompHandle, &compBGColor);
		scene.BackgroundColor = AEUtil::ColorRGB8(compBGColor);

		A_long compWidth, compHeight;
		suites.ItemSuite8->AEGP_GetItemDimensions(aeSceneComp.ItemHandle, &compWidth, &compHeight);
		scene.Resolution = { compWidth, compHeight };

		scene.Compositions;
		scene.RootComposition = MakeRef<Aet::Composition>();

		scene.Videos;
		scene.Audios;

		ExportAllCompositions();
		ExportAllFootage();
	}

	void AetExporter::ExportAllCompositions()
	{
	}

	void AetExporter::ExportAllFootage()
	{
		const std::string sprPrefix = FormatUtil::ToUpper(workingSet.Set->Name) + "_";
		const std::string sprHashPrefix = "SPR_" + sprPrefix;

		for (auto& item : workingProject.Items)
		{
			if (item.Type == AEGP_ItemType_FOOTAGE)
			{
				const auto cleanItemName = FormatUtil::ToUpper(FormatUtil::StripPrefixIfExists(FormatUtil::StripFileExtension(item.Name), SprPrefix));
				auto& video = workingScene.Scene->Videos.emplace_back(MakeRef<Aet::Video>());

				suites.FootageSuite5->AEGP_GetMainFootageFromItem(item.ItemHandle, &video->GuiData.AE_Footage);

				AEGP_FootageSignature signature;
				suites.FootageSuite5->AEGP_GetFootageSignature(video->GuiData.AE_Footage, &signature);

				video->Size = { item.Dimensions.first, item.Dimensions.second };

				if (signature == AEGP_FootageSignature_SOLID)
				{
					AEGP_ColorVal footageColor;
					suites.FootageSuite5->AEGP_GetSolidFootageColor(item.ItemHandle, false, &footageColor);

					video->Color = AEUtil::ColorRGB8(footageColor);
				}
				else
				{
					video->Color = 0xFFFFFF00;

					auto& source = video->Sources.emplace_back();
					source.Name = sprPrefix + cleanItemName; // NOTE: {SET_NAME}_{SPRITE_NAME}
					source.ID = HashIDString<SprID>(sprHashPrefix + cleanItemName); // NOTE: SPR_{SET_NAME}_{SPRITE_NAME}
				}

				video->Frames = static_cast<float>(video->Sources.size());
			}
		}
	}

	bool AetExporter::AEItemData::IsParentOf(const AEItemData& parent) const
	{
		if (Parent == nullptr)
			return false;
		else if (Parent == &parent)
			return true;
		return Parent->IsParentOf(parent);
	}
}
