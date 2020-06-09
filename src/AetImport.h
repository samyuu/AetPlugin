#pragma once
#include "AetPlugin.h"
#include "AetExtraData.h"
#include <Graphics/Auth2D/Aet/AetSet.h>
#include <Database/AetDB.h>
#include <Database/SprDB.h>
#include <unordered_map>

namespace AetPlugin
{
	using namespace Comfy;
	using namespace Comfy::Graphics;

	struct SpriteFileData
	{
		static constexpr std::string_view PNGExtension = ".png";
		static constexpr std::string_view SprPrefix = "spr_";

		std::string SanitizedFileName;
		std::string FilePath;
	};

	class AetImporter : NonCopyable
	{
	public:
		static std::unique_ptr<Aet::AetSet> LoadAetSet(std::string_view filePath);

		enum class AetSetVerifyResult
		{
			Valid,
			InvalidPath,
			InvalidFile,
			InvalidPointer,
			InvalidCount,
			InvalidData,
		};

		static AetSetVerifyResult VerifyAetSetImportable(std::string_view filePath);

	public:
		AetImporter(std::string_view workingDirectory);
		~AetImporter() = default;

		A_Err ImportAetSet(Aet::AetSet& set, AE_FIM_ImportOptions importOptions, AE_FIM_SpecialAction action, AEGP_ItemH itemHandle);

	protected:
		SuitesData suites;

	protected:
		struct WorkingDirectoryData
		{
			std::string ImportDirectory;
			std::vector<SpriteFileData> AvailableSpriteFiles;

			// NOTE: Optional to deal with hardcode expected IDs
			struct DatabaseData
			{
				std::unique_ptr<Database::AetDB> AetDB = nullptr;
				std::unique_ptr<Database::SprDB> SprDB = nullptr;
				const Database::AetSetEntry* AetSetEntry = nullptr;
				const Database::SprSetEntry* SprSetEntry = nullptr;
			} DB;

		} workingDirectory;

		const SpriteFileData* FindMatchingSpriteFile(std::string_view sourceName) const;

	protected:
		AetExtraDataMapper extraData;

		struct WorkingAetData
		{
			const Aet::AetSet* Set = nullptr;
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

		void SetupWorkingSetData(const Aet::AetSet& set);
		void SetupWorkingSceneData(const Aet::Scene& scene, size_t sceneIndex);
		void CheckWorkingDirectoryFiles();

	protected:
		A_Time FrameToAETime(frame_t frame) const;

	protected:
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

		void GetProjectHandles();
		void CreateProjectFolders();

	protected:
		bool ShouldImportVideoDBForScene(const Aet::Scene& scene, size_t sceneIndex) const;

	protected:
		void CreateSceneFolders();
		void ImportAllFootage();

		void CreateImportUnreferencedSprDBFootageAndLayer();
		std::vector<std::shared_ptr<Aet::Video>> CreateUnreferencedSprDBVideos() const;
		bool UnreferencedVideoRequiresSeparateSprite(const Aet::Video& video) const;

		void ImportAllCompositions();

	protected:
		void ImportVideo(const Aet::Video& video);

		void ImportPlaceholderVideo(const Aet::Video& video);
		void ImportSpriteVideo(const Aet::Video& video, bool dbVideo = false);
		void ImportVideoAddItemToProject(const Aet::Video& video, bool dbVideo = false);
		void ImportVideoSetSprIDComment(const Aet::Video& video);
		void ImportVideoSetSequenceInterpretation(const Aet::Video& video);
		void ImportVideoSetItemName(const Aet::Video& video);

		void ImportAudio(const Aet::Audio& audio);

	protected:
		void ImportSceneComps();
		void ImportComposition(const Aet::Composition& comp, bool videoDB = false);
		void ImportAllLayersInComp(const Aet::Composition& comp);
		void ImportLayer(const Aet::Composition& parentComp, const Aet::Layer& layer);
		void ImportLayerItemToComp(const Aet::Composition& parentComp, const Aet::Layer& layer);

		static std::unordered_map<const Aet::Composition*, frame_t> CreateGivenCompDurationsMap(const Aet::Scene& scene);

		static frame_t FindLongestLayerEndFrameInComp(const Aet::Composition& comp);
		static bool LayerMakesUseOfStartOffset(const Aet::Layer& layer);

		void ImportLayerVideo(const Aet::Layer& layer, AetExtraData& layerExtraData);
		void ImportLayerTransferMode(const Aet::Layer& layer, const Aet::LayerTransferMode& transferMode);
		void ImportLayerVideoStream(const Aet::Layer& layer, const Aet::LayerVideo& layerVideo);

		struct KeyFrameVec3 { frame_t Frame; vec3 Value; float Curve; };
		std::vector<KeyFrameVec3> combinedKeyFramesCache;

		void CombinePropertyKeyFrames(const Aet::Property1D* propertyX, const Aet::Property1D* propertyY, const Aet::Property1D* propertyZ, std::vector<KeyFrameVec3>& outCombinedKeyFrames) const;

		std::string_view GetLayerItemName(const Aet::Layer& layer) const;

		void ImportLayerAudio(const Aet::Layer& layer, AetExtraData& layerExtraData);
		void ImportLayerTiming(const Aet::Layer& layer, AetExtraData& layerExtraData);
		void ImportLayerName(const Aet::Layer& layer, AetExtraData& layerExtraData);
		void ImportLayerFlags(const Aet::Layer& layer, AetExtraData& layerExtraData);
		void ImportLayerQuality(const Aet::Layer& layer, AetExtraData& layerExtraData);
		void ImportLayerMarkers(const Aet::Layer& layer, AetExtraData& layerExtraData);

		void SetLayerRefParentLayer(const Aet::Layer& layer);
	};
}
