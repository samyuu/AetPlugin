#pragma once
#include "core_types.h"
#include "aet_plugin_main.h"
#include "aet_plugin_common.h"
#include "comfy/file_format_db.h"
#include "comfy/file_format_aet_set.h"
#include "comfy/file_format_spr_set.h"
#include "comfy/file_format_farc.h"
#include <unordered_map>

namespace AetPlugin
{
	struct SpriteFileData
	{
		static constexpr std::string_view PNGExtension = ".png";
		static constexpr std::string_view SprPrefix = "spr_";

		std::string SanitizedFileName;
		std::string FilePath;
	};

	struct AetImporter : NonCopyable
	{
		static std::pair<std::unique_ptr<Aet::AetSet>, std::unique_ptr<AetDB>> TryLoadAetSetAndDB(std::string_view aetFilePathOrFArc);
		static std::pair<std::unique_ptr<SprSet>, std::unique_ptr<SprDB>> TryLoadSprSetAndDB(std::string_view aetFilePathOrFArc);

		enum class AetSetVerifyResult { Valid, InvalidPath, InvalidFile, InvalidPointer, InvalidCount, InvalidData, };
		static AetSetVerifyResult VerifyAetSetImportable(std::string_view aetFilePathOrFArc);

		A_Err ImportAetSet(std::string_view workingDirectory, Aet::AetSet& aetSet, const SprSet* sprSet, const AetDB* aetDB, const SprDB* sprDB, AE_FIM_ImportOptions importOptions, AE_FIM_SpecialAction action, AEGP_ItemH itemHandle);

	private:
		const SpriteFileData* FindMatchingSpriteFile(std::string_view sourceName) const;
		void SetupWorkingSetData(const Aet::AetSet& aetSet, const SprSet* sprSet, const AetDB* aetDB, const SprDB* sprDB);
		void SetupWorkingSceneData(const Aet::Scene& scene, size_t sceneIndex);
		void CheckWorkingDirectoryFiles();

		A_Time FrameToAETime(frame_t frame) const;
		void GetProjectHandles();
		void CreateProjectFolders();

		b8 ShouldImportVideoDBForScene(const Aet::Scene& scene, size_t sceneIndex) const;

		void CreateSceneFolders();
		void ImportAllFootage();
		void CreateImportUnreferencedSprDBFootageAndLayer();
		std::vector<std::shared_ptr<Aet::Video>> CreateUnreferencedSprDBVideos() const;
		b8 UnreferencedVideoRequiresSeparateSprite(const Aet::Video& video) const;
		void ImportAllCompositions();

		void ImportVideo(const Aet::Video& video);
		void ImportPlaceholderVideo(const Aet::Video& video);
		void ImportSpriteVideo(const Aet::Video& video, b8 dbVideo = false);
		void ImportVideoAddItemToProject(const Aet::Video& video, b8 dbVideo = false);
		void ImportVideoSetSprIDComment(const Aet::Video& video);
		void ImportVideoSetSequenceInterpretation(const Aet::Video& video);
		void ImportVideoSetItemName(const Aet::Video& video);
		void ImportAudio(const Aet::Audio& audio);

		void ImportSceneComps();
		void ImportComposition(const Aet::Composition& comp, b8 videoDB = false);
		void ImportAllLayersInComp(const Aet::Composition& comp);
		void ImportLayer(const Aet::Composition& parentComp, const Aet::Layer& layer);
		void ImportLayerItemToComp(const Aet::Composition& parentComp, const Aet::Layer& layer);

		static std::unordered_map<const Aet::Composition*, frame_t> CreateGivenCompDurationsMap(const Aet::Scene& scene);
		static frame_t FindLongestLayerEndFrameInComp(const Aet::Composition& comp);
		static b8 LayerMakesUseOfStartOffset(const Aet::Layer& layer);

		void ImportLayerVideo(const Aet::Layer& layer, AetExDataTag& layerExtraData);
		void ImportLayerTransferMode(const Aet::Layer& layer, const Aet::LayerTransferMode& transferMode);
		void ImportLayerVideoStream(const Aet::Layer& layer, const Aet::LayerVideo& layerVideo);

		struct KeyFrameVec3 { frame_t Frame; vec3 Value; f32 Curve; };
		std::vector<KeyFrameVec3> combinedKeyFramesCache;

		void CombinePropertyKeyFrames(const Aet::FCurve* curveX, const Aet::FCurve* curveY, const Aet::FCurve* curveZ, std::vector<KeyFrameVec3>& outCombined) const;
		std::string_view GetLayerItemName(const Aet::Layer& layer) const;

		void ImportLayerAudio(const Aet::Layer& layer, AetExDataTag& layerExtraData);
		void ImportLayerTiming(const Aet::Layer& layer, AetExDataTag& layerExtraData);
		void ImportLayerName(const Aet::Layer& layer, AetExDataTag& layerExtraData);
		void ImportLayerFlags(const Aet::Layer& layer, AetExDataTag& layerExtraData);
		void ImportLayerQuality(const Aet::Layer& layer, AetExDataTag& layerExtraData);
		void ImportLayerMarkers(const Aet::Layer& layer, AetExDataTag& layerExtraData);

		void SetLayerRefParentLayer(const Aet::Layer& layer);

	private:
		SuitesData suites;
		AetExDataTagMap exData;

		struct WorkingDirectoryData
		{
			std::string ImportDirectory;
			std::vector<SpriteFileData> AvailableSpriteFiles;
		} workingDirectory;

		struct WorkingAetData
		{
			const Aet::AetSet* Set = nullptr;
			const SprSet* SprSet = nullptr;

			// NOTE: Optional to deal with hardcode expected IDs
			const AetSetEntry* AetEntry = nullptr;
			const SprSetEntry* SprEntry = nullptr;

			std::string NamePrefix, NamePrefixUnderscore;
		} workingSet;

		struct WorkingSceneData
		{
			const Aet::Scene* Scene = nullptr;
			size_t SceneIndex = 0;
			A_Ratio AE_FrameRate;

			// NOTE: Videos without sources don't have a name, so they will be named after their layer usage instead
			std::unordered_map<const Aet::Video*, const Aet::Layer*> SourcelessVideoLayerUsages;

			// NOTE: Because the AetSet format doesn't store the comp durations they have to be restored based on their layer usages
			std::unordered_map<const Aet::Composition*, frame_t> GivenCompDurations;

			std::shared_ptr<Aet::Composition> UnreferencedVideoComp = nullptr;
			std::shared_ptr<Aet::Layer> UnreferencedVideoLayer = nullptr;
		} workingScene = {};

		struct ProjectData
		{
			AEGP_ProjectH ProjectHandle;
			AEGP_ItemH RootItemHandle;
			struct FolderHandles
			{
				AEGP_ItemH Root;
				struct SceneData
				{
					AEGP_ItemH Root;
					AEGP_ItemH Data;
					AEGP_ItemH Video;
					AEGP_ItemH VideoDB;
					AEGP_ItemH Audio;
					AEGP_ItemH Comp;
				} Scene;
			} Folders;
		} project = {};
	};
}
