#include "AetExport.h"
#include "FormatUtil.h"
#include "StreamUtil.h"
#include "Graphics/Auth2D/Aet/AetMgr.h"
#include "Misc/StringHelper.h"
#include "Resource/IDHash.h"

#define LogLine(format, ...)		do { if (logLevel != LogLevel_None)		{ fprintf(logStream, "%s(): "			format "\n", __FUNCTION__, __VA_ARGS__); } } while (false)
#define LogInfoLine(format, ...)	do { if (logLevel & LogLevel_Info)		{ fprintf(logStream, "[INFO] %s(): "	format "\n", __FUNCTION__, __VA_ARGS__); } } while (false)
#define LogWarningLine(format, ...)	do { if (logLevel & LogLevel_Warning)	{ fprintf(logStream, "[WARNING] %s(): "	format "\n", __FUNCTION__, __VA_ARGS__); } } while (false)
#define LogErrorLine(format, ...)	do { if (logLevel & LogLevel_Error)		{ fprintf(logStream, "[ERROR] %s(): "	format "\n", __FUNCTION__, __VA_ARGS__); } } while (false)

namespace AetPlugin
{
	void AetExporter::SetLog(FILE* logStream, LogLevel logLevel)
	{
		this->logStream = logStream;
		this->logLevel = (logStream != nullptr) ? logLevel : LogLevel_None;
	}

	std::string AetExporter::GetAetSetNameFromProjectName() const
	{
		AEGP_ProjectH projectHandle;
		suites.ProjSuite5->AEGP_GetProjectByIndex(0, &projectHandle);
		A_char projectName[AEGP_MAX_PROJ_NAME_SIZE];
		suites.ProjSuite5->AEGP_GetProjectName(projectHandle, projectName);

		std::string setName;
		setName += AetPrefix;
		setName += FormatUtil::ToSnakeCaseLower(FormatUtil::StripFileExtension(projectName));
		return setName;
	}

	UniquePtr<Aet::AetSet> AetExporter::ExportAetSet(std::wstring_view workingDirectory)
	{
		LogLine("--- Log Start ---");
		LogLine("Log Level: '%s'", FormatUtil::FormatFlags(logLevel, LogLevelsToCheck).c_str());

		LogInfoLine("Working Directory: '%s'", Utf16ToUtf8(workingDirectory).c_str());
		this->workingDirectory.ImportDirectory = workingDirectory;

		auto set = MakeUnique<Aet::AetSet>();

		SetupWorkingProjectData();
		SetupWorkingSetData(*set);

		for (auto* sceneComp : workingSet.SceneComps)
		{
			SetupWorkingSceneData(sceneComp);
			ExportScene();
		}

		LogLine("--- Log End ---");
		return set;
	}

	void AetExporter::SetupWorkingProjectData()
	{
		A_long projectCount;
		suites.ProjSuite5->AEGP_GetNumProjects(&projectCount);

		workingProject.Index = 0;
		suites.ProjSuite5->AEGP_GetProjectByIndex(workingProject.Index, &workingProject.Handle);

		suites.ProjSuite5->AEGP_GetProjectName(workingProject.Handle, workingProject.Name);
		LogInfoLine("Project Name: '%s'", workingProject.Name);

		AEGP_MemHandle pathHandle;
		suites.ProjSuite5->AEGP_GetProjectPath(workingProject.Handle, &pathHandle);
		workingProject.Path = AEUtil::MoveFreeUTF16String(suites.MemorySuite1, pathHandle);
		LogInfoLine("Project Path: '%s'", Utf16ToUtf8(workingProject.Path).c_str());

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

			AEGP_MemHandle nameHandle;
			suites.ItemSuite8->AEGP_GetItemName(EvilGlobalState.PluginID, item.ItemHandle, &nameHandle);
			item.Name = Utf16ToUtf8(AEUtil::MoveFreeUTF16String(suites.MemorySuite1, nameHandle));

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
			LogErrorLine("No suitable AetSet folder found");
		}
		else
		{
			LogErrorLine("Suitable AetSet folder found: '%s'", foundSetFolder->Name.c_str());
			set.Name = foundSetFolder->CommentProperty.Value;
			workingSet.Folder = &(*foundSetFolder);

			workingSet.SceneComps.reserve(2);
			for (auto& item : workingProject.Items)
			{
				if (item.Type == AEGP_ItemType_COMP && item.CommentProperty.Key == CommentUtil::Keys::Scene && item.IsParentOf(*foundSetFolder))
				{
					workingSet.SceneComps.push_back(&item);
					LogInfoLine("Suitable scene composition found: '%s'", item.Name.c_str());
				}
			}

			std::sort(workingSet.SceneComps.begin(), workingSet.SceneComps.end(), [&](const auto& sceneA, const auto& sceneB)
			{
				return sceneA->CommentProperty.KeyIndex.value_or(0) < sceneB->CommentProperty.KeyIndex.value_or(0);
			});
		}

		workingSet.SprPrefix = FormatUtil::ToUpper(FormatUtil::StripPrefixIfExists(workingSet.Set->Name, AetPrefix)) + "_";
		workingSet.SprHashPrefix = FormatUtil::ToUpper(SprPrefix) + workingSet.SprPrefix;
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

		scene.Videos;
		scene.Audios;

		ExportAllCompositions();
		FixInvalidSceneData();
	}

	void AetExporter::ExportAllCompositions()
	{
		workingScene.Scene->RootComposition = MakeRef<Aet::Composition>();
		workingScene.Scene->RootComposition->GuiData.AE_Comp = workingScene.AESceneComp->CompHandle;
		workingScene.Scene->RootComposition->GuiData.AE_CompItem = workingScene.AESceneComp->ItemHandle;
		ExportComp(*workingScene.Scene->RootComposition);

		// NOTE: Reverse once at the end because pushing to the end of the vector while creating the comps is a lot more efficient
		std::reverse(workingScene.Scene->Compositions.begin(), workingScene.Scene->Compositions.end());
		// std::reverse(workingScene.Scene->Videos.begin(), workingScene.Scene->Videos.end());

		for (auto& layer : workingScene.Scene->RootComposition->GetLayers())
			ScanCheckSetLayerRefParents(*layer);
	}

	void AetExporter::ExportComp(Aet::Composition& comp)
	{
		A_long compLayerCount;
		suites.LayerSuite3->AEGP_GetCompNumLayers(comp.GuiData.AE_Comp, &compLayerCount);

		auto& compLayers = comp.GetLayers();
		compLayers.reserve(compLayerCount);

		for (A_long i = 0; i < compLayerCount; i++)
		{
			auto& layer = *compLayers.emplace_back(MakeRef<Aet::Layer>());
			suites.LayerSuite3->AEGP_GetCompLayerByIndex(comp.GuiData.AE_Comp, i, &layer.GuiData.AE_Layer);

			ExportLayer(layer);
		}
	}

	void AetExporter::ExportLayer(Aet::Layer& layer)
	{
		ExportLayerName(layer);
		ExportLayerTime(layer);
		ExportLayerQuality(layer);
		ExportLayerMarkers(layer);
		ExportLayerFlags(layer);
		ExportLayerSourceItem(layer);

		if (layer.ItemType == Aet::ItemType::Video || layer.ItemType == Aet::ItemType::Composition)
			ExportLayerVideo(layer);
	}

	void AetExporter::ExportLayerName(Aet::Layer& layer)
	{
		A_char layerName[AEGP_MAX_LAYER_NAME_SIZE], layerSourceName[AEGP_MAX_LAYER_NAME_SIZE];
		suites.LayerSuite3->AEGP_GetLayerName(layer.GuiData.AE_Layer, layerName, layerSourceName);

		if (layerName[0] != '\0')
			layer.SetName(layerName);
		else
			layer.SetName(layerSourceName);
	}

	void AetExporter::ExportLayerTime(Aet::Layer& layer)
	{
		A_Time offset, inPoint, duration;
		A_Ratio stretch;
		suites.LayerSuite3->AEGP_GetLayerOffset(layer.GuiData.AE_Layer, &offset);
		suites.LayerSuite3->AEGP_GetLayerInPoint(layer.GuiData.AE_Layer, AEGP_LTimeMode_LayerTime, &inPoint);
		suites.LayerSuite3->AEGP_GetLayerDuration(layer.GuiData.AE_Layer, AEGP_LTimeMode_LayerTime, &duration);
		suites.LayerSuite3->AEGP_GetLayerStretch(layer.GuiData.AE_Layer, &stretch);

		const frame_t frameOffset = AEUtil::AETimeToFrame(offset, workingScene.Scene->FrameRate);
		const frame_t frameInPoint = AEUtil::AETimeToFrame(inPoint, workingScene.Scene->FrameRate);
		const frame_t frameDuration = AEUtil::AETimeToFrame(duration, workingScene.Scene->FrameRate);
		const float floatStretch = AEUtil::Ratio(stretch);

		// TODO: Should these be directly affected by the stretch or is it already accounted for (?)
		layer.StartFrame = (frameOffset + frameInPoint);
		layer.EndFrame = (layer.StartFrame + frameDuration);
		layer.StartOffset = (frameInPoint);
		layer.TimeScale = (1.0f / floatStretch);
	}

	void AetExporter::ExportLayerQuality(Aet::Layer& layer)
	{
		AEGP_LayerQuality layerQuality;
		suites.LayerSuite3->AEGP_GetLayerQuality(layer.GuiData.AE_Layer, &layerQuality);
		layer.Quality = (static_cast<Aet::LayerQuality>(layerQuality + 1));
	}

	void AetExporter::ExportLayerMarkers(Aet::Layer& layer)
	{
		AEGP_StreamRefH streamRef;
		suites.StreamSuite4->AEGP_GetNewLayerStream(EvilGlobalState.PluginID, layer.GuiData.AE_Layer, AEGP_LayerStream_MARKER, &streamRef);

		A_long keyFrameCount;
		suites.KeyframeSuite3->AEGP_GetStreamNumKFs(streamRef, &keyFrameCount);

		if (keyFrameCount < 1)
			return;

		layer.Markers.reserve(keyFrameCount);
		for (AEGP_KeyframeIndex i = 0; i < keyFrameCount; i++)
		{
			A_Time time;
			suites.KeyframeSuite3->AEGP_GetKeyframeTime(streamRef, i, AEGP_LTimeMode_CompTime, &time);
			AEGP_StreamValue streamVal;
			suites.KeyframeSuite3->AEGP_GetNewKeyframeValue(EvilGlobalState.PluginID, streamRef, i, &streamVal);

			// TODO: Add the layer start frame (?)
			const frame_t frameTime = AEUtil::AETimeToFrame(time, workingScene.Scene->FrameRate);
			layer.Markers.push_back(MakeRef<Aet::Marker>(frameTime, streamVal.val.markerH[0]->nameAC));
		}
	}

	void AetExporter::ExportLayerFlags(Aet::Layer& layer)
	{
		AEGP_LayerFlags layerFlags;
		suites.LayerSuite3->AEGP_GetLayerFlags(layer.GuiData.AE_Layer, &layerFlags);

		if (layerFlags & AEGP_LayerFlag_VIDEO_ACTIVE) layer.Flags.VideoActive = true;
		if (layerFlags & AEGP_LayerFlag_AUDIO_ACTIVE) layer.Flags.AudioActive = true;
		if (layerFlags & AEGP_LayerFlag_LOCKED) layer.Flags.Locked = true;
		if (layerFlags & AEGP_LayerFlag_SHY) layer.Flags.Shy = true;
	}

	void AetExporter::ExportLayerSourceItem(Aet::Layer& layer)
	{
		AEGP_ItemH sourceItem;
		suites.LayerSuite3->AEGP_GetLayerSourceItem(layer.GuiData.AE_Layer, &sourceItem);

		AEGP_ItemType sourceItemType;
		suites.ItemSuite8->AEGP_GetItemType(sourceItem, &sourceItemType);

		if (sourceItemType == AEGP_ItemType_COMP)
		{
			layer.ItemType = Aet::ItemType::Composition;

			if (auto existingCompItem = FindExistingCompSourceItem(sourceItem); existingCompItem != nullptr)
				layer.SetItem(existingCompItem);
			else
				ExportNewCompSource(layer, sourceItem);
		}
		else if (sourceItemType == AEGP_ItemType_FOOTAGE)
		{
			layer.ItemType = Aet::ItemType::Video; // Aet::ItemType::Audio

			if (auto existingVideoItem = FindExistingVideoSourceItem(sourceItem); existingVideoItem != nullptr)
				layer.SetItem(existingVideoItem);
			else
				ExportNewVideoSource(layer, sourceItem);
		}
	}

	void AetExporter::ExportLayerVideo(Aet::Layer& layer)
	{
		layer.LayerVideo = MakeRef<Aet::LayerVideo>();
		ExportLayerTransferMode(layer, layer.LayerVideo->TransferMode);
		ExportLayerVideoStream(layer, *layer.LayerVideo);
	}

	void AetExporter::ExportLayerTransferMode(Aet::Layer& layer, Aet::LayerTransferMode& transferMode)
	{
		AEGP_LayerTransferMode layerTransferMode;
		suites.LayerSuite3->AEGP_GetLayerTransferMode(layer.GuiData.AE_Layer, &layerTransferMode);

		transferMode.BlendMode = static_cast<AetBlendMode>(layerTransferMode.mode + 1);
		transferMode.TrackMatte = static_cast<Aet::TrackMatte>(layerTransferMode.track_matte);

		if (transferMode.BlendMode != AetBlendMode::Normal && transferMode.BlendMode != AetBlendMode::Add && transferMode.BlendMode != AetBlendMode::Multiply && transferMode.BlendMode != AetBlendMode::Screen)
			LogWarningLine("Unsupported blend mode used by layer '%s'. Only 'Normal', 'Add', 'Multiplty' and 'Sceen' are supported", layer.GetName().c_str());

		if (transferMode.TrackMatte != Aet::TrackMatte::NoTrackMatte && transferMode.TrackMatte != Aet::TrackMatte::Alpha)
			LogWarningLine("Unsupported track matte used by layer '%s'. Only 'Track Matte Alpha' is supported", layer.GetName().c_str());

		if (layerTransferMode.flags & AEGP_TransferFlag_PRESERVE_ALPHA)
			transferMode.Flags.PreserveAlpha = true;
		if (layerTransferMode.flags & AEGP_TransferFlag_RANDOMIZE_DISSOLVE)
			transferMode.Flags.RandomizeDissolve = true;

		if (transferMode.Flags.PreserveAlpha || transferMode.Flags.RandomizeDissolve)
			LogWarningLine("Unsupported transfer mode flags used by layer '%s'. 'Preserve Alpha' nor 'Randomize Dissolve' are supported", layer.GetName().c_str());
	}

	void AetExporter::ExportLayerVideoStream(Aet::Layer& layer, Aet::LayerVideo& layerVideo)
	{
		const auto zeroTime = AEUtil::FrameToAETime(0.0f, workingScene.Scene->FrameRate);

		for (const auto& aeToAetStream : StreamUtil::Transform2DRemapData)
		{
			AEGP_StreamRefH streamRef;
			suites.StreamSuite4->AEGP_GetNewLayerStream(EvilGlobalState.PluginID, layer.GuiData.AE_Layer, aeToAetStream.StreamType, &streamRef);

			A_long keyFrameCount;
			suites.KeyframeSuite3->AEGP_GetStreamNumKFs(streamRef, &keyFrameCount);

			if (keyFrameCount < 1)
			{
				AEGP_StreamVal2 streamVal2;
				AEGP_StreamType streamType;
				suites.StreamSuite4->AEGP_GetLayerStreamValue(layer.GuiData.AE_Layer, aeToAetStream.StreamType, AEGP_LTimeMode_LayerTime, &zeroTime, false, &streamVal2, &streamType);

				layerVideo.Transform[aeToAetStream.FieldX]->emplace_back(static_cast<float>(streamVal2.one_d / aeToAetStream.ScaleFactor));
				if (aeToAetStream.FieldX != aeToAetStream.FieldY)
					layerVideo.Transform[aeToAetStream.FieldY]->emplace_back(static_cast<float>(streamVal2.two_d.y / aeToAetStream.ScaleFactor));
			}
			else
			{
				for (AEGP_KeyframeIndex i = 0; i < keyFrameCount; i++)
				{
					A_Time time;
					suites.KeyframeSuite3->AEGP_GetKeyframeTime(streamRef, i, AEGP_LTimeMode_LayerTime, &time);
					AEGP_StreamValue streamVal;
					suites.KeyframeSuite3->AEGP_GetNewKeyframeValue(EvilGlobalState.PluginID, streamRef, i, &streamVal);

					// TODO: Interpolation (including hold frames)
					const frame_t frameTime = AEUtil::AETimeToFrame(time, workingScene.Scene->FrameRate) + layer.StartFrame;

					layerVideo.Transform[aeToAetStream.FieldX]->emplace_back(frameTime, static_cast<float>(streamVal.val.one_d / aeToAetStream.ScaleFactor));
					if (aeToAetStream.FieldX != aeToAetStream.FieldY)
						layerVideo.Transform[aeToAetStream.FieldY]->emplace_back(frameTime, static_cast<float>(streamVal.val.two_d.y / aeToAetStream.ScaleFactor));
				}
			}
		}

		for (Transform2DField i = 0; i < Transform2DField_Count; i++)
			SetLayerVideoPropertyLinear(layerVideo.Transform[i]);
	}

	void AetExporter::SetLayerVideoPropertyLinear(Aet::Property1D& property)
	{
		if (property->size() < 2)
			return;

		for (size_t i = 0; i < property->size() - 1; i++)
		{
			auto& startKeyFrame = property.Keys[i];
			auto& endKeyFrame = property.Keys[i + 1];

			// HACK: Yeah this definitely needs a lot more work
			const float linearTangent = (endKeyFrame.Value - startKeyFrame.Value) / (endKeyFrame.Frame - startKeyFrame.Frame);

			startKeyFrame.Curve = linearTangent;
			endKeyFrame.Curve = linearTangent;
		}
	}

	void AetExporter::ExportNewCompSource(Aet::Layer& layer, AEGP_ItemH sourceItem)
	{
		auto& newCompItem = workingScene.Scene->Compositions.emplace_back(MakeRef<Aet::Composition>());
		newCompItem->GuiData.AE_CompItem = sourceItem;
		suites.CompSuite7->AEGP_GetCompFromItem(newCompItem->GuiData.AE_CompItem, &newCompItem->GuiData.AE_Comp);

		layer.SetItem(newCompItem);
		ExportComp(*newCompItem);
	}

	void AetExporter::ExportNewVideoSource(Aet::Layer& layer, AEGP_ItemH sourceItem)
	{
		auto& newVideoItem = workingScene.Scene->Videos.emplace_back(MakeRef<Aet::Video>());
		newVideoItem->GuiData.AE_FootageItem = sourceItem;
		suites.FootageSuite5->AEGP_GetMainFootageFromItem(newVideoItem->GuiData.AE_FootageItem, &newVideoItem->GuiData.AE_Footage);

		layer.SetItem(newVideoItem);
		ExportVideo(*newVideoItem);
	}

	void AetExporter::ExportVideo(Aet::Video& video)
	{
		auto foundItem = std::find_if(workingProject.Items.begin(), workingProject.Items.end(), [&](const auto& item) { return item.ItemHandle == video.GuiData.AE_FootageItem; });
		if (foundItem == workingProject.Items.end())
			return;

		auto& item = *foundItem;
		const auto cleanItemName = FormatUtil::ToUpper(FormatUtil::StripPrefixIfExists(FormatUtil::StripPrefixIfExists(FormatUtil::StripFileExtension(item.Name), SprPrefix), workingSet.SprPrefix));

		suites.FootageSuite5->AEGP_GetMainFootageFromItem(item.ItemHandle, &video.GuiData.AE_Footage);

		AEGP_FootageSignature signature;
		suites.FootageSuite5->AEGP_GetFootageSignature(video.GuiData.AE_Footage, &signature);
		video.Size = { item.Dimensions.first, item.Dimensions.second };

		if (signature == AEGP_FootageSignature_SOLID)
		{
			AEGP_ColorVal footageColor;
			suites.FootageSuite5->AEGP_GetSolidFootageColor(item.ItemHandle, false, &footageColor);
			video.Color = AEUtil::ColorRGB8(footageColor);
		}
		else
		{
			video.Color = 0xFFFFFF00;

			auto& source = video.Sources.emplace_back();
			source.Name = workingSet.SprPrefix + cleanItemName; // NOTE: {SET_NAME}_{SPRITE_NAME}
			source.ID = HashIDString<SprID>(workingSet.SprHashPrefix + cleanItemName); // NOTE: SPR_{SET_NAME}_{SPRITE_NAME}

			if (item.CommentProperty.Key == CommentUtil::Keys::SprID)
			{
				std::string_view sprIDComment = FormatUtil::StripPrefixIfExists(item.CommentProperty.Value, "0x");

				uint32_t sprID;
				auto result = std::from_chars(sprIDComment.data(), sprIDComment.data() + sprIDComment.size(), sprID, 0x10);
				if (result.ec != std::errc::invalid_argument)
					source.ID = static_cast<SprID>(sprID);
			}
		}

		video.Frames = static_cast<float>(video.Sources.size());
	}

	RefPtr<Aet::Composition> AetExporter::FindExistingCompSourceItem(AEGP_ItemH sourceItem)
	{
		// NOTE: No point in searching the root comp since it should never be referenced by any other child comp (?)
		for (const auto& comp : workingScene.Scene->Compositions)
		{
			if (comp->GuiData.AE_CompItem == sourceItem)
				return comp;
		}
		return nullptr;
	}

	RefPtr<Aet::Video> AetExporter::FindExistingVideoSourceItem(AEGP_ItemH sourceItem)
	{
		for (const auto& video : workingScene.Scene->Videos)
		{
			if (video->GuiData.AE_FootageItem == sourceItem)
				return video;
		}
		return nullptr;
	}

	void AetExporter::ScanCheckSetLayerRefParents(Aet::Layer& layer)
	{
		if (layer.ItemType == Aet::ItemType::Composition && layer.GetCompItem() != nullptr)
		{
			for (auto& layer : layer.GetCompItem()->GetLayers())
				ScanCheckSetLayerRefParents(*layer);
		}

		AEGP_LayerH parentHandle;
		suites.LayerSuite3->AEGP_GetLayerParent(layer.GuiData.AE_Layer, &parentHandle);

		if (parentHandle != nullptr)
			layer.SetRefParentLayer(FindLayerRefParent(layer, parentHandle));
	}

	RefPtr<Aet::Layer> AetExporter::FindLayerRefParent(Aet::Layer& layer, AEGP_LayerH parentHandle)
	{
		for (auto& comp : workingScene.Scene->Compositions)
		{
			for (auto& layer : comp->GetLayers())
			{
				if (layer->GuiData.AE_Layer == parentHandle)
					return layer;
			}
		}

		return nullptr;
	}

	void AetExporter::FixInvalidSceneData()
	{
		FixInvalidCompositionData(*workingScene.Scene->RootComposition);
		for (auto& comp : workingScene.Scene->Compositions)
			FixInvalidCompositionData(*comp);
	}

	void AetExporter::FixInvalidCompositionData(Aet::Composition& comp)
	{
		// TODO: Eventually automatically expand composition track mattes into video layers (?)
		FixInvalidCompositionTrackMatteDurations(comp);
	}

	void AetExporter::FixInvalidCompositionTrackMatteDurations(Aet::Composition& comp)
	{
		if (comp.GetLayers().size() < 2)
			return;

		for (size_t i = 0; i < comp.GetLayers().size() - 1; i++)
		{
			auto& layer = comp.GetLayers()[i + 0];
			auto& trackMatteLayer = comp.GetLayers()[i + 1];

			if (trackMatteLayer->LayerVideo != nullptr && trackMatteLayer->LayerVideo->TransferMode.TrackMatte != Aet::TrackMatte::NoTrackMatte)
				FixInvalidLayerTrackMatteDurations(*layer, *trackMatteLayer);
		}
	}

	void AetExporter::FixInvalidLayerTrackMatteDurations(Aet::Layer& layer, Aet::Layer& trackMatteLayer)
	{
		// TODO: What about start frame / start offset mismatches (?)
		if (layer.EndFrame == trackMatteLayer.EndFrame)
			return;

		LogWarningLine("Fixing: '%s'", layer.GetName().c_str());

		const frame_t adjustedEndFrame = std::min(layer.EndFrame, trackMatteLayer.EndFrame);
		layer.EndFrame = trackMatteLayer.EndFrame = adjustedEndFrame;
	}
}
