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
		A_Err AEGP_FileVerifyCallbackHandler(const A_UTF16Char* filePath, AE_FIM_Refcon refcon, A_Boolean* a_canImport)
		{
			bool canImport = false;
			const auto error = AetImporter::VerifyAetSetImportable(AEUtil::WCast(filePath), canImport);

			*a_canImport = canImport;
			return error;
		}

		A_Err AEGP_FileImportCallbackHandler(const A_UTF16Char* filePath, AE_FIM_ImportOptions importOptions, AE_FIM_SpecialAction action, AEGP_ItemH itemHandle, AE_FIM_Refcon refcon)
		{
			const std::wstring filePathString = { AEUtil::WCast(filePath) };
			const auto aetSet = AetImporter::LoadAetSet(filePathString);

			if (aetSet == nullptr || aetSet->GetScenes().empty())
				return A_Err_GENERIC;

			AetImporter importer = AetImporter(FileSystem::GetDirectory(filePathString));
			const auto error = importer.ImportAetSet(*aetSet, importOptions, action, itemHandle);

			return error;
		}

		A_Err RegisterAetFileType()
		{
			constexpr std::array extensionsToRegister =
			{
				std::array { 'b', 'i', 'n' },
				std::array { 'a', 'e', 'c' },
			};

			A_Err err = A_Err_NONE;
			for (const auto& extension : extensionsToRegister)
			{
				AEIO_FileKind kind = {};
				std::memcpy(&kind.mac.type, "aet", 3);
				kind.mac.creator = AEIO_ANY_CREATOR;

				AEIO_FileKind fileKind = {};
				fileKind.ext.pad = '.';
				fileKind.ext.extension[0] = extension[0];
				fileKind.ext.extension[1] = extension[1];
				fileKind.ext.extension[2] = extension[2];

				AEGP_SuiteHandler suites = { GlobalBasicPicaSuite };
				AE_FIM_ImportFlavorRef importFlavorRef = AE_FIM_ImportFlavorRef_NONE;

				ERR(suites.FIMSuite3()->AEGP_RegisterImportFlavor("XXX Project DIVA Aet", &importFlavorRef));
				ERR(suites.FIMSuite3()->AEGP_RegisterImportFlavorFileTypes(importFlavorRef, 1, &kind, 1, &fileKind));

				if (err || importFlavorRef == AE_FIM_ImportFlavorRef_NONE)
					return err;

				AE_FIM_ImportCallbacks importCallbacks = {};
				importCallbacks.refcon = nullptr;
				importCallbacks.import_cb = AEGP_FileImportCallbackHandler;
				importCallbacks.verify_cb = AEGP_FileVerifyCallbackHandler;

				ERR(suites.FIMSuite3()->AEGP_RegisterImportFlavorImportCallbacks(importFlavorRef, AE_FIM_ImportFlag_COMP, &importCallbacks));
			}
			return err;
		}
	}
}

A_Err EntryPointFunc(SPBasicSuite* pica_basicP, A_long major_versionL, A_long minor_versionL, AEGP_PluginID aegp_plugin_id, AEGP_GlobalRefcon* global_refconP)
{
	AetPlugin::GlobalBasicPicaSuite = pica_basicP;
	AetPlugin::GlobalPluginID = aegp_plugin_id;

	const auto error = AetPlugin::RegisterAetFileType();
	return error;
}
