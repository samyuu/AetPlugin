#include "AetImport.h"
#include "FormatUtil.h"
#include "StreamUtil.h"
#include "Graphics/Auth2D/Aet/AetMgr.h"
#include "Misc/StringHelper.h"
#include "FileSystem/Stream/FileStream.h"
#include "FileSystem/BinaryReader.h"
#include "FileSystem/FileHelper.h"
#include <filesystem>

namespace AetPlugin
{
	namespace
	{
		std::string GetAetSetName(const Aet::AetSet& set)
		{
			const std::string lowerCaseSetName = FormatUtil::ToLower(set.Name);
			const auto setNameWithoutAet = std::string_view(lowerCaseSetName).substr(AetPrefix.length());
			return FormatUtil::ToLower(setNameWithoutAet);
		}

		std::string FormatSceneName(const Aet::AetSet& set, const Aet::Scene& scene)
		{
			std::string result;
			result += GetAetSetName(set);
			result += "_";
			result += FormatUtil::ToLower(scene.Name);
			return result;
		}
	}

	UniquePtr<Aet::AetSet> AetImporter::LoadAetSet(std::wstring_view filePath)
	{
		const auto verifyResult = VerifyAetSetImportable(filePath);
		if (verifyResult != AetSetVerifyResult::Valid)
			return nullptr;

		auto set = MakeUnique<Aet::AetSet>();
		set->Name = Utilities::Utf16ToUtf8(FileSystem::GetFileName(filePath, false));
		set->Load(filePath);
		return set;
	}

	AetImporter::AetSetVerifyResult AetImporter::VerifyAetSetImportable(std::wstring_view filePath)
	{
		const auto fileName = FileSystem::GetFileName(filePath, false);
		if (!Utilities::StartsWithInsensitive(fileName, AetPrefixW))
			return AetSetVerifyResult::InvalidPath;

		auto fileStream = FileSystem::FileStream(filePath);
		if (!fileStream.IsOpen())
			return AetSetVerifyResult::InvalidFile;

		auto reader = FileSystem::BinaryReader(fileStream);

		struct SceneRawData
		{
			uint32_t NameOffset;
			frame_t StartFrame;
			frame_t EndFrame;
			frame_t FrameRate;
			uint32_t BackgroundColor;
			int32_t Width;
			int32_t Height;
			uint32_t CameraOffset;
			uint32_t CompositionCount;
			uint32_t CompositionOffset;
			uint32_t VideoCount;
			uint32_t VideoOffset;
			uint32_t AudioCount;
			uint32_t AudioOffset;
		};

		constexpr FileAddr validScenePlusPointerSize = static_cast<FileAddr>(sizeof(SceneRawData) + (sizeof(uint32_t) * 2));
		if (reader.GetLength() < validScenePlusPointerSize)
			return AetSetVerifyResult::InvalidPointer;

		std::array<uint8_t, 4> fileMagic;
		reader.ReadBuffer(fileMagic.data(), sizeof(fileMagic));

		// NOTE: Thanks CS3 for making it this easy
		if (std::memcmp(fileMagic.data(), "AETC", sizeof(fileMagic)) == 0)
			return AetSetVerifyResult::Valid;

		const auto isValidPointer = [&](FileAddr address) { return (address <= reader.GetLength()); };

		reader.SetPosition(static_cast<FileAddr>(0));

		constexpr uint32_t maxReasonableSceneCount = 64;
		std::array<FileAddr, maxReasonableSceneCount> scenePointers = {};

		for (uint32_t i = 0; i < maxReasonableSceneCount; i++)
		{
			scenePointers[i] = reader.ReadPtr();

			if (scenePointers[i] == FileAddr::NullPtr)
				break;
			else if (!isValidPointer(scenePointers[i]))
				return AetSetVerifyResult::InvalidPointer;
		}

		const bool insufficientSceneFileSpace = (scenePointers[0] + static_cast<FileAddr>(sizeof(SceneRawData))) >= reader.GetLength();
		if (scenePointers[0] == FileAddr::NullPtr || insufficientSceneFileSpace)
			return AetSetVerifyResult::InvalidPointer;

		SceneRawData rawScene;
		reader.SetPosition(scenePointers[0]);
		reader.ReadBuffer(&rawScene, sizeof(rawScene));

		const std::array<FileAddr, 5> fileOffsetsToCheck =
		{
			static_cast<FileAddr>(rawScene.NameOffset),
			static_cast<FileAddr>(rawScene.CameraOffset),
			static_cast<FileAddr>(rawScene.CompositionOffset),
			static_cast<FileAddr>(rawScene.VideoOffset),
			static_cast<FileAddr>(rawScene.AudioOffset),
		};

		if (!std::any_of(fileOffsetsToCheck.begin(), fileOffsetsToCheck.end(), isValidPointer))
			return AetSetVerifyResult::InvalidPointer;

		const std::array<uint32_t, 3> arraySizesToCheck =
		{
			rawScene.CompositionCount,
			rawScene.VideoCount,
			rawScene.AudioCount,
		};

		constexpr uint32_t reasonableArraySizeLimit = 999999;
		if (std::any_of(arraySizesToCheck.begin(), arraySizesToCheck.end(), [&](const auto size) { return size > reasonableArraySizeLimit; }))
			return AetSetVerifyResult::InvalidCount;

		const auto isReasonableFrameRangeDuration = [&](frame_t frame)
		{
			constexpr frame_t reasonableStartEndFrameLimit = 100000.0f;
			return (frame > -reasonableStartEndFrameLimit) && (frame < +reasonableStartEndFrameLimit);
		};

		const auto isReasonableFrameRate = [](frame_t frameRate)
		{
			constexpr frame_t reasonableFrameRateLimit = 999.0f;
			return (frameRate > 0.0f && frameRate <= reasonableFrameRateLimit);
		};

		const auto isReasonableSize = [](int32_t size)
		{
			constexpr int32_t reasonableSizeLimit = 30000;
			return (size > 0 && size <= reasonableSizeLimit);
		};

		if (!isReasonableFrameRangeDuration(rawScene.StartFrame) ||
			!isReasonableFrameRangeDuration(rawScene.EndFrame) ||
			!isReasonableFrameRate(rawScene.FrameRate) ||
			!isReasonableSize(rawScene.Width) ||
			!isReasonableSize(rawScene.Height))
			return AetSetVerifyResult::InvalidCount;

		return AetSetVerifyResult::Valid;
	}

	AetImporter::AetImporter(std::wstring_view workingDirectory)
	{
		this->workingDirectory.ImportDirectory = workingDirectory;
	}

	A_Err AetImporter::ImportAetSet(Aet::AetSet& set, AE_FIM_ImportOptions importOptions, AE_FIM_SpecialAction action, AEGP_ItemH itemHandle)
	{
		SetupWorkingSetData(set);
		CheckWorkingDirectorySpriteFiles();

		GetProjectHandles();
		CreateProjectFolders();

		size_t sceneIndex = 0;
		for (const auto& scene : workingSet.Set->GetScenes())
		{
			SetupWorkingSceneData(*scene, sceneIndex++);
			CreateSceneFolders();
			ImportAllFootage();
			ImportAllCompositions();
		}

		// suites.UtilitySuite3->AEGP_ReportInfo(GlobalPluginID, "Let's hope for the best...");
		return A_Err_NONE;
	}

	const SpriteFileData* AetImporter::FindMatchingSpriteFile(std::string_view sourceName) const
	{
		const auto& spriteFiles = workingDirectory.AvailableSpriteFiles;
		auto matchingSpriteFile = std::find_if(spriteFiles.begin(), spriteFiles.end(), [&](auto& spriteFile)
		{
			return Utilities::MatchesInsensitive(spriteFile.SanitizedFileName, sourceName);
		});

		return (matchingSpriteFile != spriteFiles.end()) ? &(*matchingSpriteFile) : nullptr;
	}

	void AetImporter::SetupWorkingSetData(const Aet::AetSet& set)
	{
		workingSet.Set = &set;
		workingSet.NamePrefix = GetAetSetName(set);
		workingSet.NamePrefixUnderscore = workingSet.NamePrefix + "_";
	}

	void AetImporter::SetupWorkingSceneData(const Aet::Scene& scene, size_t sceneIndex)
	{
		workingScene.Scene = &scene;
		workingScene.SceneIndex = sceneIndex;
		workingScene.AE_FrameRate = { static_cast<A_long>(scene.FrameRate * AEUtil::FixedPoint), static_cast<A_u_long>(AEUtil::FixedPoint) };

		workingScene.SourcelessVideoLayerUsages.clear();
		auto setCompUsages = [&](const Aet::Composition& comp)
		{
			for (const auto& layer : comp.GetLayers())
				if (layer->GetVideoItem() != nullptr && layer->GetVideoItem()->Sources.empty())
					workingScene.SourcelessVideoLayerUsages[layer->GetVideoItem().get()] = layer.get();
		};
		setCompUsages(*scene.RootComposition);
		for (const auto& comp : scene.Compositions)
			setCompUsages(*comp);
	}

	void AetImporter::CheckWorkingDirectorySpriteFiles()
	{
		const auto directoryIterator = std::filesystem::directory_iterator(workingDirectory.ImportDirectory);
		for (const auto& stdPath : directoryIterator)
		{
			const auto path = stdPath.path();
			const auto fileName = path.filename().string();

			if (!Utilities::EndsWithInsensitive(fileName, SpriteFileData::PngExtension))
				continue;

			std::string_view sanitizedFileName = std::string_view(fileName).substr(0, fileName.length() - SpriteFileData::PngExtension.size());

			sanitizedFileName = FormatUtil::StripPrefixIfExists(sanitizedFileName, SpriteFileData::SprPrefix);
			sanitizedFileName = FormatUtil::StripPrefixIfExists(sanitizedFileName, workingSet.NamePrefixUnderscore);

			SpriteFileData& spriteFile = workingDirectory.AvailableSpriteFiles.emplace_back();
			spriteFile.SanitizedFileName = sanitizedFileName;
			spriteFile.FilePath = path.wstring();
		}
	}

	A_Time AetImporter::FrameToAETime(frame_t frame) const
	{
		return AEUtil::FrameToAETime(frame, workingScene.Scene->FrameRate);
	}

	void AetImporter::GetProjectHandles()
	{
		suites.ProjSuite5->AEGP_GetProjectByIndex(0, &project.ProjectHandle);
		suites.ProjSuite5->AEGP_GetProjectRootFolder(project.ProjectHandle, &project.RootItemHandle);
	}

	void AetImporter::CreateProjectFolders()
	{
		const auto& setRootName = workingSet.NamePrefix;
		suites.ItemSuite1->AEGP_CreateNewFolder(setRootName.c_str(), project.RootItemHandle, &project.Folders.Root);

		CommentUtil::Set(suites.ItemSuite8, project.Folders.Root, { CommentUtil::Keys::AetSet, workingSet.Set->Name });
	}

	void AetImporter::CreateSceneFolders()
	{
		const auto sceneRootName = FormatUtil::ToLower(workingScene.Scene->Name);
		suites.ItemSuite1->AEGP_CreateNewFolder(sceneRootName.c_str(), project.Folders.Root, &project.Folders.Scene.Root);
		suites.ItemSuite1->AEGP_CreateNewFolder(ProjectStructure::Names::Data, project.Folders.Scene.Root, &project.Folders.Scene.Data);
		suites.ItemSuite1->AEGP_CreateNewFolder(ProjectStructure::Names::Video, project.Folders.Scene.Data, &project.Folders.Scene.Video);
		suites.ItemSuite1->AEGP_CreateNewFolder(ProjectStructure::Names::Audio, project.Folders.Scene.Data, &project.Folders.Scene.Audio);
		suites.ItemSuite1->AEGP_CreateNewFolder(ProjectStructure::Names::Comp, project.Folders.Scene.Data, &project.Folders.Scene.Comp);
	}

	void AetImporter::ImportAllFootage()
	{
		for (const auto& video : workingScene.Scene->Videos)
			ImportVideo(*video);

		for (const auto& audio : workingScene.Scene->Audios)
			ImportAudio(*audio);
	}

	void AetImporter::ImportAllCompositions()
	{
		ImportSceneComps();

		ImportLayersInComp(*workingScene.Scene->RootComposition);
		for (int i = static_cast<int>(workingScene.Scene->Compositions.size()) - 1; i >= 0; i--)
			ImportLayersInComp(*workingScene.Scene->Compositions[i]);
	}

	void AetImporter::ImportVideo(const Aet::Video& video)
	{
		if (video.Sources.empty())
			ImportPlaceholderVideo(video);
		else
			ImportSpriteVideo(video);
	}

	void AetImporter::ImportPlaceholderVideo(const Aet::Video& video)
	{
		const AEGP_ColorVal videoColor = AEUtil::ColorRGB8(video.Color);

		if (auto layerUsage = workingScene.SourcelessVideoLayerUsages.find(&video); layerUsage != workingScene.SourcelessVideoLayerUsages.end())
		{
			suites.FootageSuite5->AEGP_NewSolidFootage(layerUsage->second->GetName().c_str(), video.Size.x, video.Size.y, &videoColor, &video.GuiData.AE_Footage);
		}
		else
		{
			char placeholderNameBuffer[AEGP_MAX_ITEM_NAME_SIZE];
			sprintf(placeholderNameBuffer, "Placeholder (%dx%d)", video.Size.x, video.Size.y);

			suites.FootageSuite5->AEGP_NewSolidFootage(placeholderNameBuffer, video.Size.x, video.Size.y, &videoColor, &video.GuiData.AE_Footage);
		}

		ImportVideoAddItemToProject(video);
	}

	void AetImporter::ImportSpriteVideo(const Aet::Video& video)
	{
		const auto frontSourceNameWithoutAetPrefix = FormatUtil::StripPrefixIfExists(video.Sources.front().Name, workingSet.NamePrefixUnderscore);
		if (auto matchingSpriteFile = FindMatchingSpriteFile(frontSourceNameWithoutAetPrefix); matchingSpriteFile != nullptr)
		{
			AEGP_FootageLayerKey footageLayerKey = {};
			footageLayerKey.layer_idL = AEGP_LayerID_UNKNOWN;
			footageLayerKey.layer_indexL = 0;

			if (video.Sources.size() > 1)
			{
				// BUG: The Footage name still isn't correct
				const auto sequenceBaseName = FormatUtil::StripSuffixIfExists(frontSourceNameWithoutAetPrefix, "_000");
				std::memcpy(footageLayerKey.nameAC, sequenceBaseName.data(), sequenceBaseName.size());
			}

			AEGP_FileSequenceImportOptions sequenceImportOptions = {};
			sequenceImportOptions.all_in_folderB = (video.Sources.size() > 1);
			sequenceImportOptions.force_alphabeticalB = false;
			sequenceImportOptions.start_frameL = 0;
			sequenceImportOptions.end_frameL = static_cast<A_long>(video.Sources.size()) - 1;

			suites.FootageSuite5->AEGP_NewFootage(EvilGlobalState.PluginID, AEUtil::UTF16Cast(matchingSpriteFile->FilePath.c_str()), &footageLayerKey, &sequenceImportOptions, false, nullptr, &video.GuiData.AE_Footage);
		}
		else
		{
			constexpr frame_t placeholderDuration = 270.0f;

			const A_Time duration = FrameToAETime(placeholderDuration);
			suites.FootageSuite5->AEGP_NewPlaceholderFootage(EvilGlobalState.PluginID, frontSourceNameWithoutAetPrefix.data(), video.Size.x, video.Size.y, &duration, &video.GuiData.AE_Footage);
		}

		ImportVideoAddItemToProject(video);
		ImportVideoSetSprIDComment(video);
		ImportVideoSetSequenceInterpretation(video);
		ImportVideoSetItemName(video);
	}

	void AetImporter::ImportVideoAddItemToProject(const Aet::Video& video)
	{
		if (video.GuiData.AE_Footage != nullptr)
			suites.FootageSuite5->AEGP_AddFootageToProject(video.GuiData.AE_Footage, project.Folders.Scene.Video, &video.GuiData.AE_FootageItem);
	}

	void AetImporter::ImportVideoSetSprIDComment(const Aet::Video& video)
	{
		if (video.Sources.empty())
			return;

		CommentUtil::Buffer commentBuffer = {};
		for (auto& source : video.Sources)
		{
			char appendBuffer[32];
			sprintf(appendBuffer, "0x%02X", source.ID);
			std::strcat(commentBuffer.data(), appendBuffer);

			if (&source != &video.Sources.back())
				std::strcat(commentBuffer.data(), ", ");
		}

		CommentUtil::Set(suites.ItemSuite8, video.GuiData.AE_FootageItem, { CommentUtil::Keys::SprID, commentBuffer.data() });
	}

	void AetImporter::ImportVideoSetSequenceInterpretation(const Aet::Video& video)
	{
		if (video.Sources.size() <= 1)
			return;

		AEGP_FootageInterp interpretation;
		suites.FootageSuite5->AEGP_GetFootageInterpretation(video.GuiData.AE_FootageItem, false, &interpretation);
		interpretation.native_fpsF = workingScene.Scene->FrameRate;
		interpretation.conform_fpsF = workingScene.Scene->FrameRate;
		suites.FootageSuite5->AEGP_SetFootageInterpretation(video.GuiData.AE_FootageItem, false, &interpretation);
	}

	void AetImporter::ImportVideoSetItemName(const Aet::Video& video)
	{
		if (video.Sources.empty())
			return;

		AEGP_MemHandle nameHandle;
		suites.ItemSuite8->AEGP_GetItemName(EvilGlobalState.PluginID, video.GuiData.AE_FootageItem, &nameHandle);
		const auto itemName = Utf16ToUtf8(AEUtil::MoveFreeUTF16String(suites.MemorySuite1, nameHandle));

		// TODO: Should the set name be appended (?)
		const auto cleanName = FormatUtil::StripPrefixIfExists(FormatUtil::StripPrefixIfExists(itemName, SprPrefix), workingSet.NamePrefixUnderscore);
		const auto setCleanName = /*workingSet.NamePrefixUnderscore +*/ FormatUtil::ToLower(cleanName);

		suites.ItemSuite8->AEGP_SetItemName(video.GuiData.AE_FootageItem, AEUtil::UTF16Cast(Utf8ToUtf16(setCleanName).c_str()));
	}

	void AetImporter::ImportAudio(const Aet::Audio& audio)
	{
		// TODO:
		if (audio.GuiData.AE_Footage != nullptr)
			suites.FootageSuite5->AEGP_AddFootageToProject(audio.GuiData.AE_Footage, project.Folders.Scene.Video, &audio.GuiData.AE_FootageItem);
	}

	void AetImporter::ImportSceneComps()
	{
		const auto& scene = *workingScene.Scene;

		const A_Time sceneDuration = FrameToAETime(scene.EndFrame);
		const auto sceneName = FormatSceneName(*workingSet.Set, scene);

		suites.CompSuite7->AEGP_CreateComp(project.Folders.Scene.Root, AEUtil::UTF16Cast(Utf8ToUtf16(sceneName).c_str()), scene.Resolution.x, scene.Resolution.y, &AEUtil::OneToOneRatio, &sceneDuration, &workingScene.AE_FrameRate, &scene.RootComposition->GuiData.AE_Comp);
		suites.CompSuite7->AEGP_GetItemFromComp(scene.RootComposition->GuiData.AE_Comp, &scene.RootComposition->GuiData.AE_CompItem);

		const auto backgroundColor = AEUtil::ColorRGB8(scene.BackgroundColor);
		suites.CompSuite7->AEGP_SetCompBGColor(scene.RootComposition->GuiData.AE_Comp, &backgroundColor);

		auto givenCompDurations = CreateGivenCompDurationsMap(scene);
		for (const auto& comp : scene.Compositions)
		{
			const frame_t frameDuration = givenCompDurations[comp.get()];
			const A_Time duration = FrameToAETime(frameDuration);

			suites.CompSuite7->AEGP_CreateComp(project.Folders.Scene.Comp, AEUtil::UTF16Cast(Utf8ToUtf16(comp->GetName()).c_str()), scene.Resolution.x, scene.Resolution.y, &AEUtil::OneToOneRatio, &duration, &workingScene.AE_FrameRate, &comp->GuiData.AE_Comp);
			suites.CompSuite7->AEGP_GetItemFromComp(comp->GuiData.AE_Comp, &comp->GuiData.AE_CompItem);
		}

		CommentUtil::Set(suites.ItemSuite8, scene.RootComposition->GuiData.AE_CompItem, { CommentUtil::Keys::Scene, scene.Name, workingScene.SceneIndex });
	}

	void AetImporter::ImportLayersInComp(const Aet::Composition& comp)
	{
		for (int i = static_cast<int>(comp.GetLayers().size()) - 1; i >= 0; i--)
			ImportLayer(comp, *comp.GetLayers()[i]);

		for (const auto& layer : comp.GetLayers())
			SetLayerRefParentLayer(*layer);
	}

	void AetImporter::ImportLayer(const Aet::Composition& parentComp, const Aet::Layer& layer)
	{
		ImportLayerItemToComp(parentComp, layer);

		if (layer.GuiData.AE_Layer == nullptr)
			return;

		if (layer.LayerVideo != nullptr)
			ImportLayerVideo(layer);

		if (layer.LayerAudio != nullptr)
			ImportLayerAudio(layer);

		ImportLayerTiming(layer);
		ImportLayerName(layer);
		ImportLayerFlags(layer);
		ImportLayerQuality(layer);
		ImportLayerMarkers(layer);
	}

	void AetImporter::ImportLayerItemToComp(const Aet::Composition& parentComp, const Aet::Layer& layer)
	{
		auto tryAddAEFootageLayerToComp = [&](const auto* layerItem)
		{
			if (layerItem != nullptr && layerItem->GuiData.AE_FootageItem != nullptr)
				suites.LayerSuite3->AEGP_AddLayer(layerItem->GuiData.AE_FootageItem, parentComp.GuiData.AE_Comp, &layer.GuiData.AE_Layer);
		};

		auto tryAddAECompLayerToComp = [&](const auto* layerItem)
		{
			if (layerItem != nullptr && layerItem->GuiData.AE_CompItem != nullptr)
				suites.LayerSuite3->AEGP_AddLayer(layerItem->GuiData.AE_CompItem, parentComp.GuiData.AE_Comp, &layer.GuiData.AE_Layer);
		};

		if (layer.ItemType == Aet::ItemType::Video)
			tryAddAEFootageLayerToComp(layer.GetVideoItem());
		else if (layer.ItemType == Aet::ItemType::Audio)
			tryAddAEFootageLayerToComp(layer.GetAudioItem());
		else if (layer.ItemType == Aet::ItemType::Composition)
			tryAddAECompLayerToComp(layer.GetCompItem());
	}


	std::unordered_map<const Aet::Composition*, frame_t> AetImporter::CreateGivenCompDurationsMap(const Aet::Scene& scene)
	{
		std::unordered_map<const Aet::Composition*, frame_t> givenCompDurations;

		auto registerGivenCompDurations = [&givenCompDurations](const Aet::Composition& compToRegister)
		{
			for (const auto& layer : compToRegister.GetLayers())
			{
				if (layer->ItemType != Aet::ItemType::Composition || layer->GetCompItem() == nullptr)
					continue;

				frame_t& mapDuration = givenCompDurations[layer->GetCompItem().get()];
				mapDuration = std::max(layer->EndFrame, mapDuration);
			}
		};

		givenCompDurations.reserve(scene.Compositions.size() + 1);
		givenCompDurations[scene.RootComposition.get()] = scene.EndFrame;

		registerGivenCompDurations(*scene.RootComposition);

		for (const auto& comp : scene.Compositions)
			registerGivenCompDurations(*comp);

		return givenCompDurations;
	}

	bool AetImporter::LayerUsesStartOffset(const Aet::Layer& layer)
	{
		if (layer.ItemType == Aet::ItemType::Video)
		{
			if (layer.GetVideoItem() != nullptr && layer.GetVideoItem()->Sources.size() > 1)
				return true;

			return false;
		}
		else if (layer.ItemType == Aet::ItemType::Composition)
			return true;
		else
			return false;
	}

	void AetImporter::ImportLayerVideo(const Aet::Layer& layer)
	{
		ImportLayerTransferMode(layer, layer.LayerVideo->TransferMode);
		ImportLayerVideoStream(layer, *layer.LayerVideo);
	}

	void AetImporter::ImportLayerTransferMode(const Aet::Layer& layer, const Aet::LayerTransferMode& transferMode)
	{
		AEGP_LayerTransferMode layerTransferMode = {};
		layerTransferMode.mode = (static_cast<PF_TransferMode>(transferMode.BlendMode) - 1);
		layerTransferMode.flags = static_cast<AEGP_TransferFlags>(*reinterpret_cast<const uint8_t*>(&transferMode.Flags));
		layerTransferMode.track_matte = static_cast<AEGP_TrackMatte>(transferMode.TrackMatte);

		suites.LayerSuite3->AEGP_SetLayerTransferMode(layer.GuiData.AE_Layer, &layerTransferMode);
	}

	void AetImporter::ImportLayerVideoStream(const Aet::Layer& layer, const Aet::LayerVideo& layerVideo)
	{
		for (const auto& aetToAEStream : StreamUtil::Transform2DRemapData)
		{
			AEGP_StreamValue2 streamValue2 = {};
			suites.StreamSuite4->AEGP_GetNewLayerStream(EvilGlobalState.PluginID, layer.GuiData.AE_Layer, aetToAEStream.StreamType, &streamValue2.streamH);

			const bool singleProperty = (aetToAEStream.FieldX == aetToAEStream.FieldY);
			const Aet::Property1D& xKeyFrames = layerVideo.Transform[aetToAEStream.FieldX];
			const Aet::Property1D& yKeyFrames = layerVideo.Transform[aetToAEStream.FieldY];

			// NOTE: Set initial stream value
			streamValue2.val.two_d.x = !xKeyFrames->empty() ? (xKeyFrames->front().Value * aetToAEStream.ScaleFactor) : 0.0f;
			streamValue2.val.two_d.y = !yKeyFrames->empty() ? (yKeyFrames->front().Value * aetToAEStream.ScaleFactor) : 0.0f;
			suites.StreamSuite4->AEGP_SetStreamValue(EvilGlobalState.PluginID, streamValue2.streamH, &streamValue2);

			if (xKeyFrames.Keys.size() <= 1 && yKeyFrames.Keys.size() <= 1)
				continue;

#if 0
			if (!singleProperty)
			{
				// BUG: Only the position can be separated. WTF?
				A_Boolean isSeparationLeader;
				suites.DynamicStreamSuite4->AEGP_IsSeparationLeader(streamValue2.streamH, &isSeparationLeader);

				if (isSeparationLeader)
				{
					suites.DynamicStreamSuite4->AEGP_SetDimensionsSeparated(streamValue2.streamH, true);
					// continue;
				}
			}
#endif

			AEGP_AddKeyframesInfoH addKeyFrameInfo;
			suites.KeyframeSuite3->AEGP_StartAddKeyframes(streamValue2.streamH, &addKeyFrameInfo);

			auto insertStreamKeyFrame = [&](frame_t frame, float xValue, float yValue)
			{
				// TODO: Is this correct (?)
				const frame_t startOffset = LayerUsesStartOffset(layer) ? layer.StartOffset : 0.0f;
				const A_Time time = FrameToAETime(frame - layer.StartFrame + startOffset);

				AEGP_KeyframeIndex index;
				suites.KeyframeSuite3->AEGP_AddKeyframes(addKeyFrameInfo, AEGP_LTimeMode_LayerTime, &time, &index);

				AEGP_StreamValue streamValue = {};
				streamValue.streamH = streamValue2.streamH;
				streamValue.val.two_d.x = xValue * aetToAEStream.ScaleFactor;
				streamValue.val.two_d.y = yValue * aetToAEStream.ScaleFactor;
				suites.KeyframeSuite3->AEGP_SetAddKeyframe(addKeyFrameInfo, index, &streamValue);
			};

			if (singleProperty)
			{
				for (const auto& keyFrame : xKeyFrames.Keys)
					insertStreamKeyFrame(keyFrame.Frame, keyFrame.Value, 0.0f);
			}
			else
			{
				CombineXYPropertiesToKeyFrameVec2s(xKeyFrames, yKeyFrames, combinedVec2KeyFramesCache);

				for (const KeyFrameVec2& keyFrame : combinedVec2KeyFramesCache)
					insertStreamKeyFrame(keyFrame.Frame, keyFrame.Value.x, keyFrame.Value.y);
			}

			suites.KeyframeSuite3->AEGP_EndAddKeyframes(true, addKeyFrameInfo);
		}
	}

	namespace
	{
		template <class KeyFrameType>
		const KeyFrameType* FindKeyFrameAt(frame_t frame, const std::vector<KeyFrameType>& keyFrames)
		{
			auto found = std::find_if(keyFrames.begin(), keyFrames.end(), [&](auto& k) { return Aet::AetMgr::AreFramesTheSame(k.Frame, frame); });
			return (found != keyFrames.end()) ? &(*found) : nullptr;
		}
	}

	void AetImporter::CombineXYPropertiesToKeyFrameVec2s(const Aet::Property1D& propertyX, const Aet::Property1D& propertyY, std::vector<KeyFrameVec2>& outCombinedKeyFrames) const
	{
		outCombinedKeyFrames.clear();
		outCombinedKeyFrames.reserve(propertyX->size() + propertyY->size());

		for (const auto& xKeyFrame : propertyX.Keys)
		{
			const auto matchingYKeyFrame = FindKeyFrameAt<Aet::KeyFrame>(xKeyFrame.Frame, propertyY.Keys);
			const vec2 value =
			{
				xKeyFrame.Value,
				(matchingYKeyFrame != nullptr) ? matchingYKeyFrame->Value : Aet::AetMgr::GetValueAt(propertyY, xKeyFrame.Frame),
			};
			outCombinedKeyFrames.push_back({ xKeyFrame.Frame, value, xKeyFrame.Curve });
		}

		for (const auto& yKeyFrame : propertyY.Keys)
		{
			if (auto existingKeyFrame = FindKeyFrameAt<KeyFrameVec2>(yKeyFrame.Frame, outCombinedKeyFrames); existingKeyFrame != nullptr)
				continue;

			const float xValue = Aet::AetMgr::GetValueAt(propertyX, yKeyFrame.Frame);
			outCombinedKeyFrames.push_back({ yKeyFrame.Frame, vec2(xValue, yKeyFrame.Value), yKeyFrame.Curve });
		}

		std::sort(outCombinedKeyFrames.begin(), outCombinedKeyFrames.end(), [](const auto& a, const auto& b) { return a.Frame < b.Frame; });
	}

	std::string_view AetImporter::GetLayerItemName(const Aet::Layer& layer) const
	{
		if (layer.GetVideoItem() != nullptr)
		{
			if (layer.GetVideoItem()->Sources.size() == 1)
				return layer.GetVideoItem()->Sources.front().Name;
			else if (auto layerUsage = workingScene.SourcelessVideoLayerUsages.find(layer.GetVideoItem()); layerUsage != workingScene.SourcelessVideoLayerUsages.end())
				return layerUsage->second->GetName();
			else
				return "";
		}
		else if (layer.GetAudioItem() != nullptr)
			return "";
		else if (layer.GetCompItem() != nullptr)
			return layer.GetCompItem()->GetName();
		else
			return "";
	}

	void AetImporter::ImportLayerAudio(const Aet::Layer& layer)
	{
		// NOTE: This is unused by the game so this can be ignored for now
	}

	void AetImporter::ImportLayerTiming(const Aet::Layer& layer)
	{
		// TODO: This still isn't entirely accurate it seems

		const A_Time inPoint = LayerUsesStartOffset(layer) ?
			FrameToAETime(layer.StartOffset) :
			FrameToAETime(0.0f);
		const A_Time duration = FrameToAETime((layer.EndFrame - layer.StartFrame) * layer.TimeScale);
		suites.LayerSuite3->AEGP_SetLayerInPointAndDuration(layer.GuiData.AE_Layer, AEGP_LTimeMode_CompTime, &inPoint, &duration);

		const A_Time offset = LayerUsesStartOffset(layer) ?
			FrameToAETime((-layer.StartOffset / layer.TimeScale) + layer.StartFrame) :
			FrameToAETime(layer.StartFrame);
		suites.LayerSuite3->AEGP_SetLayerOffset(layer.GuiData.AE_Layer, &offset);

		const A_Ratio stretch = AEUtil::Ratio(1.0f / layer.TimeScale);
		suites.LayerSuite1->AEGP_SetLayerStretch(layer.GuiData.AE_Layer, &stretch);
	}

	void AetImporter::ImportLayerName(const Aet::Layer& layer)
	{
		// NOTE: To ensure square brackets appear around the layer name in the layer name view
		if (GetLayerItemName(layer) != layer.GetName())
			suites.LayerSuite1->AEGP_SetLayerName(layer.GuiData.AE_Layer, layer.GetName().c_str());
	}

	void AetImporter::ImportLayerFlags(const Aet::Layer& layer)
	{
		const uint16_t layerFlags = *(const uint16_t*)(&layer.Flags);
		for (size_t flagsBitIndex = 0; flagsBitIndex < sizeof(layerFlags) * CHAR_BIT; flagsBitIndex++)
		{
			const uint16_t flagsBitMask = (1 << flagsBitIndex);
			suites.LayerSuite1->AEGP_SetLayerFlag(layer.GuiData.AE_Layer, static_cast<AEGP_LayerFlags>(flagsBitMask), static_cast<A_Boolean>(layerFlags & flagsBitMask));
		}

		// NOTE: Makes sure underlying transfer modes etc are being preserved as well as the underlying layers aren't being cut off outside the comp region
		if (layer.ItemType == Aet::ItemType::Composition)
			suites.LayerSuite1->AEGP_SetLayerFlag(layer.GuiData.AE_Layer, AEGP_LayerFlag_COLLAPSE, true);
	}

	void AetImporter::ImportLayerQuality(const Aet::Layer& layer)
	{
		suites.LayerSuite1->AEGP_SetLayerQuality(layer.GuiData.AE_Layer, (static_cast<AEGP_LayerQuality>(layer.Quality) - 1));
	}

	void AetImporter::ImportLayerMarkers(const Aet::Layer& layer)
	{
		if (layer.Markers.empty())
			return;

		AEGP_StreamValue2 streamValue2 = {};
		suites.StreamSuite4->AEGP_GetNewLayerStream(EvilGlobalState.PluginID, layer.GuiData.AE_Layer, AEGP_LayerStream_MARKER, &streamValue2.streamH);

		for (const auto& marker : layer.Markers)
		{
			AEGP_MarkerVal markerValue = {};
			AEGP_MarkerVal* markerValuePtr = &markerValue;
			std::memcpy(markerValue.nameAC, marker->Name.data(), marker->Name.size());

			AEGP_StreamValue streamValue = {};
			streamValue.streamH = streamValue2.streamH;
			streamValue.val.markerH = &markerValuePtr;

			const A_Time time = FrameToAETime(marker->Frame);
			AEGP_KeyframeIndex index;

			// NOTE: AEGP_StartAddKeyframes / AEGP_EndAddKeyframes is not supported for markers
			// suites.KeyframeSuite3->AEGP_InsertKeyframe(streamValue.streamH, AEGP_LTimeMode_LayerTime, &time, &index);
			suites.KeyframeSuite3->AEGP_InsertKeyframe(streamValue.streamH, AEGP_LTimeMode_CompTime, &time, &index);
			suites.KeyframeSuite3->AEGP_SetKeyframeValue(streamValue.streamH, index, &streamValue);
		}
	}

	void AetImporter::SetLayerRefParentLayer(const Aet::Layer& layer)
	{
		if (layer.GetRefParentLayer() == nullptr)
			return;

		if (layer.GuiData.AE_Layer != nullptr && layer.GetRefParentLayer()->GuiData.AE_Layer != nullptr)
			suites.LayerSuite3->AEGP_SetLayerParent(layer.GuiData.AE_Layer, layer.GetRefParentLayer()->GuiData.AE_Layer);
	}
}
