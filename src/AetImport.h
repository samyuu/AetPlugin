#pragma once
#include "AetPlugin.h"
#include "Graphics/Auth2D/Aet/AetSet.h"
#include "Database/AetDB.h"
#include "Database/SprDB.h"
#include <unordered_map>

namespace AetPlugin
{
	using namespace Comfy;
	using namespace Comfy::Graphics;

	struct SpriteFileData
	{
		static constexpr std::string_view PngExtension = ".png";
		static constexpr std::string_view SprPrefix = "spr_";

		std::string SanitizedFileName;
		std::wstring FilePath;
	};

	class AetImporter : NonCopyable
	{
	public:
		static UniquePtr<Aet::AetSet> LoadAetSet(std::wstring_view filePath);

		enum class AetSetVerifyResult
		{
			Valid,
			InvalidPath,
			InvalidFile,
			InvalidPointer,
			InvalidCount,
			InvalidData,
		};

		static AetSetVerifyResult VerifyAetSetImportable(std::wstring_view filePath);

	public:
		AetImporter(std::wstring_view workingDirectory);
		~AetImporter() = default;

		A_Err ImportAetSet(Aet::AetSet& set, AE_FIM_ImportOptions importOptions, AE_FIM_SpecialAction action, AEGP_ItemH itemHandle);

	protected:
		SuitesData suites;

	protected:
		struct WorkingDirectoryData
		{
			std::wstring ImportDirectory;
			std::vector<SpriteFileData> AvailableSpriteFiles;

			// NOTE: Optional to deal with hardcode expected IDs
			struct DatabaseData
			{
				UniquePtr<Database::AetDB> AetDB = nullptr;
				UniquePtr<Database::SprDB> SprDB = nullptr;
				const Database::AetSetEntry* AetSetEntry = nullptr;
				const Database::SprSetEntry* SprSetEntry = nullptr;
			} DB;

		} workingDirectory;

		const SpriteFileData* FindMatchingSpriteFile(std::string_view sourceName) const;

	protected:
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
					AEGP_ItemH Audio;
					AEGP_ItemH Comp;
				} Scene;
			} Folders;

		} project = {};

		void GetProjectHandles();
		void CreateProjectFolders();

	protected:
		void CreateSceneFolders();
		void ImportAllFootage();
		void ImportAllCompositions();

	protected:
		void ImportVideo(const Aet::Video& video);

		void ImportPlaceholderVideo(const Aet::Video& video);
		void ImportSpriteVideo(const Aet::Video& video);
		void ImportVideoAddItemToProject(const Aet::Video& video);
		void ImportVideoSetSprIDComment(const Aet::Video& video);
		void ImportVideoSetSequenceInterpretation(const Aet::Video& video);
		void ImportVideoSetItemName(const Aet::Video& video);

		void ImportAudio(const Aet::Audio& audio);

	protected:
		void ImportSceneComps();
		void ImportLayersInComp(const Aet::Composition& comp);
		void ImportLayer(const Aet::Composition& parentComp, const Aet::Layer& layer);
		void ImportLayerItemToComp(const Aet::Composition& parentComp, const Aet::Layer& layer);

		static std::unordered_map<const Aet::Composition*, frame_t> CreateGivenCompDurationsMap(const Aet::Scene& scene);
		static bool LayerUsesStartOffset(const Aet::Layer& layer);

		void ImportLayerVideo(const Aet::Layer& layer);
		void ImportLayerTransferMode(const Aet::Layer& layer, const Aet::LayerTransferMode& transferMode);
		void ImportLayerVideoStream(const Aet::Layer& layer, const Aet::LayerVideo& layerVideo);

		struct KeyFrameVec2 { frame_t Frame; vec2 Value; float Curve; };
		std::vector<KeyFrameVec2> combinedVec2KeyFramesCache;

		void CombineXYPropertiesToKeyFrameVec2s(const Aet::Property1D& propertyX, const Aet::Property1D& propertyY, std::vector<KeyFrameVec2>& outCombinedKeyFrames) const;

		std::string_view GetLayerItemName(const Aet::Layer& layer) const;

		void ImportLayerAudio(const Aet::Layer& layer);
		void ImportLayerTiming(const Aet::Layer& layer);
		void ImportLayerName(const Aet::Layer& layer);
		void ImportLayerFlags(const Aet::Layer& layer);
		void ImportLayerQuality(const Aet::Layer& layer);
		void ImportLayerMarkers(const Aet::Layer& layer);

		void SetLayerRefParentLayer(const Aet::Layer& layer);
	};
}
