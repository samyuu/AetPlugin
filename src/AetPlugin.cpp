#include "AetPlugin.h"
#include "AetImport.h"
#include "Misc/StringHelper.h"
#include "FileSystem/FileHelper.h"

namespace AetPlugin
{
	AEGP_PluginID GlobalPluginID = -1;
	SPBasicSuite* GlobalBasicPicaSuite = nullptr;

	namespace
	{
		constexpr std::array AetSetFileExtensions =
		{
			std::array { 'b', 'i', 'n' },
			std::array { 'a', 'e', 'c' },
		};

		A_Err AEGP_FileVerifyCallbackHandler(const A_UTF16Char* filePath, AE_FIM_Refcon refcon, A_Boolean* a_canImport)
		{
			const auto verifyResult = AetImporter::VerifyAetSetImportable(AEUtil::WCast(filePath));
			*a_canImport = (verifyResult == AetImporter::AetSetVerifyResult::Valid);

			return A_Err_NONE;
		}

		A_Err AEGP_FileImportCallbackHandler(const A_UTF16Char* filePath, AE_FIM_ImportOptions importOptions, AE_FIM_SpecialAction action, AEGP_ItemH itemHandle, AE_FIM_Refcon refcon)
		{
			const auto filePathString = std::wstring(AEUtil::WCast(filePath));
			const auto aetSet = AetImporter::LoadAetSet(filePathString);

			if (aetSet == nullptr || aetSet->GetScenes().empty())
				return A_Err_GENERIC;

			auto importer = AetImporter(FileSystem::GetDirectory(filePathString));
			A_Err error = importer.ImportAetSet(*aetSet, importOptions, action, itemHandle);

			return error;
		}

		A_Err RegisterAetSetFileType()
		{
			const auto suites = AEGP_SuiteHandler(GlobalBasicPicaSuite);

			std::array<AEIO_FileKind, 1> fileTypes = {};
			for (auto& fileType : fileTypes)
			{
				fileType.mac.type = AEIO_FileType_GENERIC;
				fileType.mac.creator = AEIO_ANY_CREATOR;
			}

			std::array<AEIO_FileKind, AetSetFileExtensions.size()> fileExtensions = {};
			for (size_t i = 0; i < AetSetFileExtensions.size(); i++)
			{
				fileExtensions[i].ext.pad = '.';
				fileExtensions[i].ext.extension[0] = AetSetFileExtensions[i][0];
				fileExtensions[i].ext.extension[1] = AetSetFileExtensions[i][1];
				fileExtensions[i].ext.extension[2] = AetSetFileExtensions[i][2];
			}

			AE_FIM_ImportFlavorRef importFlavorRef = AE_FIM_ImportFlavorRef_NONE;

			A_Err err = A_Err_NONE;
			ERR(suites.FIMSuite3()->AEGP_RegisterImportFlavor("Project DIVA AetSet", &importFlavorRef));
			ERR(suites.FIMSuite3()->AEGP_RegisterImportFlavorFileTypes(importFlavorRef,
				static_cast<A_long>(fileTypes.size()), fileTypes.data(),
				static_cast<A_long>(fileExtensions.size()), fileExtensions.data()));

			if (err || importFlavorRef == AE_FIM_ImportFlavorRef_NONE)
				return err;

			AE_FIM_ImportCallbacks importCallbacks = {};
			importCallbacks.refcon = nullptr;
			importCallbacks.import_cb = AEGP_FileImportCallbackHandler;
			importCallbacks.verify_cb = AEGP_FileVerifyCallbackHandler;

			ERR(suites.FIMSuite3()->AEGP_RegisterImportFlavorImportCallbacks(importFlavorRef, AE_FIM_ImportFlag_COMP, &importCallbacks));
			return err;
		}
	}
}

A_Err EntryPointFunc(SPBasicSuite* pica_basicP, A_long major_versionL, A_long minor_versionL, AEGP_PluginID aegp_plugin_id, AEGP_GlobalRefcon* global_refconP)
{
	AetPlugin::GlobalBasicPicaSuite = pica_basicP;
	AetPlugin::GlobalPluginID = aegp_plugin_id;

	const auto error = AetPlugin::RegisterAetSetFileType();
	return error;
}
