#include "AetExport.h"
#include "FormatUtil.h"
#include "StreamUtil.h"
#include <Graphics/Auth2D/Aet/AetUtil.h>
#include <Graphics/Utilities/SpritePacker.h>
#include <IO/Path.h>
#include <Misc/StringUtil.h>
#include <Misc/ImageHelper.h>
#include <Resource/IDHash.h>
#include <future>

#define LogLine(format, ...)		do { if (logLevel != LogLevel_None)		{ fprintf(logStream, "%s(): "			format "\n", __FUNCTION__, __VA_ARGS__); } } while (false)
#define LogInfoLine(format, ...)	do { if (logLevel & LogLevel_Info)		{ fprintf(logStream, "[INFO] %s(): "	format "\n", __FUNCTION__, __VA_ARGS__); } } while (false)
#define LogWarningLine(format, ...)	do { if (logLevel & LogLevel_Warning)	{ fprintf(logStream, "[WARNING] %s(): "	format "\n", __FUNCTION__, __VA_ARGS__); } } while (false)
#define LogErrorLine(format, ...)	do { if (logLevel & LogLevel_Error)		{ fprintf(logStream, "[ERROR] %s(): "	format "\n", __FUNCTION__, __VA_ARGS__); } } while (false)

namespace AetPlugin
{
	namespace
	{
		constexpr std::array ScreenModeResolutions
		{
			std::make_pair(ScreenMode::QVGA, ivec2(320, 240)),
			std::make_pair(ScreenMode::VGA, ivec2(640, 480)),
			std::make_pair(ScreenMode::SVGA, ivec2(800, 600)),
			std::make_pair(ScreenMode::XGA, ivec2(1024, 768)),
			std::make_pair(ScreenMode::SXGA, ivec2(1280, 1024)),
			std::make_pair(ScreenMode::SXGA_PLUS, ivec2(1400, 1050)),
			std::make_pair(ScreenMode::UXGA, ivec2(1600, 1200)),
			std::make_pair(ScreenMode::WVGA, ivec2(800, 480)),
			std::make_pair(ScreenMode::WSVGA, ivec2(1024, 600)),
			std::make_pair(ScreenMode::WXGA, ivec2(1280, 768)),
			std::make_pair(ScreenMode::WXGA_, ivec2(1360, 768)),
			std::make_pair(ScreenMode::WUXGA, ivec2(1920, 1200)),
			std::make_pair(ScreenMode::WQXGA, ivec2(2560, 1536)),
			std::make_pair(ScreenMode::HDTV720, ivec2(1280, 720)),
			std::make_pair(ScreenMode::HDTV1080, ivec2(1920, 1080)),
			std::make_pair(ScreenMode::WQHD, ivec2(2560, 1440)),

			// NOTE: Special cases for the CS3 games
			std::make_pair(static_cast<ScreenMode>(0x10), ivec2(960, 544)),
			std::make_pair(ScreenMode::HDTV720, ivec2(1280, 728)),
		};

		constexpr ScreenMode GetScreenModeFromResolution(ivec2 inputResolution)
		{
			for (const auto&[mode, resolution] : ScreenModeResolutions)
			{
				if (resolution == inputResolution)
					return mode;
			}

			return ScreenMode::HDTV720;
		}

		ivec2 GetAetSetResolution(const Aet::AetSet& set)
		{
			return !set.GetScenes().empty() ? set.GetScenes().front()->Resolution : ivec2(0, 0);
		}
	}

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
		setName += Util::ToSnakeCaseLowerCopy(std::string(IO::Path::TrimExtension(projectName)));
		return setName;
	}

	std::pair<std::unique_ptr<Aet::AetSet>, std::unique_ptr<SprSetSrcInfo>> AetExporter::ExportAetSet(std::string_view workingDirectory, bool parseSprIDComments)
	{
		LogLine("--- Log Start ---");
		LogLine("Log Level: '%s'", FormatUtil::FormatFlags(logLevel, LogLevelsToCheck).c_str());

		settings.ParseSprIDComments = parseSprIDComments;

		this->workingDirectory.ImportDirectory = workingDirectory;
		LogInfoLine("Working Directory: '%s'", this->workingDirectory.ImportDirectory.c_str());

		auto aetSet = std::make_unique<Aet::AetSet>();

		SetupWorkingProjectData();
		SetupWorkingSetData(*aetSet);

		for (auto* sceneComp : workingSet.SceneComps)
		{
			SetupWorkingSceneData(sceneComp);
			ExportScene();
			UpdateSceneTrackMatteUsingVideos();
		}

		UpdateSetTrackMatteUsingVideos();

		LogLine("--- Log End ---");
		return std::make_pair(std::move(aetSet), std::move(workingSet.SprSetSrcInfo));
	}

	std::unique_ptr<SprSet> AetExporter::CreateSprSetFromSprSetSrcInfo(const SprSetSrcInfo& sprSetSrcInfo, const Aet::AetSet& aetSet, const SprSetExportOptions& options)
	{
		struct SpriteImageSource
		{
			std::string SprName;
			const SprSetSrcInfo::SprSrcInfo* SrcSpr;

			ivec2 Size;
			std::unique_ptr<uint8_t[]> Pixels;
		};

		std::vector<std::future<SpriteImageSource>> imageSourceFutures;
		imageSourceFutures.reserve(sprSetSrcInfo.SprFileSources.size());

		for (const auto&[sprName, srcSpr] : sprSetSrcInfo.SprFileSources)
		{
			imageSourceFutures.emplace_back(std::async(std::launch::async, [&sprName, &srcSpr]
			{
				auto imageSource = SpriteImageSource();
				imageSource.SprName = sprName;
				imageSource.SrcSpr = &srcSpr;
				Util::ReadImage(imageSource.SrcSpr->FilePath, imageSource.Size, imageSource.Pixels);
				return imageSource;
			}));
		}

		std::vector<SpriteImageSource> imageSources;
		imageSources.reserve(imageSourceFutures.size());

		for (auto& future : imageSourceFutures)
		{
			if (auto imageSource = future.get(); imageSource.Pixels != nullptr)
				imageSources.emplace_back(std::move(imageSource));
		}

		// TODO: Maybe callback progress reporting
		auto spritePackerMarkups = std::vector<Utilities::SprMarkup>();
		auto spritePacker = Utilities::SpritePacker();
		spritePacker.Settings.PowerOfTwoTextures = options.PowerOfTwo;
		spritePacker.Settings.AllowYCbCrTextures = options.EncodeYCbCr;
		spritePacker.Settings.GenerateMipMaps = false;

		const auto aetSetScreenMode = GetScreenModeFromResolution(GetAetSetResolution(aetSet));

		spritePackerMarkups.reserve(imageSources.size());
		for (const auto& imageSource : imageSources)
		{
			auto& sprMarkup = spritePackerMarkups.emplace_back();
			sprMarkup.Name = imageSource.SprName;
			sprMarkup.Size = imageSource.Size;
			sprMarkup.RGBAPixels = imageSource.Pixels.get();
			sprMarkup.ScreenMode = aetSetScreenMode;
			sprMarkup.Flags = Utilities::SprMarkupFlags_None;

			// NOTE: Otherwise neighboring sprites might become visible through the mask
			if (imageSource.SrcSpr->UsesTrackMatte)
				sprMarkup.Flags |= Utilities::SprMarkupFlags_NoMerge;

			if (options.EnableCompression)
				sprMarkup.Flags |= Utilities::SprMarkupFlags_Compress;
		}

		return spritePacker.Create(spritePackerMarkups);
	}

	Database::AetDB AetExporter::CreateAetDBFromAetSet(const Aet::AetSet& set, std::string_view setFileName) const
	{
		Database::AetDB aetDB;

		auto& setEntry = aetDB.Entries.emplace_back();
		setEntry.FileName = Util::ToSnakeCaseLowerCopy(std::string(setFileName));
		setEntry.Name = Util::ToUpperCopy(set.Name);
		setEntry.ID = HashIDString<AetSetID>(setEntry.Name);

		if (workingSet.IDOverride.AetSetID != AetSetID::Invalid)
			setEntry.ID = workingSet.IDOverride.AetSetID;

		const auto sprSetName = Util::ToUpperCopy(std::string(SprPrefix)) + std::string(Util::StripPrefixInsensitive(setEntry.Name, AetPrefix));
		setEntry.SprSetID = HashIDString<SprSetID>(sprSetName);

		if (workingSet.IDOverride.SprSetID != SprSetID::Invalid)
			setEntry.SprSetID = workingSet.IDOverride.SprSetID;

		setEntry.SceneEntries.reserve(set.GetScenes().size());
		size_t sceneIndex = 0;
		for (const auto& scene : set.GetScenes())
		{
			auto& sceneEntry = setEntry.SceneEntries.emplace_back();
			sceneEntry.Name = setEntry.Name + "_" + Util::ToUpperCopy(scene->Name);
			sceneEntry.ID = HashIDString<AetSceneID>(sceneEntry.Name);

			if (sceneIndex < workingSet.IDOverride.SceneIDs.size() && workingSet.IDOverride.SceneIDs[sceneIndex] != AetSceneID::Invalid)
				sceneEntry.ID = workingSet.IDOverride.SceneIDs[sceneIndex];

			sceneIndex++;
		}

		return aetDB;
	}

	Database::SprDB AetExporter::CreateSprDBFromAetSet(const Aet::AetSet& set, std::string_view setFileName, const SprSet* sprSet) const
	{
		Database::SprDB sprDB;

		const auto sprPrefixStr = std::string(SprPrefix);
		const auto sprPrefixUpper = Util::ToUpperCopy(sprPrefixStr);

		auto& setEntry = sprDB.Entries.emplace_back();
		setEntry.FileName = Util::ToLowerCopy(sprPrefixStr) + Util::ToSnakeCaseLowerCopy(std::string(Util::StripPrefixInsensitive(setFileName, AetPrefix)));
		setEntry.Name = Util::ToUpperCopy(sprPrefixStr) + Util::ToUpperCopy(std::string(Util::StripPrefixInsensitive(set.Name, AetPrefix)));
		setEntry.ID = HashIDString<SprSetID>(setEntry.Name);

		if (workingSet.IDOverride.SprSetID != SprSetID::Invalid)
			setEntry.ID = workingSet.IDOverride.SprSetID;

		const auto sprPrefix = Util::ToUpperCopy(std::string(Util::StripPrefixInsensitive(set.Name, AetPrefix))) + "_";

		int16_t sprIndex = 0;
		for (const auto& scene : set.GetScenes())
		{
			setEntry.SprEntries.reserve(setEntry.SprEntries.size() + scene->Videos.size());
			for (const auto& video : scene->Videos)
			{
				for (const auto& source : video->Sources)
				{
					auto& sprEntry = setEntry.SprEntries.emplace_back();
					sprEntry.Name = sprPrefixUpper + source.Name;
					sprEntry.ID = source.ID;

					if (sprSet != nullptr)
					{
						const auto nameToFind = Util::StripPrefixInsensitive(source.Name, sprPrefix);
						auto matchingSpr = std::find_if(sprSet->Sprites.begin(), sprSet->Sprites.end(), [&](const Spr& spr) { return spr.Name == nameToFind; });

						if (matchingSpr != sprSet->Sprites.end())
							sprEntry.Index = static_cast<int16_t>(std::distance(sprSet->Sprites.data(), &(*matchingSpr)));
						else
							sprEntry.Index = sprIndex++;
					}
					else
					{
						sprEntry.Index = sprIndex++;
					}
				}
			}
		}

		if (sprSet != nullptr && sprSet->TexSet != nullptr)
		{
			const auto sprTexPrefixUpper = Util::ToUpperCopy(std::string(SprTexPrefix));

			setEntry.SprTexEntries.reserve(sprSet->TexSet->Textures.size());

			int16_t sprTexIndex = 0;
			for (const auto& tex : sprSet->TexSet->Textures)
			{
				auto& sprTexEntry = setEntry.SprTexEntries.emplace_back();
				sprTexEntry.Name = sprTexPrefixUpper + sprPrefix + tex->Name.value_or("UNKNOWN");
				sprTexEntry.ID = HashIDString<SprID>(sprTexEntry.Name);
				sprTexEntry.Index = sprTexIndex++;
			}
		}

		return sprDB;
	}

	bool AetExporter::IsProjectExportable(const SuitesData& suites)
	{
		AEGP_ProjectH projectHandle;
		if (suites.ProjSuite5->AEGP_GetProjectByIndex(0, &projectHandle) != A_Err_NONE)
			return false;

		AEGP_ItemH rootFolder;
		if (suites.ProjSuite5->AEGP_GetProjectRootFolder(projectHandle, &rootFolder) != A_Err_NONE)
			return false;

		AEGP_ItemH firstItemHandle;
		if (suites.ItemSuite8->AEGP_GetFirstProjItem(projectHandle, &firstItemHandle) != A_Err_NONE)
			return false;

		size_t aetSetCommentItemCount = 0;

		AEGP_ItemH currentItemHandle, previousItemHandle = firstItemHandle;
		while (true)
		{
			if (suites.ItemSuite8->AEGP_GetNextProjItem(projectHandle, previousItemHandle, &currentItemHandle); currentItemHandle == nullptr)
				break;

			previousItemHandle = currentItemHandle;

			A_u_long commentSize;
			if (suites.ItemSuite8->AEGP_GetItemCommentLength(currentItemHandle, &commentSize) != A_Err_NONE || commentSize <= 0)
				continue;

			CommentUtil::Buffer commentBuffer;
			const auto comment = CommentUtil::Get(suites.ItemSuite8, currentItemHandle, commentBuffer);

			if (comment.Key == CommentUtil::Keys::AetSet)
				aetSetCommentItemCount++;
		}

		// NOTE: Only allow a single set comment so there won't be any accidental exports due to ambiguity
		return (aetSetCommentItemCount == 1);
	}

	const SuitesData& AetExporter::Suites() const
	{
		return suites;
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
		workingProject.Path = UTF8::Narrow(AEUtil::MoveFreeUTF16String(suites.MemorySuite1, pathHandle));
		LogInfoLine("Project Path: '%s'", workingProject.Path.c_str());

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
			item.Name = UTF8::Narrow(AEUtil::MoveFreeUTF16String(suites.MemorySuite1, nameHandle));

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
			LogInfoLine("Suitable AetSet folder found: '%s'", foundSetFolder->Name.c_str());

			const auto[setName, aetSetID, sprSetID] = CommentUtil::ParseArray<3>(foundSetFolder->CommentProperty.Value);
			set.Name = setName;
			workingSet.IDOverride.AetSetID = static_cast<AetSetID>(CommentUtil::ParseID(aetSetID));
			workingSet.IDOverride.SprSetID = static_cast<SprSetID>(CommentUtil::ParseID(sprSetID));

			workingSet.Folder = &(*foundSetFolder);

			workingSet.SceneComps.reserve(2);
			for (auto& item : workingProject.Items)
			{
				if (item.Type == AEGP_ItemType_COMP && item.CommentProperty.Key == CommentUtil::Keys::AetScene && item.IsParentOf(*foundSetFolder))
				{
					workingSet.SceneComps.push_back(&item);
					LogInfoLine("Suitable scene composition found: '%s'", item.Name.c_str());
				}
			}

			std::sort(workingSet.SceneComps.begin(), workingSet.SceneComps.end(), [&](const auto& sceneA, const auto& sceneB)
			{
				return sceneA->CommentProperty.KeyIndex.value_or(0) < sceneB->CommentProperty.KeyIndex.value_or(0);
			});

			for (const auto& sceneComp : workingSet.SceneComps)
			{
				const auto[sceneName, sceneID] = CommentUtil::ParseArray<2>(sceneComp->CommentProperty.Value);
				workingSet.IDOverride.SceneIDs.push_back(static_cast<AetSceneID>(CommentUtil::ParseID(sceneID)));
			}
		}

		workingSet.SprPrefix = Util::ToUpperCopy(std::string(Util::StripPrefixInsensitive(workingSet.Set->Name, AetPrefix))) + "_";
		workingSet.SprHashPrefix = Util::ToUpperCopy(std::string(SprPrefix)) + workingSet.SprPrefix;
		workingSet.SprSetSrcInfo = std::make_unique<SprSetSrcInfo>();
	}

	void AetExporter::SetupWorkingSceneData(AEItemData* sceneComp)
	{
		workingScene.Scene = workingSet.Set->GetScenes().emplace_back(std::make_shared<Aet::Scene>()).get();
		workingScene.AESceneComp = sceneComp;
	}

	void AetExporter::ExportScene()
	{
		auto& scene = *workingScene.Scene;
		auto& aeSceneComp = *workingScene.AESceneComp;

		const auto[sceneName, sceneID] = CommentUtil::ParseArray<2>(aeSceneComp.CommentProperty.Value);
		scene.Name = sceneName;

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
		workingScene.Scene->RootComposition = std::make_shared<Aet::Composition>();
		auto& rootCompExtraData = extraData.Get(*workingScene.Scene->RootComposition);

		rootCompExtraData.AE_Comp = workingScene.AESceneComp->CompHandle;
		rootCompExtraData.AE_CompItem = workingScene.AESceneComp->ItemHandle;
		ExportComp(*workingScene.Scene->RootComposition, rootCompExtraData);

		// NOTE: Reverse once at the end because pushing to the end of the vector while creating the comps is a lot more efficient
		std::reverse(workingScene.Scene->Compositions.begin(), workingScene.Scene->Compositions.end());
		// std::reverse(workingScene.Scene->Videos.begin(), workingScene.Scene->Videos.end());

		for (auto& layer : workingScene.Scene->RootComposition->GetLayers())
			ScanCheckSetLayerRefParents(*layer);
	}

	void AetExporter::ExportComp(Aet::Composition& comp, AetExtraData& compExtraData)
	{
		A_long compLayerCount;
		suites.LayerSuite7->AEGP_GetCompNumLayers(compExtraData.AE_Comp, &compLayerCount);

		auto& compLayers = comp.GetLayers();
		compLayers.reserve(compLayerCount);

		for (A_long i = 0; i < compLayerCount; i++)
		{
			auto& layer = *compLayers.emplace_back(std::make_shared<Aet::Layer>());
			auto& layerExtraData = extraData.Get(layer);

			suites.LayerSuite7->AEGP_GetCompLayerByIndex(compExtraData.AE_Comp, i, &layerExtraData.AE_Layer);
			ExportLayer(layer, layerExtraData);
		}
	}

	void AetExporter::ExportLayer(Aet::Layer& layer, AetExtraData& layerExtraData)
	{
		ExportLayerName(layer, layerExtraData);
		ExportLayerTime(layer, layerExtraData);
		ExportLayerQuality(layer, layerExtraData);
		ExportLayerMarkers(layer, layerExtraData);
		ExportLayerFlags(layer, layerExtraData);
		ExportLayerSourceItem(layer, layerExtraData);

		if (layer.ItemType == Aet::ItemType::Video || layer.ItemType == Aet::ItemType::Composition)
			ExportLayerVideo(layer, layerExtraData);
		else if (layer.ItemType == Aet::ItemType::Audio)
			ExportLayerAudio(layer, layerExtraData);
	}

	void AetExporter::ExportLayerName(Aet::Layer& layer, AetExtraData& layerExtraData)
	{
		AEGP_MemHandle nameHandle, sourceNameHandle;
		suites.LayerSuite7->AEGP_GetLayerName(EvilGlobalState.PluginID, layerExtraData.AE_Layer, &nameHandle, &sourceNameHandle);

		const auto name = UTF8::Narrow(AEUtil::MoveFreeUTF16String(suites.MemorySuite1, nameHandle));
		const auto sourceName = UTF8::Narrow(AEUtil::MoveFreeUTF16String(suites.MemorySuite1, sourceNameHandle));

		if (!name.empty() && name.front() != '\0')
			layer.SetName(name);
		else
			layer.SetName(sourceName);
	}

	void AetExporter::ExportLayerTime(Aet::Layer& layer, AetExtraData& layerExtraData)
	{
		A_Time offset, inPoint, duration;
		A_Ratio stretch;
		suites.LayerSuite7->AEGP_GetLayerOffset(layerExtraData.AE_Layer, &offset);
		suites.LayerSuite7->AEGP_GetLayerInPoint(layerExtraData.AE_Layer, AEGP_LTimeMode_LayerTime, &inPoint);
		suites.LayerSuite7->AEGP_GetLayerDuration(layerExtraData.AE_Layer, AEGP_LTimeMode_LayerTime, &duration);
		suites.LayerSuite7->AEGP_GetLayerStretch(layerExtraData.AE_Layer, &stretch);

		const frame_t frameOffset = AEUtil::AETimeToFrame(offset, workingScene.Scene->FrameRate);
		const frame_t frameInPoint = AEUtil::AETimeToFrame(inPoint, workingScene.Scene->FrameRate);
		const frame_t frameDuration = AEUtil::AETimeToFrame(duration, workingScene.Scene->FrameRate);
		const float floatStretch = AEUtil::Ratio(stretch);

		layer.StartFrame = (frameOffset + frameInPoint);
		layer.EndFrame = (layer.StartFrame + (frameDuration * floatStretch));
		layer.StartOffset = (frameInPoint);
		layer.TimeScale = (1.0f / floatStretch);
	}

	void AetExporter::ExportLayerQuality(Aet::Layer& layer, AetExtraData& layerExtraData)
	{
		AEGP_LayerQuality layerQuality;
		suites.LayerSuite7->AEGP_GetLayerQuality(layerExtraData.AE_Layer, &layerQuality);
		layer.Quality = (static_cast<Aet::LayerQuality>(layerQuality + 1));
	}

	void AetExporter::ExportLayerMarkers(Aet::Layer& layer, AetExtraData& layerExtraData)
	{
		AEGP_StreamRefH streamRef;
		suites.StreamSuite4->AEGP_GetNewLayerStream(EvilGlobalState.PluginID, layerExtraData.AE_Layer, AEGP_LayerStream_MARKER, &streamRef);

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

			const frame_t frameTime = AEUtil::AETimeToFrame(time, workingScene.Scene->FrameRate);
			layer.Markers.push_back(std::make_shared<Aet::Marker>(frameTime, streamVal.val.markerH[0]->nameAC));
		}
	}

	void AetExporter::ExportLayerFlags(Aet::Layer& layer, AetExtraData& layerExtraData)
	{
		AEGP_LayerFlags layerFlags;
		suites.LayerSuite7->AEGP_GetLayerFlags(layerExtraData.AE_Layer, &layerFlags);

		if (layerFlags & AEGP_LayerFlag_VIDEO_ACTIVE) layer.Flags.VideoActive = true;
		if (layerFlags & AEGP_LayerFlag_AUDIO_ACTIVE) layer.Flags.AudioActive = true;
		if (layerFlags & AEGP_LayerFlag_LOCKED) layer.Flags.Locked = true;
		if (layerFlags & AEGP_LayerFlag_SHY) layer.Flags.Shy = true;
	}

	namespace
	{
		bool HasAudioFileExtension(std::string_view name)
		{
			constexpr auto knownAudioExtensions = ".aif;.aiff;.adx;.wav;.mp3;.mpeg;.mpg;.ogg;.flac";
			return IO::Path::DoesAnyPackedExtensionMatch(IO::Path::GetExtension(name), knownAudioExtensions);
		}
	}

	void AetExporter::ExportLayerSourceItem(Aet::Layer& layer, AetExtraData& layerExtraData)
	{
		AEGP_ItemH sourceItem;
		suites.LayerSuite7->AEGP_GetLayerSourceItem(layerExtraData.AE_Layer, &sourceItem);

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
			// HACK: This is to ensure layers that have been imported using a dummy solid will still be exported correctly despite technically not being audio footage in AE
			if (HasAudioFileExtension(layer.GetName()))
			{
				layer.ItemType = Aet::ItemType::Audio;

				if (auto existingAudioItem = FindExistingAudioSourceItem(sourceItem); existingAudioItem != nullptr)
					layer.SetItem(existingAudioItem);
				else
					ExportNewAudioSource(layer, sourceItem);
			}
			else
			{
				layer.ItemType = Aet::ItemType::Video;

				if (auto existingVideoItem = FindExistingVideoSourceItem(sourceItem); existingVideoItem != nullptr)
					layer.SetItem(existingVideoItem);
				else
					ExportNewVideoSource(layer, sourceItem);
			}
		}
	}

	void AetExporter::ExportLayerVideo(Aet::Layer& layer, AetExtraData& layerExtraData)
	{
		layer.LayerVideo = std::make_shared<Aet::LayerVideo>();

		if (A_Boolean isLayer3D; (suites.LayerSuite1->AEGP_IsLayer3D(layerExtraData.AE_Layer, &isLayer3D) == A_Err_NONE) && isLayer3D)
			layer.LayerVideo->Transform3D = std::make_shared<Aet::LayerVideo3D>();

		ExportLayerTransferMode(layer, layer.LayerVideo->TransferMode, layerExtraData);
		ExportLayerVideoStream(layer, *layer.LayerVideo, layerExtraData);
	}

	void AetExporter::ExportLayerTransferMode(Aet::Layer& layer, Aet::LayerTransferMode& transferMode, AetExtraData& layerExtraData)
	{
		AEGP_LayerTransferMode layerTransferMode;
		suites.LayerSuite7->AEGP_GetLayerTransferMode(layerExtraData.AE_Layer, &layerTransferMode);

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

	void AetExporter::ExportLayerVideoStream(Aet::Layer& layer, Aet::LayerVideo& layerVideo, AetExtraData& layerExtraData)
	{
		auto exportStream = [&](AEGP_LayerStream streamType, Aet::Property1D* propX, Aet::Property1D* propY, Aet::Property1D* propZ)
		{
			if (propX == nullptr && propY == nullptr && propZ == nullptr)
				return;

			const auto& aeKeyFrames = GetAEKeyFrames(layer, layerExtraData, streamType);

			if (propX != nullptr)
			{
				for (const auto& aeKeyFrame : aeKeyFrames)
					propX->Keys.emplace_back(aeKeyFrame.Time, aeKeyFrame.Value.x);
				SetLayerVideoPropertyLinearTangents(*propX);
			}

			if (propY != nullptr)
			{
				for (const auto& aeKeyFrame : aeKeyFrames)
					propY->Keys.emplace_back(aeKeyFrame.Time, aeKeyFrame.Value.y);
				SetLayerVideoPropertyLinearTangents(*propY);
			}

			if (propZ != nullptr)
			{
				for (const auto& aeKeyFrame : aeKeyFrames)
					propZ->Keys.emplace_back(aeKeyFrame.Time, aeKeyFrame.Value.z);
				SetLayerVideoPropertyLinearTangents(*propZ);
			}
		};

		const auto combinedLayerVideo = StreamUtil::CombinedAetLayerVideo2D3D(layerVideo);
		exportStream(AEGP_LayerStream_ANCHORPOINT, combinedLayerVideo.OriginX, combinedLayerVideo.OriginY, combinedLayerVideo.OriginZ);
		exportStream(AEGP_LayerStream_POSITION, combinedLayerVideo.PositionX, combinedLayerVideo.PositionY, combinedLayerVideo.PositionZ);
		exportStream(AEGP_LayerStream_ROTATE_X, combinedLayerVideo.RotationX, nullptr, nullptr);
		exportStream(AEGP_LayerStream_ROTATE_Y, combinedLayerVideo.RotationY, nullptr, nullptr);
		exportStream(AEGP_LayerStream_ROTATE_Z, combinedLayerVideo.RotationZ, nullptr, nullptr);
		exportStream(AEGP_LayerStream_SCALE, combinedLayerVideo.ScaleX, combinedLayerVideo.ScaleY, combinedLayerVideo.ScaleZ);
		exportStream(AEGP_LayerStream_OPACITY, combinedLayerVideo.Opacity, nullptr, nullptr);
		exportStream(AEGP_LayerStream_ORIENTATION, combinedLayerVideo.DirectionX, combinedLayerVideo.DirectionY, combinedLayerVideo.DirectionZ);
	}

	void AetExporter::SetLayerVideoPropertyLinearTangents(Aet::Property1D& property)
	{
		if (property->size() < 2)
			return;

		auto getLinearTangent = [](const auto& keyFrameA, const auto& keyFrameB)
		{
			if (keyFrameA.Frame == keyFrameB.Frame)
				return 0.0f;

			return static_cast<float>(
				(static_cast<double>(keyFrameB.Value) - static_cast<double>(keyFrameA.Value)) /
				(static_cast<double>(keyFrameB.Frame) - static_cast<double>(keyFrameA.Frame)));
		};

		for (size_t i = 0; i < property->size(); i++)
		{
			auto* prevKeyFrame = (i - 1 < property->size()) ? &property.Keys[i - 1] : nullptr;
			auto* currKeyFrame = &property.Keys[i + 0];
			auto* nextKeyFrame = (i + 1 < property->size()) ? &property.Keys[i + 1] : nullptr;

			if (prevKeyFrame == nullptr)
				currKeyFrame->Curve = getLinearTangent(*currKeyFrame, *nextKeyFrame);
			else if (nextKeyFrame == nullptr)
				currKeyFrame->Curve = getLinearTangent(*prevKeyFrame, *currKeyFrame);
			else
				currKeyFrame->Curve = (getLinearTangent(*prevKeyFrame, *currKeyFrame) + getLinearTangent(*currKeyFrame, *nextKeyFrame)) / 2.0f;
		}
	}

	const std::vector<AetExporter::AEKeyFrame>& AetExporter::GetAEKeyFrames(const Aet::Layer& layer, const AetExtraData& layerExtraData, AEGP_LayerStream streamType)
	{
		const auto scaleFactor = 1.0f / StreamUtil::GetAetToAEStreamFactor(streamType);
		auto threeDValToVec3 = [&](const AEGP_ThreeDVal& input)
		{
			return vec3(static_cast<float>(input.x) * scaleFactor, static_cast<float>(input.y) * scaleFactor, static_cast<float>(input.z) * scaleFactor);
		};

		aeKeyFramesCache.clear();

		AEGP_StreamRefH streamRef;
		suites.StreamSuite4->AEGP_GetNewLayerStream(EvilGlobalState.PluginID, layerExtraData.AE_Layer, streamType, &streamRef);

		A_long keyFrameCount;
		suites.KeyframeSuite3->AEGP_GetStreamNumKFs(streamRef, &keyFrameCount);

		if (keyFrameCount < 1)
		{
			const auto zeroTime = AEUtil::FrameToAETime(0.0f, workingScene.Scene->FrameRate);

			AEGP_StreamVal2 streamVal2;
			AEGP_StreamType outStreamType;
			suites.StreamSuite4->AEGP_GetLayerStreamValue(layerExtraData.AE_Layer, streamType, AEGP_LTimeMode_LayerTime, &zeroTime, false, &streamVal2, &outStreamType);

			// HACK: Not sure why this one returns 0.0f despite being treated and displated as the default 100% scale in AE
			if (streamType == AEGP_LayerStream_SCALE)
				streamVal2.three_d.z = 100.0f;

			aeKeyFramesCache.push_back({ 0.0f, threeDValToVec3(streamVal2.three_d) });
			return aeKeyFramesCache;
		}

		for (AEGP_KeyframeIndex i = 0; i < keyFrameCount; i++)
		{
			A_Time time;
			suites.KeyframeSuite3->AEGP_GetKeyframeTime(streamRef, i, AEGP_LTimeMode_LayerTime, &time);
			AEGP_StreamValue streamVal;
			suites.KeyframeSuite3->AEGP_GetNewKeyframeValue(EvilGlobalState.PluginID, streamRef, i, &streamVal);

			const frame_t frameTime = AEUtil::AETimeToFrame(time, workingScene.Scene->FrameRate) + layer.StartFrame;
			aeKeyFramesCache.push_back({ frameTime, threeDValToVec3(streamVal.val.three_d) });
		}
		return aeKeyFramesCache;
	}

	void AetExporter::ExportLayerAudio(Aet::Layer& layer, AetExtraData& layerExtraData)
	{
		layer.LayerAudio = std::make_shared<Aet::LayerAudio>();
	}

	void AetExporter::ExportNewCompSource(Aet::Layer& layer, AEGP_ItemH sourceItem)
	{
		auto& newCompItem = workingScene.Scene->Compositions.emplace_back(std::make_shared<Aet::Composition>());
		auto& compExtraData = extraData.Get(*newCompItem);

		compExtraData.AE_CompItem = sourceItem;
		suites.CompSuite7->AEGP_GetCompFromItem(compExtraData.AE_CompItem, &compExtraData.AE_Comp);

		layer.SetItem(newCompItem);
		ExportComp(*newCompItem, compExtraData);
	}

	void AetExporter::ExportNewVideoSource(Aet::Layer& layer, AEGP_ItemH sourceItem)
	{
		auto& newVideoItem = workingScene.Scene->Videos.emplace_back(std::make_shared<Aet::Video>());
		auto& videoExtraData = extraData.Get(*newVideoItem);

		videoExtraData.AE_FootageItem = sourceItem;
		suites.FootageSuite5->AEGP_GetMainFootageFromItem(videoExtraData.AE_FootageItem, &videoExtraData.AE_Footage);

		layer.SetItem(newVideoItem);
		ExportVideo(*newVideoItem, videoExtraData);
	}

	void AetExporter::ExportVideo(Aet::Video& video, AetExtraData& videoExtraData)
	{
		auto foundItem = std::find_if(workingProject.Items.begin(), workingProject.Items.end(), [&](const auto& item) { return (item.ItemHandle == videoExtraData.AE_FootageItem); });
		if (foundItem == workingProject.Items.end())
			return;

		auto& item = *foundItem;
		const auto cleanItemName = Util::ToUpperCopy(std::string(Util::StripPrefixInsensitive(Util::StripPrefixInsensitive(IO::Path::TrimExtension(item.Name), SprPrefix), workingSet.SprPrefix)));

		suites.FootageSuite5->AEGP_GetMainFootageFromItem(item.ItemHandle, &videoExtraData.AE_Footage);

		AEGP_FootageSignature signature;
		suites.FootageSuite5->AEGP_GetFootageSignature(videoExtraData.AE_Footage, &signature);

		AEGP_FootageInterp interpretation;
		suites.FootageSuite5->AEGP_GetFootageInterpretation(videoExtraData.AE_FootageItem, false, &interpretation);

		video.Size = { item.Dimensions.first, item.Dimensions.second };

		if (signature == AEGP_FootageSignature_SOLID)
		{
			AEGP_ColorVal footageColor;
			suites.FootageSuite5->AEGP_GetSolidFootageColor(item.ItemHandle, false, &footageColor);
			video.Color = AEUtil::ColorRGB8(footageColor);
			video.FilesPerFrame = 0.0f;
		}
		else
		{
			video.Color = 0xFFFFFF00;

			A_long mainFilesCount, filesPerFrame;
			suites.FootageSuite5->AEGP_GetFootageNumFiles(videoExtraData.AE_Footage, &mainFilesCount, &filesPerFrame);

			video.Sources.reserve(mainFilesCount);
			for (A_long i = 0; i < mainFilesCount; i++)
			{
				auto& source = video.Sources.emplace_back();
				const auto sourceName = FormatVideoSourceName(video, cleanItemName, i, mainFilesCount);

				source.Name = workingSet.SprPrefix + sourceName; // NOTE: {SET_NAME}_{SPRITE_NAME}
				source.ID = HashIDString<SprID>(workingSet.SprHashPrefix + sourceName); // NOTE: SPR_{SET_NAME}_{SPRITE_NAME}

				AEGP_MemHandle pathHandle;
				suites.FootageSuite5->AEGP_GetFootagePath(videoExtraData.AE_Footage, i, AEGP_FOOTAGE_MAIN_FILE_INDEX, &pathHandle);

				const auto sourcePath = UTF8::Narrow(AEUtil::MoveFreeUTF16String(suites.MemorySuite1, pathHandle));

				if (!sourcePath.empty())
					workingSet.SprSetSrcInfo->SprFileSources[sourceName] = SprSetSrcInfo::SprSrcInfo { sourcePath, &video };
			}

			if (mainFilesCount > 1)
				video.FilesPerFrame = static_cast<float>(interpretation.native_fpsF) / workingScene.Scene->FrameRate;
			else
				video.FilesPerFrame = 1.0f;

			if (item.CommentProperty.Key == CommentUtil::Keys::Spr && settings.ParseSprIDComments)
			{
				size_t i = 0;
				CommentUtil::IterateCommaSeparateList(item.CommentProperty.Value, [&](auto item)
				{
					const auto sprID = static_cast<SprID>(CommentUtil::ParseID(item));
					if (sprID != SprID::Invalid && i < video.Sources.size())
						video.Sources[i].ID = sprID;
					i++;
				});
			}
		}
	}

	std::string AetExporter::FormatVideoSourceName(Aet::Video& video, std::string_view itemName, int sourceIndex, int sourceCount) const
	{
		if (sourceCount == 1)
			return std::string(itemName);

		char indexString[16];
		sprintf_s(indexString, "%03d", sourceIndex);

		const auto preIndexString = itemName.substr(0, itemName.rfind('['));
		return std::string(preIndexString) + indexString;
	}

	void AetExporter::ExportNewAudioSource(Aet::Layer& layer, AEGP_ItemH sourceItem)
	{
		auto& newAudioItem = workingScene.Scene->Audios.emplace_back(std::make_shared<Aet::Audio>());
		auto& audioExtraData = extraData.Get(*newAudioItem);

		audioExtraData.AE_FootageItem = sourceItem;
		suites.FootageSuite5->AEGP_GetMainFootageFromItem(audioExtraData.AE_FootageItem, &audioExtraData.AE_Footage);

		layer.SetItem(newAudioItem);
		newAudioItem->SoundID = 0;
	}

	std::shared_ptr<Aet::Composition> AetExporter::FindExistingCompSourceItem(AEGP_ItemH sourceItem)
	{
		// NOTE: No point in searching the root comp since it should never be referenced by any other child comp (?)
		for (const auto& comp : workingScene.Scene->Compositions)
		{
			if (extraData.Get(*comp).AE_CompItem == sourceItem)
				return comp;
		}
		return nullptr;
	}

	std::shared_ptr<Aet::Video> AetExporter::FindExistingVideoSourceItem(AEGP_ItemH sourceItem)
	{
		for (const auto& video : workingScene.Scene->Videos)
		{
			if (extraData.Get(*video).AE_FootageItem == sourceItem)
				return video;
		}
		return nullptr;
	}

	std::shared_ptr<Aet::Audio> AetExporter::FindExistingAudioSourceItem(AEGP_ItemH sourceItem)
	{
		for (const auto& audio : workingScene.Scene->Audios)
		{
			if (extraData.Get(*audio).AE_FootageItem == sourceItem)
				return audio;
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
		suites.LayerSuite7->AEGP_GetLayerParent(extraData.Get(layer).AE_Layer, &parentHandle);

		if (parentHandle != nullptr)
			layer.SetRefParentLayer(FindLayerRefParent(layer, parentHandle));
	}

	std::shared_ptr<Aet::Layer> AetExporter::FindLayerRefParent(Aet::Layer& layer, AEGP_LayerH parentHandle)
	{
		for (auto& comp : workingScene.Scene->Compositions)
		{
			for (auto& layer : comp->GetLayers())
			{
				if (extraData.Get(*layer).AE_Layer == parentHandle)
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

	void AetExporter::UpdateSceneTrackMatteUsingVideos()
	{
		auto checkComp = [&](const auto& comp)
		{
			for (const auto& layer : comp.GetLayers())
			{
				if (layer->ItemType != Aet::ItemType::Video || layer->LayerVideo == nullptr)
					continue;

				if (layer->GetVideoItem() != nullptr && layer->LayerVideo->TransferMode.TrackMatte != Aet::TrackMatte::NoTrackMatte)
					workingSet.TrackMatteUsingVideos.insert(layer->GetVideoItem().get());
			}
		};

		for (const auto& comp : workingScene.Scene->Compositions)
			checkComp(*comp);
		checkComp(*workingScene.Scene->RootComposition);
	}

	void AetExporter::UpdateSetTrackMatteUsingVideos()
	{
		for (auto&[sprName, srcSpr] : workingSet.SprSetSrcInfo->SprFileSources)
			srcSpr.UsesTrackMatte = (workingSet.TrackMatteUsingVideos.find(srcSpr.Video) != workingSet.TrackMatteUsingVideos.end());
	}
}
