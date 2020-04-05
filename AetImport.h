#pragma once
#include "AetPlugin.h"
#include "Graphics/Auth2D/Aet/AetSet.h"

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
		struct SuitesData
		{
			AEGP_SuiteHandler Handler = { GlobalBasicPicaSuite };

#define DeclareSuiteMember(suiteName) ConCat(AEGP_, suiteName)* suiteName = Handler.suiteName()

			DeclareSuiteMember(ProjSuite5); // AE 10.0
			DeclareSuiteMember(ItemSuite1); // AE 5.0
			DeclareSuiteMember(FootageSuite5); // AE 10.0
			DeclareSuiteMember(CompSuite7); // AE 9.0
			DeclareSuiteMember(LayerSuite1); // AE 5.0
			DeclareSuiteMember(LayerSuite3); // AE 6.0
			DeclareSuiteMember(StreamSuite4); // AE 9
			DeclareSuiteMember(DynamicStreamSuite4); // AE 9.0
			DeclareSuiteMember(KeyframeSuite3); // AE 6.5
			DeclareSuiteMember(UtilitySuite3); // AE 10.0

#undef DeclareSuiteMember
		} suites;

	protected:
		struct WorkingDirectoryData
		{
			std::wstring ImportDirectory;
			std::vector<SpriteFileData> AvailableSpriteFiles;
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
			float FrameRate = 0.0f;
			A_Ratio AE_FrameRate;
		} workingScene = {};

		void SetupWorkingSetData(const Aet::AetSet& set);
		void SetupWorkingSceneData(const Aet::Scene& scene);
		void CheckWorkingDirectorySpriteFiles();

	protected:
		A_Time FrameToAETime(frame_t frame) const;
		frame_t AETimeToFrame(A_Time time) const;

	protected:
		struct ProjectData
		{
			AEGP_ProjectH ProjectHandle;
			AEGP_ItemH RootItemHandle;

			struct FolderHandles
			{
				AEGP_ItemH Root;
				AEGP_ItemH Data;
				AEGP_ItemH Video;
				AEGP_ItemH Audio;
				AEGP_ItemH Comp;
			} Folders;

		} project = {};

		void GetProjectHandles();
		void CreateProjectFolders();

	protected:
		void ImportAllFootage();
		void ImportAllCompositions();

		frame_t GetCompDuration(const Aet::Composition& comp) const;

	protected:
		void ImportVideo(const Aet::Video& video);

		void ImportPlaceholderVideo(const Aet::Video& video);
		void ImportSpriteVideo(const Aet::Video& video);

		void ImportAudio(const Aet::Audio& audio);

	protected:
		void ImportSceneComps();
		void ImportLayersInComp(const Aet::Composition& comp);
		void ImportLayer(const Aet::Composition& parentComp, const Aet::Layer& layer);
		void ImportLayerItemToComp(const Aet::Composition& parentComp, const Aet::Layer& layer);

		static bool LayerUsesStartOffset(const Aet::Layer& layer);

		void ImportLayerVideo(const Aet::Layer& layer);
		void ImportLayerTransferMode(const Aet::Layer& layer, const Aet::LayerTransferMode& transferMode);
		void ImportLayerVideoStream(const Aet::Layer& layer, const Aet::LayerVideo& layerVideo);

		struct KeyFrameVec2 { frame_t Frame; vec2 Value; float Curve; };
		std::vector<KeyFrameVec2> combinedVec2KeyFramesCache;

		void CombineXYPropertiesToKeyFrameVec2s(const Aet::Property1D& propertyX, const Aet::Property1D& propertyY, std::vector<KeyFrameVec2>& outCombinedKeyFrames) const;

		void ImportLayerAudio(const Aet::Layer& layer);
		void ImportLayerTiming(const Aet::Layer& layer);
		void ImportLayerName(const Aet::Layer& layer);
		void ImportLayerFlags(const Aet::Layer& layer);
		void ImportLayerQuality(const Aet::Layer& layer);
		void ImportLayerMarkers(const Aet::Layer& layer);

		void SetLayerRefParentLayer(const Aet::Layer& layer);
	};
}
