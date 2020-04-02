#include "AetImport.h"
#include "Graphics/Auth2D/Aet/AetMgr.h"
#include "Misc/StringHelper.h"
#include "FileSystem/FileHelper.h"
#include <algorithm>
#include <numeric>
#include <filesystem>

namespace AetPlugin
{
	namespace
	{
		char ToLower(char input)
		{
			constexpr char caseDifference = ('A' - 'a');
			if (input >= 'A' && input <= 'Z')
				return input - caseDifference;
			return input;
		}

		std::string ToLower(std::string_view value)
		{
			auto lowerCaseString = std::string(value);
			for (auto& character : lowerCaseString)
				character = ToLower(character);
			return lowerCaseString;
		}

		std::string GetAetSetName(const Aet::AetSet& set)
		{
			const std::string lowerCaseSetName = ToLower(set.Name);
			const auto setNameWithoutAet = std::string_view(lowerCaseSetName).substr(std::strlen("aet_"));
			return ToLower(setNameWithoutAet);
		}

		std::string FormatSceneName(const Aet::AetSet& set, const Aet::Scene& scene)
		{
			std::string result;
			result += GetAetSetName(set);
			result += "_";
			result += ToLower(scene.Name);
			return result;
		}

		std::string_view StripPrefixIfExists(std::string_view stringInput, std::string_view prefix)
		{
			if (Utilities::StartsWithInsensitive(stringInput, prefix))
				return stringInput.substr(prefix.size(), stringInput.size() - prefix.size());

			return stringInput;
		}
	}

	UniquePtr<Aet::AetSet> AetImporter::LoadAetSet(std::wstring_view filePath)
	{
		auto set = MakeUnique<Aet::AetSet>();
		set->Name = Utilities::Utf16ToUtf8(FileSystem::GetFileName(filePath, false));
		set->Load(filePath);
		return set;
	}

	A_Err AetImporter::VerifyAetSetImportable(std::wstring_view filePath, bool& canImport)
	{
		const auto fileName = FileSystem::GetFileName(filePath, false);

		if (!Utilities::StartsWithInsensitive(fileName, L"aet_"))
		{
			canImport = false;
			return A_Err_NONE;
		}

		// TODO: Check file properties
		canImport = true;

		return A_Err_NONE;
	}

	AetImporter::AetImporter(std::wstring_view workingDirectory)
	{
		this->workingDirectory.ImportDirectory = workingDirectory;
	}

	A_Err AetImporter::ImportAetSet(Aet::AetSet& set, AE_FIM_ImportOptions importOptions, AE_FIM_SpecialAction action, AEGP_ItemH itemHandle)
	{
		Aet::Scene& mainScene = *set.GetScenes().front();

		SetupWorkingAetSceneData(set, mainScene);
		CheckWorkingDirectorySpriteFiles();

		GetProjectHandles();
		CreateProjectFolders();

		ImportAllFootage(set, mainScene);
		ImportAllCompositions(set, mainScene);

		// suites.UtilitySuite3()->AEGP_ReportInfo(GlobalPluginID, "Let's hope for the best...");
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

	void AetImporter::SetupWorkingAetSceneData(Aet::AetSet& set, const Aet::Scene& mainScene)
	{
		workingAet.SpriteNamePrefix = GetAetSetName(set);
		workingAet.SpriteNamePrefixUnderscore = workingAet.SpriteNamePrefix + "_";

		workingScene.FrameRate = mainScene.FrameRate;
		workingScene.AE_FrameRate = { static_cast<A_long>(mainScene.FrameRate * AEUtil::FixedPoint), static_cast<A_u_long>(AEUtil::FixedPoint) };;
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

			sanitizedFileName = StripPrefixIfExists(sanitizedFileName, SpriteFileData::SprPrefix);
			sanitizedFileName = StripPrefixIfExists(sanitizedFileName, workingAet.SpriteNamePrefixUnderscore);

			SpriteFileData& spriteFile = workingDirectory.AvailableSpriteFiles.emplace_back();
			spriteFile.SanitizedFileName = sanitizedFileName;
			spriteFile.FilePath = path.wstring();
		}
	}

	A_Time AetImporter::FrameToAETime(frame_t frame) const
	{
		return { static_cast<A_long>(frame * AEUtil::FixedPoint), static_cast<A_u_long>(workingScene.FrameRate * AEUtil::FixedPoint) };
	}

	frame_t AetImporter::AETimeToFrame(A_Time time) const
	{
		return static_cast<frame_t>(time.value / time.scale);
	}

	void AetImporter::GetProjectHandles()
	{
		suites.ProjSuite5->AEGP_GetProjectByIndex(0, &project.ProjectHandle);
		suites.ProjSuite5->AEGP_GetProjectRootFolder(project.ProjectHandle, &project.RootItemHandle);
	}

	void AetImporter::CreateProjectFolders()
	{
		suites.ItemSuite8->AEGP_CreateNewFolder(AEUtil::UTF16Cast(L"root"), project.RootItemHandle, &project.Folders.Root);
		suites.ItemSuite8->AEGP_CreateNewFolder(AEUtil::UTF16Cast(L"data"), project.Folders.Root, &project.Folders.Data);
		suites.ItemSuite8->AEGP_CreateNewFolder(AEUtil::UTF16Cast(L"video"), project.Folders.Data, &project.Folders.Video);
		suites.ItemSuite8->AEGP_CreateNewFolder(AEUtil::UTF16Cast(L"audio"), project.Folders.Data, &project.Folders.Audio);
		suites.ItemSuite8->AEGP_CreateNewFolder(AEUtil::UTF16Cast(L"comp"), project.Folders.Data, &project.Folders.Comp);
	}

	void AetImporter::ImportAllFootage(const Aet::AetSet& set, const Aet::Scene& scene)
	{
		for (auto& video : scene.Videos)
			ImportVideo(*video);

		for (auto& audio : scene.Audios)
			ImportAudio(*audio);
	}

	void AetImporter::ImportAllCompositions(const Aet::AetSet& set, const Aet::Scene& scene)
	{
		{
			const A_Time duration = FrameToAETime(scene.EndFrame);
			const auto sceneName = FormatSceneName(set, scene);

			suites.CompSuite4->AEGP_CreateComp(project.Folders.Root, sceneName.c_str(), scene.Resolution.x, scene.Resolution.y, &AEUtil::OneToOneRatio, &duration, &workingScene.AE_FrameRate, &scene.RootComposition->GuiData.AE_Comp);
			suites.CompSuite4->AEGP_GetItemFromComp(scene.RootComposition->GuiData.AE_Comp, &scene.RootComposition->GuiData.AE_CompItem);
		}

		for (auto& comp : scene.Compositions)
		{
			const A_Time duration = FrameToAETime(GetCompDuration(*comp));
			suites.CompSuite4->AEGP_CreateComp(project.Folders.Comp, comp->GetName().data(), scene.Resolution.x, scene.Resolution.y, &AEUtil::OneToOneRatio, &duration, &workingScene.AE_FrameRate, &comp->GuiData.AE_Comp);
			suites.CompSuite4->AEGP_GetItemFromComp(comp->GuiData.AE_Comp, &comp->GuiData.AE_CompItem);
		}

		ImportLayersInComp(*scene.RootComposition);

		for (int i = static_cast<int>(scene.Compositions.size()) - 1; i >= 0; i--)
			ImportLayersInComp(*scene.Compositions[i]);
	}

	frame_t AetImporter::GetCompDuration(const Aet::Composition& comp) const
	{
		frame_t latestFrame = 1.0f;
		for (auto& layer : comp.GetLayers())
		{
			if (layer->EndFrame > latestFrame)
				latestFrame = layer->EndFrame;
		}
		return latestFrame;
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
		struct RGB8 { uint8_t R, G, B; };
		const RGB8 videoColor = *reinterpret_cast<const RGB8*>(&video.Color);

		constexpr float rgb8ToFloat = static_cast<float>(std::numeric_limits<uint8_t>::max());
		constexpr float aeAlpha = 1.0f;

		const AEGP_ColorVal aeColor = { aeAlpha, videoColor.R / rgb8ToFloat, videoColor.G / rgb8ToFloat, videoColor.B / rgb8ToFloat };

		char placeholderNameBuffer[AEGP_MAX_ITEM_NAME_SIZE];
		sprintf_s(placeholderNameBuffer, std::size(placeholderNameBuffer), "Placeholder (%dx%d)", video.Size.x, video.Size.y);

		suites.FootageSuite5->AEGP_NewSolidFootage(placeholderNameBuffer, video.Size.x, video.Size.y, &aeColor, &video.GuiData.AE_Footage);

		if (video.GuiData.AE_Footage != nullptr)
			suites.FootageSuite5->AEGP_AddFootageToProject(video.GuiData.AE_Footage, project.Folders.Video, &video.GuiData.AE_FootageItem);
	}

	void AetImporter::ImportSpriteVideo(const Aet::Video& video)
	{
		const auto frontSourceNameWithoutAetPrefix = StripPrefixIfExists(video.Sources.front().Name, workingAet.SpriteNamePrefixUnderscore);
		if (auto matchingSpriteFile = FindMatchingSpriteFile(frontSourceNameWithoutAetPrefix); matchingSpriteFile != nullptr)
		{
			AEGP_FootageLayerKey footageLayerKey = {};
			footageLayerKey.layer_idL = AEGP_LayerID_UNKNOWN;
			footageLayerKey.layer_indexL = 0;
			
			if (video.Sources.size() > 1)
				std::memcpy(footageLayerKey.nameAC, frontSourceNameWithoutAetPrefix.data(), frontSourceNameWithoutAetPrefix.size());

			AEGP_FileSequenceImportOptions sequenceImportOptions = {};
			sequenceImportOptions.all_in_folderB = (video.Sources.size() > 1);
			sequenceImportOptions.force_alphabeticalB = false;
			sequenceImportOptions.start_frameL = 0;
			sequenceImportOptions.end_frameL = static_cast<A_long>(video.Sources.size()) - 1;

			suites.FootageSuite5->AEGP_NewFootage(GlobalPluginID, AEUtil::UTF16Cast(matchingSpriteFile->FilePath.c_str()), &footageLayerKey, &sequenceImportOptions, FALSE, nullptr, &video.GuiData.AE_Footage);
		}
		else
		{
			static constexpr frame_t placeholderDuration = 270.0f;

			const A_Time duration = FrameToAETime(placeholderDuration);
			suites.FootageSuite5->AEGP_NewPlaceholderFootage(GlobalPluginID, frontSourceNameWithoutAetPrefix.data(), video.Size.x, video.Size.y, &duration, &video.GuiData.AE_Footage);
		}

		if (video.GuiData.AE_Footage != nullptr)
			suites.FootageSuite5->AEGP_AddFootageToProject(video.GuiData.AE_Footage, project.Folders.Video, &video.GuiData.AE_FootageItem);
	}

	void AetImporter::ImportAudio(const Aet::Audio& audio)
	{
		// TODO:
	}

	void AetImporter::ImportLayersInComp(const Aet::Composition& comp)
	{
		for (int i = static_cast<int>(comp.GetLayers().size()) - 1; i >= 0; i--)
			ImportLayer(comp, *comp.GetLayers()[i]);

		for (auto& layer : comp.GetLayers())
		{
			if (layer->GetRefParentLayer() != nullptr && layer->GuiData.AE_Layer != nullptr && layer->GetRefParentLayer()->GuiData.AE_Layer != nullptr)
				suites.LayerSuite8->AEGP_SetLayerParent(layer->GuiData.AE_Layer, layer->GetRefParentLayer()->GuiData.AE_Layer);
		}
	}

	void AetImporter::ImportLayer(const Aet::Composition& parentComp, const Aet::Layer& layer)
	{
		if (layer.ItemType == Aet::ItemType::Video)
		{
			if (layer.GetVideoItem() != nullptr && layer.GetVideoItem()->GuiData.AE_FootageItem != nullptr)
				suites.LayerSuite8->AEGP_AddLayer(layer.GetVideoItem()->GuiData.AE_FootageItem, parentComp.GuiData.AE_Comp, &layer.GuiData.AE_Layer);
		}
		else if (layer.ItemType == Aet::ItemType::Audio)
		{
			if (layer.GetAudioItem() != nullptr && layer.GetAudioItem()->GuiData.AE_FootageItem != nullptr)
				suites.LayerSuite8->AEGP_AddLayer(layer.GetAudioItem()->GuiData.AE_FootageItem, parentComp.GuiData.AE_Comp, &layer.GuiData.AE_Layer);
		}
		else if (layer.ItemType == Aet::ItemType::Composition)
		{
			if (layer.GetCompItem() != nullptr && layer.GetCompItem()->GuiData.AE_CompItem != nullptr)
				suites.LayerSuite8->AEGP_AddLayer(layer.GetCompItem()->GuiData.AE_CompItem, parentComp.GuiData.AE_Comp, &layer.GuiData.AE_Layer);
		}

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

	// TODO: Cleanup
	namespace
	{
		enum AetPropertyType
		{
			OriginX,
			OriginY,
			PositionX,
			PositionY,
			Rotation,
			ScaleX,
			ScaleY,
			Opacity
		};

		constexpr struct { AEGP_LayerStream Stream; AetPropertyType X, Y; float Factor; } StreamPropertiesRemapData[] =
		{
			{ AEGP_LayerStream_ANCHORPOINT, OriginX,	OriginY,	1.0f },
			{ AEGP_LayerStream_POSITION,	PositionX,	PositionY,	1.0f },
			{ AEGP_LayerStream_ROTATION,	Rotation,	Rotation,	1.0f },
			{ AEGP_LayerStream_SCALE,		ScaleX,		ScaleY,		100.0f },
			{ AEGP_LayerStream_OPACITY,		Opacity,	Opacity,	100.0f },
		};

		struct AetVec2KeyFrame
		{
			frame_t Frame;
			vec2 Value;
			float Curve;
		};

		template <class KeyFrameType>
		const KeyFrameType* FindKeyFrameAt(frame_t frame, const std::vector<KeyFrameType>& keyFrames)
		{
			for (const auto& keyFrame : keyFrames)
			{
				if (Aet::AetMgr::AreFramesTheSame(keyFrame.Frame, frame))
					return &keyFrame;
			}
			return nullptr;
		}

		std::vector<AetVec2KeyFrame> CombineXYKeyFrames(const Aet::Property1D& xKeyFrames, const Aet::Property1D& yKeyFrames)
		{
			std::vector<AetVec2KeyFrame> combinedKeyFrames;
			combinedKeyFrames.reserve(xKeyFrames->size() + yKeyFrames->size());

			for (auto& xKeyFrame : xKeyFrames.Keys)
			{
				vec2 value = { xKeyFrame.Value, 0.0f };

				auto matchingYKeyFrame = FindKeyFrameAt<Aet::KeyFrame>(xKeyFrame.Frame, yKeyFrames.Keys);
				if (matchingYKeyFrame != nullptr)
					value.y = matchingYKeyFrame->Value;
				else
					value.y = Aet::AetMgr::GetValueAt(yKeyFrames, xKeyFrame.Frame);

				combinedKeyFrames.push_back({ xKeyFrame.Frame, value, xKeyFrame.Curve });
			}

			for (auto& yKeyFrame : yKeyFrames.Keys)
			{
				auto existingKeyFrame = FindKeyFrameAt<AetVec2KeyFrame>(yKeyFrame.Frame, combinedKeyFrames);
				if (existingKeyFrame != nullptr)
					continue;

				const float xValue = Aet::AetMgr::GetValueAt(xKeyFrames, yKeyFrame.Frame);
				combinedKeyFrames.push_back({ yKeyFrame.Frame, vec2(xValue, yKeyFrame.Value), yKeyFrame.Curve });
			}

			std::sort(combinedKeyFrames.begin(), combinedKeyFrames.end(), [](const auto& a, const auto& b) { return a.Frame < b.Frame; });
			return combinedKeyFrames;
		}

		template <typename FrameToAETimeFunc>
		void ImportLayerVideoKeyFrames(AEGP_StreamSuite5* streamSuite5, AEGP_KeyframeSuite3* KeyframeSuite3, const Aet::Layer& layer, const Aet::LayerVideo& layerVideo, FrameToAETimeFunc frameToAETime)
		{
			for (const auto& value : StreamPropertiesRemapData)
			{
				AEGP_StreamValue2 streamValue2 = {};
				streamSuite5->AEGP_GetNewLayerStream(GlobalPluginID, layer.GuiData.AE_Layer, value.Stream, &streamValue2.streamH);

				const auto& xKeyFrames = layerVideo.Transform[value.X];
				const auto& yKeyFrames = layerVideo.Transform[value.Y];

				// NOTE: Set base value
				{
					streamValue2.val.two_d.x = xKeyFrames->size() > 0 ? xKeyFrames->front().Value * value.Factor : 0.0f;
					streamValue2.val.two_d.y = yKeyFrames->size() > 0 ? yKeyFrames->front().Value * value.Factor : 0.0f;

					streamSuite5->AEGP_SetStreamValue(GlobalPluginID, streamValue2.streamH, &streamValue2);
				}

#if 0
				// TODO: Only the position can be separated. WTF?
				A_Boolean separationLeader;
				suites.DynamicStreamSuite4()->AEGP_IsSeparationLeader(streamValue2.streamH, &separationLeader);
				if (xProperties != yProperties && separationLeader)
				{
					suites.DynamicStreamSuite4()->AEGP_SetDimensionsSeparated(streamValue2.streamH, true);
					continue;
					//suites.DynamicStreamSuite4()->AEGP_GetSeparationDimension();
				}
#endif
				auto addKeyFrames = [&](frame_t frame, float xValue, float yValue)
				{
					const A_Time time = frameToAETime(frame);
					AEGP_KeyframeIndex index;

					AEGP_StreamValue streamValue = {};
					streamValue.streamH = streamValue2.streamH;

					streamValue.val.two_d.x = xValue * value.Factor;
					streamValue.val.two_d.y = yValue * value.Factor;

					// TODO: Undo spam but AEGP_StartAddKeyframes doesn't have an insert function (?)
					KeyframeSuite3->AEGP_InsertKeyframe(streamValue.streamH, AEGP_LTimeMode_LayerTime, &time, &index);
					KeyframeSuite3->AEGP_SetKeyframeValue(streamValue.streamH, index, &streamValue);
				};

				if (&xKeyFrames == &yKeyFrames)
				{
					if (xKeyFrames->size() > 1)
					{
						for (auto& keyFrame : xKeyFrames.Keys)
							addKeyFrames(keyFrame.Frame, keyFrame.Value, 0.0f);
					}

#if 0
					if (yProperties->size() > 1 && xProperties != yProperties)
					{
						for (auto& keyFrame : *yProperties)
							addKeyFrames(keyFrame, true);
					}
#endif
				}
				else
				{
					// TODO: Combine keyframes ??
					auto combinedKeyFrames = CombineXYKeyFrames(xKeyFrames, yKeyFrames);

					if (combinedKeyFrames.size() > 1)
					{
						for (auto& keyFrame : combinedKeyFrames)
							addKeyFrames(keyFrame.Frame, keyFrame.Value.x, keyFrame.Value.y);
					}
				}
			}
		}
	}

	void AetImporter::ImportLayerVideo(const Aet::Layer& layer)
	{
		ImportLayerTransferMode(layer, layer.LayerVideo->TransferMode);
		
		// HACK:
		ImportLayerVideoKeyFrames(suites.StreamSuite5, suites.KeyframeSuite3, layer, *layer.LayerVideo, [this](frame_t f) { return FrameToAETime(f); });
	}

	void AetImporter::ImportLayerTransferMode(const Aet::Layer& layer, const Aet::LayerTransferMode& transferMode)
	{
		AEGP_LayerTransferMode layerTransferMode = {};
		layerTransferMode.mode = (static_cast<PF_TransferMode>(transferMode.BlendMode) - 1);
		layerTransferMode.flags = static_cast<AEGP_TransferFlags>(*reinterpret_cast<const uint8_t*>(&transferMode.Flags));
		layerTransferMode.track_matte = static_cast<AEGP_TrackMatte>(transferMode.TrackMatte);

		suites.LayerSuite8->AEGP_SetLayerTransferMode(layer.GuiData.AE_Layer, &layerTransferMode);
	}

	void AetImporter::ImportLayerAudio(const Aet::Layer& layer)
	{
		// TODO:
	}

	void AetImporter::ImportLayerTiming(const Aet::Layer& layer)
	{
		if (layer.StartOffset != 0.0f)
		{
			const A_Time startOffset = FrameToAETime(layer.StartOffset);
			const A_Time duration = FrameToAETime((layer.EndFrame - layer.StartFrame) / layer.TimeScale);
			suites.LayerSuite8->AEGP_SetLayerInPointAndDuration(layer.GuiData.AE_Layer, AEGP_LTimeMode_CompTime, &startOffset, &duration);

			const A_Time offsetAdjustedStartTime = FrameToAETime(-layer.StartOffset + layer.StartFrame);
			suites.LayerSuite8->AEGP_SetLayerOffset(layer.GuiData.AE_Layer, &offsetAdjustedStartTime);
		}
		else
		{
			const A_Time startTime = FrameToAETime(layer.StartFrame);
			suites.LayerSuite8->AEGP_SetLayerOffset(layer.GuiData.AE_Layer, &startTime);
		}

		// BUG: Is this working correctly now... (?)
		const A_Ratio stretch = { static_cast<A_long>(1.0f / layer.TimeScale * AEUtil::FixedPoint), static_cast<A_u_long>(AEUtil::FixedPoint) };
		suites.LayerSuite1->AEGP_SetLayerStretch(layer.GuiData.AE_Layer, &stretch);
	}

	void AetImporter::ImportLayerName(const Aet::Layer& layer)
	{
		suites.LayerSuite1->AEGP_SetLayerName(layer.GuiData.AE_Layer, layer.GetName().c_str());
	}

	void AetImporter::ImportLayerFlags(const Aet::Layer& layer)
	{
		const uint16_t layerFlags = *(const uint16_t*)(&layer.Flags);
		for (size_t flagsBitIndex = 0; flagsBitIndex < sizeof(layerFlags) * CHAR_BIT; flagsBitIndex++)
		{
			const uint16_t flagsBitMask = (1 << flagsBitIndex);
			suites.LayerSuite1->AEGP_SetLayerFlag(layer.GuiData.AE_Layer, static_cast<AEGP_LayerFlags>(flagsBitMask), static_cast<A_Boolean>(layerFlags& flagsBitMask));
		}

		// NOTE: Makes sure underlying transfer modes etc are being preserved
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
		suites.StreamSuite5->AEGP_GetNewLayerStream(GlobalPluginID, layer.GuiData.AE_Layer, AEGP_LayerStream_MARKER, &streamValue2.streamH);

		for (auto& marker : layer.Markers)
		{
			AEGP_MarkerVal markerValue = {};
			AEGP_MarkerVal* markerValuePtr = &markerValue;
			std::memcpy(markerValue.nameAC, marker->Name.data(), marker->Name.size());

			AEGP_StreamValue streamValue = {};
			streamValue.streamH = streamValue2.streamH;
			streamValue.val.markerH = &markerValuePtr;

			const A_Time time = FrameToAETime(marker->Frame);
			AEGP_KeyframeIndex index;
			suites.KeyframeSuite3->AEGP_InsertKeyframe(streamValue.streamH, AEGP_LTimeMode_LayerTime, &time, &index);
			suites.KeyframeSuite3->AEGP_SetKeyframeValue(streamValue.streamH, index, &streamValue);
		}
	}
}
