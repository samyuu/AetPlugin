#pragma once
#include "AetPlugin.h"
#include "AetExtraData.h"
#include <Graphics/Auth2D/Aet/AetSet.h>
#include <Graphics/Auth2D/SprSet.h>
#include <Database/AetDB.h>
#include <Database/SprDB.h>
#include <unordered_map>
#include <unordered_set>

namespace AetPlugin
{
	using namespace Comfy;
	using namespace Comfy::Graphics;

	struct SprSetSrcInfo
	{
		struct SprSrcInfo
		{
			std::string FilePath;
			const Aet::Video* Video = nullptr;
			bool UsesTrackMatte = false;
		};

		// NOTE: Key = Aet::VideoSource::Name
		std::unordered_map<std::string, SprSrcInfo> SprFileSources;
	};

	class AetExporter : NonCopyable
	{
	public:
		AetExporter() = default;
		~AetExporter() = default;

		void SetLog(FILE* log, LogLevel logLevel);

		std::string GetAetSetNameFromProjectName() const;

		std::pair<std::unique_ptr<Aet::AetSet>, std::unique_ptr<SprSetSrcInfo>> ExportAetSet(std::string_view workingDirectory, bool parseSprIDComments);
		std::unique_ptr<SprSet> CreateSprSetFromSprSetSrcInfo(const SprSetSrcInfo& sprSetSrcInfo, const Aet::AetSet& aetSet, bool powerOfTwo);

		Database::AetDB CreateAetDBFromAetSet(const Aet::AetSet& set, std::string_view setFileName) const;
		Database::SprDB CreateSprDBFromAetSet(const Aet::AetSet& set, std::string_view setFileName, const SprSet* sprSet) const;

		static bool IsProjectExportable(const SuitesData& suites);

	protected:
		LogLevel logLevel = LogLevel_None;
		FILE* logStream = nullptr;

		SuitesData suites;

		struct SettingsData
		{
			bool ParseSprIDComments;
		} settings = {};

	protected:
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

			inline bool IsParentOf(const AEItemData& parent) const
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

		void SetupWorkingProjectData();

	protected:
		AetExtraDataMapper extraData;

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

		void ExportComp(Aet::Composition& comp, AetExtraData& compExtraData);

		void ExportLayer(Aet::Layer& layer, AetExtraData& layerExtraData);
		void ExportLayerName(Aet::Layer& layer, AetExtraData& layerExtraData);
		void ExportLayerTime(Aet::Layer& layer, AetExtraData& layerExtraData);
		void ExportLayerQuality(Aet::Layer& layer, AetExtraData& layerExtraData);
		void ExportLayerMarkers(Aet::Layer& layer, AetExtraData& layerExtraData);
		void ExportLayerFlags(Aet::Layer& layer, AetExtraData& layerExtraData);
		void ExportLayerSourceItem(Aet::Layer& layer, AetExtraData& layerExtraData);

		void ExportLayerVideo(Aet::Layer& layer, AetExtraData& layerExtraData);
		void ExportLayerTransferMode(Aet::Layer& layer, Aet::LayerTransferMode& transferMode, AetExtraData& layerExtraData);
		void ExportLayerVideoStream(Aet::Layer& layer, Aet::LayerVideo& layerVideo, AetExtraData& layerExtraData);
		void SetLayerVideoPropertyLinearTangents(Aet::Property1D& property);

		struct AEKeyFrame { frame_t Time; vec3 Value; };
		std::vector<AEKeyFrame> aeKeyFramesCache;

		const std::vector<AEKeyFrame>& GetAEKeyFrames(const Aet::Layer& layer, const AetExtraData& layerExtraData, AEGP_LayerStream streamType);
		
		void ExportLayerAudio(Aet::Layer& layer, AetExtraData& layerExtraData);

		void ExportNewCompSource(Aet::Layer& layer, AEGP_ItemH sourceItem);
		void ExportNewVideoSource(Aet::Layer& layer, AEGP_ItemH sourceItem);
		void ExportVideo(Aet::Video& video, AetExtraData& videoExtraData);
		std::string FormatVideoSourceName(Aet::Video& video, std::string_view itemName, int sourceIndex, int sourceCount) const;

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
	};
}
