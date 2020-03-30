#include "AetPlugin.h"
#include "AetImport.h"
#include "Misc/StringHelper.h"
#include "FileSystem/FileHelper.h"

AEGP_PluginID GlobalPluginID = -1;
SPBasicSuite* GlobalBasicPicaSuite = nullptr;

using namespace Comfy;
using namespace Comfy::Graphics;

namespace
{
	inline const wchar_t* WideStr(const A_UTF16Char* value)
	{
		return reinterpret_cast<const wchar_t*>(value);
	}

	A_Err NonCancerCanImportFile(const A_UTF16Char* filePath, AE_FIM_Refcon refcon, A_Boolean* a_canImport)
	{
		bool canImport = false;
		const auto error = VerifyAetSetImportable(WideStr(filePath), canImport);

		*a_canImport = canImport;
		return error;
	}

	A_Err NonCancerImportTest(const A_UTF16Char* filePath, AE_FIM_ImportOptions importOptions, AE_FIM_SpecialAction action, AEGP_ItemH itemHandle, AE_FIM_Refcon refcon)
	{
		const std::wstring filePathString = { WideStr(filePath) };

		SetWorkingAetImportDirectory(std::wstring(FileSystem::GetDirectory(filePathString)));

		Aet::AetSet aetSet;
		aetSet.Name = Utilities::Utf16ToUtf8(FileSystem::GetFileName(filePathString, false));
		aetSet.Load(filePathString);

		const auto error = ImportAetSet(aetSet, importOptions, action, itemHandle);
		return error;
	}

	A_Err RegisterAetFileType()
	{
		AEIO_FileKind kind = {};
		memcpy(&kind.mac.type, "aet", 3);
		kind.mac.creator = AEIO_ANY_CREATOR;

		AEIO_FileKind fileKind = {};
		fileKind.ext.pad = '.';
		fileKind.ext.extension[0] = 'b';
		fileKind.ext.extension[1] = 'i';
		fileKind.ext.extension[2] = 'n';

		AEGP_SuiteHandler suites = { GlobalBasicPicaSuite };
		AE_FIM_ImportFlavorRef importFlavorRef = AE_FIM_ImportFlavorRef_NONE;

		A_Err err = A_Err_NONE;
		ERR(suites.FIMSuite3()->AEGP_RegisterImportFlavor("XXX Project DIVA Aet", &importFlavorRef));
		ERR(suites.FIMSuite3()->AEGP_RegisterImportFlavorFileTypes(importFlavorRef, 1, &kind, 1, &fileKind));

		if (err || importFlavorRef == AE_FIM_ImportFlavorRef_NONE)
			return err;

		AE_FIM_ImportCallbacks importCallbacks = {};
		importCallbacks.refcon = nullptr;
		importCallbacks.import_cb = NonCancerImportTest;
		importCallbacks.verify_cb = NonCancerCanImportFile;

		ERR(suites.FIMSuite3()->AEGP_RegisterImportFlavorImportCallbacks(importFlavorRef, AE_FIM_ImportFlag_COMP, &importCallbacks));
		return err;
	}
}

A_Err EntryPointFunc(struct SPBasicSuite* pica_basicP, A_long major_versionL, A_long minor_versionL, AEGP_PluginID aegp_plugin_id, AEGP_GlobalRefcon* global_refconP)
{
	GlobalBasicPicaSuite = pica_basicP;
	GlobalPluginID = aegp_plugin_id;

	const auto error = RegisterAetFileType();
	return error;
}
