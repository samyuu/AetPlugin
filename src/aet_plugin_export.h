#pragma once
#include "core_types.h"
#include "aet_plugin_main.h"
#include "aet_plugin_common.h"
#include "comfy/file_format_db.h"
#include "comfy/file_format_aet_set.h"
#include "comfy/file_format_spr_set.h"
#include "comfy/file_format_farc.h"
#include <unordered_map>
#include <unordered_set>

namespace AetPlugin
{
	struct SprSetSrcInfo
	{
		struct SprSrcInfo
		{
			std::string FilePath;
			const Aet::Video* Video = nullptr;
			b8 UsesTrackMatte = false;
		};

		// NOTE: Key = Aet::VideoSource::Name
		std::unordered_map<std::string, SprSrcInfo> SprFileSources;
	};

	struct AetExporter : NonCopyable
	{
		void SetLog(FILE* log, LogLevelFlags level);

		std::string GetAetSetNameFromProjectName() const;
		std::pair<std::unique_ptr<Aet::AetSet>, std::unique_ptr<SprSetSrcInfo>> ExportAetSet(std::string_view workingDirectory, b8 parseSprIDComments);

		struct SprSetExportOptions { b8 PowerOfTwo, EnableCompression, EncodeYCbCr; };
		std::unique_ptr<SprSet> CreateSprSetFromSprSetSrcInfo(const SprSetSrcInfo& sprSetSrcInfo, const Aet::AetSet& aetSet, const SprSetExportOptions& options);

		std::unique_ptr<AetDB> CreateAetDBFromAetSet(const Aet::AetSet& set, std::string_view setFileName) const;
		std::unique_ptr<SprDB> CreateSprDBFromAetSet(const Aet::AetSet& set, std::string_view setFileName, const SprSet* sprSet) const;

		static b8 IsProjectExportable(const SuitesData& suites);
		const SuitesData& Suites() const;

	private:
		struct AEItemData;
		struct AEKeyFrame;

		void SetupWorkingProjectData();
		void SetupWorkingSetData(Aet::AetSet& set);
		void SetupWorkingSceneData(AEItemData* sceneComp);

		void ExportScene();
		void ExportAllCompositions();

		void ExportComp(Aet::Composition& comp, AetExDataTag& compExtraData);

		void ExportLayer(Aet::Layer& layer, AetExDataTag& layerExtraData);
		void ExportLayerName(Aet::Layer& layer, AetExDataTag& layerExtraData);
		void ExportLayerTime(Aet::Layer& layer, AetExDataTag& layerExtraData);
		void ExportLayerQuality(Aet::Layer& layer, AetExDataTag& layerExtraData);
		void ExportLayerMarkers(Aet::Layer& layer, AetExDataTag& layerExtraData);
		void ExportLayerFlags(Aet::Layer& layer, AetExDataTag& layerExtraData);
		void ExportLayerSourceItem(Aet::Layer& layer, AetExDataTag& layerExtraData);

		void ExportLayerVideo(Aet::Layer& layer, AetExDataTag& layerExtraData);
		void ExportLayerTransferMode(Aet::Layer& layer, Aet::LayerTransferMode& transferMode, AetExDataTag& layerExtraData);
		void ExportLayerVideoStream(Aet::Layer& layer, Aet::LayerVideo& layerVideo, AetExDataTag& layerExtraData);
		void TrySetLinearFCurveTangents(Aet::FCurve& property);

		const std::vector<AEKeyFrame>& GetAEKeyFrames(const Aet::Layer& layer, const AetExDataTag& layerExtraData, AEGP_LayerStream streamType);

		void ExportLayerAudio(Aet::Layer& layer, AetExDataTag& layerExtraData);

		void ExportNewCompSource(Aet::Layer& layer, AEGP_ItemH sourceItem);
		void ExportNewVideoSource(Aet::Layer& layer, AEGP_ItemH sourceItem);
		void ExportVideo(Aet::Video& video, AetExDataTag& videoExtraData);
		std::string FormatVideoSourceName(Aet::Video& video, std::string_view itemName, i32 sourceIndex, i32 sourceCount) const;

		void ExportNewAudioSource(Aet::Layer& layer, AEGP_ItemH sourceItem);

		std::shared_ptr<Aet::Composition> FindExistingCompSourceItem(AEGP_ItemH sourceItem);
		std::shared_ptr<Aet::Video> FindExistingVideoSourceItem(AEGP_ItemH sourceItem);
		std::shared_ptr<Aet::Audio> FindExistingAudioSourceItem(AEGP_ItemH sourceItem);

		void ScanCheckSetLayerRefParents(Aet::Layer& layer);
		std::shared_ptr<Aet::Layer> FindLayerRefParent(Aet::Layer& layer, AEGP_LayerH parentHandle);

		void FixInvalidSceneData();
		void FixInvalidCompositionData(Aet::Composition& comp);
		void FixInvalidCompositionTrackMatteDurations(Aet::Composition& comp);
		void FixInvalidLayerTrackMatteDurations(Aet::Layer& layer, Aet::Layer& trackMatteLayer);

		void UpdateSceneTrackMatteUsingVideos();
		void UpdateSetTrackMatteUsingVideos();

	private:
		LogLevelFlags logLevel = LogLevelFlags_None;
		FILE* logStream = nullptr;

		SuitesData suites;

		struct SettingsData
		{
			b8 ParseSprIDComments;
		} settings = {};

		struct AEItemData
		{
			std::string Name;
			std::string Comment;
			CommentUtil::Property CommentProperty;

			AEGP_ItemH ItemHandle;
			AEGP_CompH CompHandle;

			AEGP_ItemFlags Flags;
			AEGP_ItemType Type;
			std::pair<A_long, A_long> Dimensions;
			AEItemData* Parent;

			inline b8 IsParentOf(const AEItemData& parent) const
			{
				if (Parent == nullptr)
					return false;
				else if (Parent == &parent)
					return true;
				return Parent->IsParentOf(parent);
			}
		};

		struct WorkingProjectData
		{
			A_long Index;
			AEGP_ProjectH Handle;
			A_char Name[AEGP_MAX_PROJ_NAME_SIZE];
			std::string Path;
			AEGP_ItemH RootFolder;
			std::vector<AEItemData> Items;
		} workingProject = {};

		AetExDataTagMap extraData;

		struct WorkingDirectoryData
		{
			std::string ImportDirectory;
		} workingDirectory;

		struct WorkingSetData
		{
			Aet::AetSet* Set = nullptr;
			AEItemData* Folder = nullptr;
			std::vector<AEItemData*> SceneComps;

			std::string SprPrefix, SprHashPrefix;

			std::unique_ptr<SprSetSrcInfo> SprSetSrcInfo = nullptr;
			std::unordered_set<const Aet::Video*> TrackMatteUsingVideos;

			struct IDOverrideData
			{
				AetSetID AetSetID = AetSetID::Invalid;
				SprSetID SprSetID = SprSetID::Invalid;
				std::vector<AetSceneID> SceneIDs;
			} IDOverride;
		} workingSet;

		struct WorkingSceneData
		{
			Aet::Scene* Scene;
			AEItemData* AESceneComp;
		} workingScene = {};

		struct AEKeyFrame { frame_t Time; vec3 Value; };
		std::vector<AEKeyFrame> aeKeyFramesCache;
	};
}
