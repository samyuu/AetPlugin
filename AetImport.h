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
		static A_Err VerifyAetSetImportable(std::wstring_view filePath, bool& canImport);

	public:
		AetImporter(std::wstring_view workingDirectory);
		~AetImporter() = default;

		A_Err ImportAetSet(Aet::AetSet& set, AE_FIM_ImportOptions importOptions, AE_FIM_SpecialAction action, AEGP_ItemH itemHandle);

	protected:
		struct SuitesData
		{
			AEGP_SuiteHandler Handler = { GlobalBasicPicaSuite };

#define ConCat(a, b) a ## b
#define DeclareSuiteMember(suiteName) ConCat(AEGP_, suiteName)* suiteName = Handler.suiteName()

			DeclareSuiteMember(ProjSuite5);
			DeclareSuiteMember(ItemSuite8);
			DeclareSuiteMember(FootageSuite5);
			DeclareSuiteMember(CompSuite4);
			DeclareSuiteMember(LayerSuite1);
			DeclareSuiteMember(LayerSuite8);
			DeclareSuiteMember(StreamSuite5);
			DeclareSuiteMember(KeyframeSuite3);
			DeclareSuiteMember(UtilitySuite3);

#undef DeclareSuiteMember
#undef ConCat
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
			std::string SpriteNamePrefix, SpriteNamePrefixUnderscore;
		} workingAet;

		struct WorkingSceneData
		{
			float FrameRate = 0.0f;
			A_Ratio AE_FrameRate;
		} workingScene = {};

		void SetupWorkingAetSceneData(Aet::AetSet& set, const Aet::Scene& mainScene);
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
		void ImportAllFootage(const Aet::AetSet& set, const Aet::Scene& scene);
		void ImportAllCompositions(const Aet::AetSet& set, const Aet::Scene& scene);

		frame_t GetCompDuration(const Aet::Composition& comp) const;

	protected:
		void ImportVideo(const Aet::Video& video);
		
		void ImportPlaceholderVideo(const Aet::Video& video);
		void ImportSpriteVideo(const Aet::Video& video);

		void ImportAudio(const Aet::Audio& audio);

	protected:
		void ImportSceneComps(const Aet::AetSet& set, const Aet::Scene& scene);
		void ImportLayersInComp(const Aet::Composition& comp);
		void ImportLayer(const Aet::Composition& parentComp, const Aet::Layer& layer);

		void ImportLayerVideo(const Aet::Layer& layer);
		void ImportLayerTransferMode(const Aet::Layer& layer, const Aet::LayerTransferMode& transferMode);

		void ImportLayerAudio(const Aet::Layer& layer);
		void ImportLayerTiming(const Aet::Layer& layer);
		void ImportLayerName(const Aet::Layer& layer);
		void ImportLayerFlags(const Aet::Layer& layer);
		void ImportLayerQuality(const Aet::Layer& layer);
		void ImportLayerMarkers(const Aet::Layer& layer);
	};
}
