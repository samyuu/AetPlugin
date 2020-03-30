#include "AetImport.h"
#include "Graphics/Auth2D/Aet/AetMgr.h"
#include "Misc/StringHelper.h"
#include "FileSystem/FileHelper.h"
#include <filesystem>

using namespace Comfy;
using namespace Comfy::Graphics;

namespace
{
	static std::wstring WorkingImportDirectory;

	constexpr A_Ratio OneToOneRatio = { 1, 1 };
	constexpr float FixedPoint = 10000.0f;

	float GlobalFrameRate = 0.0f;

	const A_UTF16Char* UTF16(const wchar_t* value)
	{
		return reinterpret_cast<const A_UTF16Char*>(value);
	}

	frame_t GetCompDuration(Aet::Composition& comp)
	{
		frame_t latestFrame = 1.0f;
		for (auto& layer : comp.GetLayers())
		{
			if (layer->EndFrame > latestFrame)
				latestFrame = layer->EndFrame;
		}
		return latestFrame;
	}

	A_Time FrameToATime(frame_t frame)
	{
		return { static_cast<A_long>(frame * FixedPoint), static_cast<A_u_long>(GlobalFrameRate * FixedPoint) };
	}
}

namespace
{
	bool iequals(const std::string_view a, const std::string_view b)
	{
		return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](char a, char b) { return tolower(a) == tolower(b); });
	}

	void ImportVideo(AEGP_SuiteHandler& suites, Aet::Video& video, AEGP_ItemH folder)
	{
		auto getVideoName = [](Aet::Video& video)
		{
			if (video.Sources.size() == 1)
				return video.Sources.front().Name;

			if (video.Sources.size() > 1)
			{
				// TODO:
				return video.Sources.front().Name;
			}

			std::string nameBuffer = std::string(AEGP_MAX_ITEM_NAME_SIZE, '\0');
			sprintf_s(nameBuffer.data(), nameBuffer.size(), "Placeholder (%dx%d)", video.Size.x, video.Size.y);

			return nameBuffer;
		};

		const auto name = getVideoName(video);

		if (true && video.Sources.size() > 0)
		{
			// TEMP: L"D:\\PS4\\CUSA08026 [DX] output\\rom_ps4_fix\\rom\\2d\\spr_mml\\spr_gam_cmn"
			for (const auto& p : std::filesystem::directory_iterator(WorkingImportDirectory))
			{
				const auto path = p.path();
				const auto fileName = path.filename().string();

				if (fileName.size() < 9 || !fileName._Starts_with("spr_"))
					continue;

				const auto fileNameWihoutSpr = fileName.substr(4, fileName.size() - 8);

				if (fileNameWihoutSpr.size() != name.size())
					continue;

				if (iequals(name, fileNameWihoutSpr))
				{
					AEGP_FootageLayerKey footageLayerKey = {};
					footageLayerKey.layer_idL = AEGP_LayerID_UNKNOWN;
					footageLayerKey.layer_indexL = 0;

					const auto widePath = path.wstring();
					suites.FootageSuite5()->AEGP_NewFootage(GlobalPluginID, UTF16(widePath.c_str()), &footageLayerKey, nullptr, FALSE, nullptr, &video.GuiData.AE_Footage);
					suites.FootageSuite5()->AEGP_AddFootageToProject(video.GuiData.AE_Footage, folder, &video.GuiData.AE_FootageItem);
					return;
				}
			}
		}

		// TODO: video->Frames
		const A_Time duration = { 1, 1 };

		// suites.FootageSuite5()->AEGP_NewPlaceholderFootage(GlobalPluginID, name.c_str(), video->Size.x, video->Size.y, &duration, &video->GuiData.AE_Footage);
		const AEGP_ColorVal color = { 1.0f, 0.04f, 0.35f, 0.82f /*video->Color*/ };
		suites.FootageSuite5()->AEGP_NewSolidFootage(name.c_str(), video.Size.x, video.Size.y, &color, &video.GuiData.AE_Footage);

		suites.FootageSuite5()->AEGP_AddFootageToProject(video.GuiData.AE_Footage, folder, &video.GuiData.AE_FootageItem);
	}

	void ImportBlendMode(AEGP_SuiteHandler& suites, Aet::Layer* layer)
	{
		auto getTransferModeFromBlendMode = [](Graphics::AetBlendMode blendMode)
		{
			return static_cast<PF_TransferMode>(blendMode) - 1;
		};

		if (layer->ItemType != Aet::ItemType::Video)
			return;

		AEGP_LayerTransferMode layerTransferMode = {};
		layerTransferMode.mode = getTransferModeFromBlendMode(layer->LayerVideo->TransferMode.BlendMode);

		suites.LayerSuite6()->AEGP_SetLayerTransferMode(layer->GuiData.AE_Layer, &layerTransferMode);
	}

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

	void ImportKeyFrameProperties(AEGP_SuiteHandler& suites, Aet::Layer* layer)
	{
		for (const auto& value : StreamPropertiesRemapData)
		{
			AEGP_StreamValue2 streamValue2 = {};
			suites.StreamSuite5()->AEGP_GetNewLayerStream(GlobalPluginID, layer->GuiData.AE_Layer, value.Stream, &streamValue2.streamH);

			const auto& xKeyFrames = layer->LayerVideo->Transform[value.X];
			const auto& yKeyFrames = layer->LayerVideo->Transform[value.Y];

			// NOTE: Set base value
			{
				streamValue2.val.two_d.x = xKeyFrames->size() > 0 ? xKeyFrames->front().Value * value.Factor : 0.0f;
				streamValue2.val.two_d.y = yKeyFrames->size() > 0 ? yKeyFrames->front().Value * value.Factor : 0.0f;

				suites.StreamSuite5()->AEGP_SetStreamValue(GlobalPluginID, streamValue2.streamH, &streamValue2);
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
				const A_Time time = FrameToATime(frame);
				AEGP_KeyframeIndex index;

				AEGP_StreamValue streamValue = {};
				streamValue.streamH = streamValue2.streamH;

				streamValue.val.two_d.x = xValue * value.Factor;
				streamValue.val.two_d.y = yValue * value.Factor;

				// TODO: Undo spam but AEGP_StartAddKeyframes doesn't have an insert function (?)
				suites.KeyframeSuite3()->AEGP_InsertKeyframe(streamValue.streamH, AEGP_LTimeMode_LayerTime, &time, &index);
				suites.KeyframeSuite3()->AEGP_SetKeyframeValue(streamValue.streamH, index, &streamValue);
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

	void ImportAnimationData(AEGP_SuiteHandler& suites, Aet::Layer* layer)
	{
		if (layer->LayerVideo == nullptr)
			return;

		ImportBlendMode(suites, layer);
		ImportKeyFrameProperties(suites, layer);
	}

	void ImportLayer(AEGP_SuiteHandler& suites, Aet::Composition* parentComp, Aet::Layer* layer)
	{
		if (layer->ItemType == Aet::ItemType::Video)
		{
			if (layer->GetVideoItem() == nullptr)
				return;

			suites.LayerSuite1()->AEGP_AddLayer(layer->GetVideoItem()->GuiData.AE_FootageItem, parentComp->GuiData.AE_Comp, &layer->GuiData.AE_Layer);
		}
		else if (layer->ItemType == Aet::ItemType::Audio)
		{
			if (layer->GetAudioItem() == nullptr)
				return;

			// TODO:
			// suites.LayerSuite1()->AEGP_AddLayer(layer->GetReferencedSoundEffect()->GuiData.AE_FootageItem, parentComp->GuiData.AE_Comp, &layer->GuiData.AE_Layer);
		}
		else if (layer->ItemType == Aet::ItemType::Composition)
		{
			if (layer->GetCompItem() == nullptr)
				return;

			suites.LayerSuite1()->AEGP_AddLayer(layer->GetCompItem()->GuiData.AE_CompItem, parentComp->GuiData.AE_Comp, &layer->GuiData.AE_Layer);
		}

		if (layer->LayerVideo != nullptr)
			ImportAnimationData(suites, layer);

		if (layer->GuiData.AE_Layer != nullptr)
		{
			const A_Time startTime = FrameToATime(layer->StartFrame);
			const A_Time duration = FrameToATime(layer->EndFrame - layer->StartFrame);

			// TODO: WTF IS THIS DOGSHIT
			if (layer->StartOffset != 0.0f)
			{
				auto result = suites.LayerSuite1()->AEGP_SetLayerFlag(layer->GuiData.AE_Layer, AEGP_LayerFlag_TIME_REMAPPING, true);
				
				AEGP_StreamValue2 remapStreamValue2 = {};
				result = suites.StreamSuite5()->AEGP_GetNewLayerStream(GlobalPluginID, layer->GuiData.AE_Layer, AEGP_LayerStream_TIME_REMAP, &remapStreamValue2.streamH);
				remapStreamValue2.val.one_d = (1.0f / GlobalFrameRate) * layer->StartOffset;

				result = suites.StreamSuite5()->AEGP_SetStreamValue(GlobalPluginID, remapStreamValue2.streamH, &remapStreamValue2);

				// TODO: THIS NEEDS AT LEAST TWO KEY FRAMES?????
				if (true)
				{
					AEGP_StreamValue streamValue = {};
					streamValue.streamH = remapStreamValue2.streamH;
					streamValue.val.one_d = (1.0f / GlobalFrameRate) * layer->StartOffset;

					//const A_Time time = FrameToATime(0.0f); AEGP_KeyframeIndex index;
					//suites.KeyframeSuite3()->AEGP_InsertKeyframe(streamValue.streamH, AEGP_LTimeMode_LayerTime, &time, &index);
					AEGP_KeyframeIndex index = 0;
					suites.KeyframeSuite3()->AEGP_SetKeyframeValue(streamValue.streamH, index, &streamValue);
				}

				int temp = 0;
			}

			if (layer->ItemType == Aet::ItemType::Composition)
			{
				suites.LayerSuite1()->AEGP_SetLayerOffset(layer->GuiData.AE_Layer, &startTime);

				// NOTE: Makes sure underlying blend modes etc are being preserved
				suites.LayerSuite1()->AEGP_SetLayerFlag(layer->GuiData.AE_Layer, AEGP_LayerFlag_COLLAPSE, true);
			}
			else
			{
				// TODO: Ehh wtf?
				suites.LayerSuite1()->AEGP_SetLayerInPointAndDuration(layer->GuiData.AE_Layer, AEGP_LTimeMode_LayerTime, &startTime, &duration);
			}

			suites.LayerSuite1()->AEGP_SetLayerName(layer->GuiData.AE_Layer, layer->GetName().c_str());

			// TODO: This doesnt't work because setting the stretch also automatically (thanks adobe) changes the in and out point
			const A_Ratio stretch = { static_cast<A_long>(1.0f / layer->TimeScale * FixedPoint), static_cast<A_u_long>(FixedPoint) };
			suites.LayerSuite1()->AEGP_SetLayerStretch(layer->GuiData.AE_Layer, &stretch);
		}
	}

	void ImportLayersInComp(AEGP_SuiteHandler& suites, Aet::Composition* comp)
	{
		for (int i = static_cast<int>(comp->GetLayers().size()) - 1; i >= 0; i--)
			ImportLayer(suites, comp, comp->GetLayers().at(i).get());
	}
}

void SetWorkingAetImportDirectory(const std::wstring& directory)
{
	WorkingImportDirectory = directory;
}

A_Err VerifyAetSetImportable(const std::wstring& filePath, bool& canImport)
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

A_Err ImportAetSet(Aet::AetSet& aetSet, AE_FIM_ImportOptions importOptions, AE_FIM_SpecialAction action, AEGP_ItemH itemHandle)
{
	Aet::Scene& mainScene = *aetSet.GetScenes().front();
	GlobalFrameRate = mainScene.FrameRate;

	const A_Ratio aetFps = { static_cast<A_long>(mainScene.FrameRate * FixedPoint), static_cast<A_u_long>(FixedPoint) };

	AEGP_SuiteHandler suites = { GlobalBasicPicaSuite };

	AEGP_ProjectH projectHandle;
	suites.ProjSuite5()->AEGP_GetProjectByIndex(0, &projectHandle);

	AEGP_ItemH aeRootItem;
	suites.ProjSuite5()->AEGP_GetProjectRootFolder(projectHandle, &aeRootItem);

	AEGP_ItemH aeRootFolder;
	suites.ItemSuite8()->AEGP_CreateNewFolder(UTF16(L"root"), aeRootItem, &aeRootFolder);

	AEGP_ItemH aeDataFolder;
	suites.ItemSuite8()->AEGP_CreateNewFolder(UTF16(L"data"), aeRootFolder, &aeDataFolder);

	AEGP_ItemH aeCompFolder;
	suites.ItemSuite8()->AEGP_CreateNewFolder(UTF16(L"comps"), aeDataFolder, &aeCompFolder);

	AEGP_ItemH aeVideoFolder;
	suites.ItemSuite8()->AEGP_CreateNewFolder(UTF16(L"videos"), aeDataFolder, &aeVideoFolder);

	for (auto& video : mainScene.Videos)
		ImportVideo(suites, *video, aeVideoFolder);

	{
		const A_Time duration = FrameToATime(mainScene.EndFrame);

		suites.CompSuite4()->AEGP_CreateComp(aeRootFolder, mainScene.Name.c_str(), mainScene.Resolution.x, mainScene.Resolution.y, &OneToOneRatio, &duration, &aetFps, &mainScene.RootComposition->GuiData.AE_Comp);
		suites.CompSuite4()->AEGP_GetItemFromComp(mainScene.RootComposition->GuiData.AE_Comp, &mainScene.RootComposition->GuiData.AE_CompItem);
	}

	for (auto& comp : mainScene.Compositions)
	{
		const A_Time duration = FrameToATime(GetCompDuration(*comp));
		suites.CompSuite4()->AEGP_CreateComp(aeCompFolder, comp->GetName().data(), mainScene.Resolution.x, mainScene.Resolution.y, &OneToOneRatio, &duration, &aetFps, &comp->GuiData.AE_Comp);
		suites.CompSuite4()->AEGP_GetItemFromComp(comp->GuiData.AE_Comp, &comp->GuiData.AE_CompItem);
	}

	ImportLayersInComp(suites, mainScene.RootComposition.get());

	for (int i = static_cast<int>(mainScene.Compositions.size()) - 1; i >= 0; i--)
		ImportLayersInComp(suites, mainScene.Compositions[i].get());

	// suites.UtilitySuite3()->AEGP_ReportInfo(GlobalPluginID, "Let's hope for the best...");
	return A_Err_NONE;
}
