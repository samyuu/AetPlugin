#include "aet_plugin_main.h"
#include "aet_plugin_import.h"
#include "aet_plugin_export.h"
#include "comfy/file_format_db.h"
#include "comfy/file_format_aet_set.h"
#include "comfy/file_format_spr_set.h"
#include "comfy/file_format_farc.h"
#include "comfy/texture_util.h"
#include "core_io.h"
#include <time.h>
#include <future>
#include <array>

namespace AetPlugin
{
	static constexpr struct { char V[4]; } AetSetFileExtensions[] =
	{
		// NOTE: Regular legacy format
		{ '.', 'b', 'i', 'n' },
		// NOTE: Modern format as stored inside a ".farc"
		{ '.', 'a', 'e', 'c' },
		// NOTE: FArc expected to contain a modern ".aec"
		{ 'f', 'a', 'r', 'c' },
	};

	static A_Err AEGP_FileVerifyCallbackHandler(const A_UTF16Char* filePath, AE_FIM_Refcon refcon, A_Boolean* a_canImport)
	{
		const auto verifyResult = AetImporter::VerifyAetSetImportable(UTF8::Narrow(AEUtil::WCast(filePath)));
		*a_canImport = (verifyResult == AetImporter::AetSetVerifyResult::Valid);

		return A_Err_NONE;
	}

	static A_Err AEGP_FileImportCallbackHandler(const A_UTF16Char* filePath, AE_FIM_ImportOptions importOptions, AE_FIM_SpecialAction action, AEGP_ItemH itemHandle, AE_FIM_Refcon refcon)
	{
		const auto aetSetPathOrFArc = UTF8::Narrow(AEUtil::WCast(filePath));
		const auto[aetSet, aetDB] = AetImporter::TryLoadAetSetAndDB(aetSetPathOrFArc);

		if (aetSet == nullptr || aetSet->Scenes.empty())
			return A_Err_GENERIC;

		const auto[sprSet, sprDB] = AetImporter::TryLoadSprSetAndDB(aetSetPathOrFArc);
		const auto workingDirectory = Path::GetDirectoryName(aetSetPathOrFArc);

		if (sprSet != nullptr)
			ExtractAllSprPNGs(workingDirectory, *sprSet);

		AetImporter importer = {};
		auto err = importer.ImportAetSet(workingDirectory, *aetSet, sprSet.get(), aetDB.get(), sprDB.get(), importOptions, action, itemHandle);

		return err;
	}

	static A_Err RegisterAetSetFileTypeImport(const SuitesData& suites)
	{
		std::array<AEIO_FileKind, 1> fileTypes = {};
		for (auto& fileType : fileTypes)
		{
			fileType.mac.type = 'AETC';
			fileType.mac.creator = AEIO_ANY_CREATOR;
		}

		std::array<AEIO_FileKind, ArrayCount(AetSetFileExtensions)> fileExtensions = {};
		for (size_t i = 0; i < ArrayCount(AetSetFileExtensions); i++)
		{
			fileExtensions[i].ext.pad = AetSetFileExtensions[i].V[0];
			fileExtensions[i].ext.extension[0] = AetSetFileExtensions[i].V[1];
			fileExtensions[i].ext.extension[1] = AetSetFileExtensions[i].V[2];
			fileExtensions[i].ext.extension[2] = AetSetFileExtensions[i].V[3];
		}

		AE_FIM_ImportFlavorRef importFlavorRef = AE_FIM_ImportFlavorRef_NONE;

		A_Err err = A_Err_NONE;
		ERR(suites.FIMSuite3->AEGP_RegisterImportFlavor("Project DIVA AetSet", &importFlavorRef));
		ERR(suites.FIMSuite3->AEGP_RegisterImportFlavorFileTypes(importFlavorRef,
			static_cast<A_long>(fileTypes.size()), fileTypes.data(),
			static_cast<A_long>(fileExtensions.size()), fileExtensions.data()));

		if (err || importFlavorRef == AE_FIM_ImportFlavorRef_NONE)
			return err;

		AE_FIM_ImportCallbacks importCallbacks = {};
		importCallbacks.refcon = nullptr;
		importCallbacks.import_cb = AEGP_FileImportCallbackHandler;
		importCallbacks.verify_cb = AEGP_FileVerifyCallbackHandler;

		ERR(suites.FIMSuite3->AEGP_RegisterImportFlavorImportCallbacks(importFlavorRef, AE_FIM_ImportFlag_COMP, &importCallbacks));
		return err;
	}

	static A_Err AEGP_UpdateMenuHook(AEGP_GlobalRefcon plugin_refconPV, AEGP_UpdateMenuRefcon refconPV, AEGP_WindowType active_window)
	{
		if (!Global.ExportAetSetCommand)
			return A_Err_NONE;

		const SuitesData suites;

		A_Err err = A_Err_NONE;
		if (AetExporter::IsProjectExportable(suites))
			ERR(suites.CommandSuite1->AEGP_EnableCommand(Global.ExportAetSetCommand));
		else
			ERR(suites.CommandSuite1->AEGP_DisableCommand(Global.ExportAetSetCommand));
		return err;
	}

	struct ExportOptions
	{
		// NOTE: Arbitrary buffer size to store at least the date time compile time ID
		using IDType = std::array<char, 128>;

		static constexpr cstr StorageValueKey = "ExportOptions";
		static constexpr IDType CompileTimeID = { "ExportOptions { { " __DATE__ " }, { " __TIME__ " } };" };

		// NOTE: To identify and compare the serialized object in memory
		size_t ThisStructSize = sizeof(ExportOptions);
		IDType ThisStructID = CompileTimeID;

		b8 Database_ExportSprDB = true;
		b8 Database_ExportAetDB = true;
		b8 Database_MDataDBs = true;
		b8 Database_MergeExisting = true;
		b8 Sprite_ExportSprSet = true;
		b8 Sprite_PowerOfTwoTextures = true;
		b8 Sprite_CompressTextures = true;
		b8 Sprite_EncodeYCbCr = true;
		b8 SpriteFArc_Compress = true;
		b8 Debug_WriteLog = false;
		b8 Misc_ExportAetSet = true;
	};

	static ExportOptions RetrieveLastUsedExportOptions(const SuitesData& suites)
	{
		A_Err err = A_Err_NONE;

		const ExportOptions defaultOptions = {};
		ExportOptions outOptions = {};

		AEGP_PersistentBlobH persistentBlob = {};
		ERR(suites.PersistentDataSuite3->AEGP_GetApplicationBlob(&persistentBlob));
		ERR(suites.PersistentDataSuite3->AEGP_GetData(persistentBlob, PersistentDataPluginSectionKey, ExportOptions::StorageValueKey, static_cast<A_u_long>(sizeof(ExportOptions)), &defaultOptions, &outOptions));
		if (err != A_Err_NONE)
			return defaultOptions;

		// NOTE: The perstistent data is stored as a raw object memory dump so any version that even has the potentail of having a different memory layout or version will be discarded.
		//		 This means previous options will always be invalidated after recompilation
		if (outOptions.ThisStructSize != sizeof(ExportOptions) || outOptions.ThisStructID != ExportOptions::CompileTimeID)
			return defaultOptions;

		return outOptions;
	}

	static void SaveLastUsedExportOptions(const SuitesData& suites, const ExportOptions& options)
	{
		A_Err err = A_Err_NONE;

		AEGP_PersistentBlobH persistentBlob = {};
		ERR(suites.PersistentDataSuite3->AEGP_GetApplicationBlob(&persistentBlob));
		ERR(suites.PersistentDataSuite3->AEGP_SetData(persistentBlob, PersistentDataPluginSectionKey, ExportOptions::StorageValueKey, static_cast<A_u_long>(sizeof(options)), &options));
	}

	struct ExportAetSetFileDialogResult { std::string OutputPath; ExportOptions Options; };
	static ExportAetSetFileDialogResult OpenExportAetSetFileDialog(const SuitesData& suites, std::string_view fileName)
	{
		ExportOptions options = RetrieveLastUsedExportOptions(suites);

		Shell::FileDialog dialog = {};
		dialog.InFileName = fileName;
		dialog.InDefaultExtension = "bin";
		dialog.InFilters = { { "Project DIVA AetSet", "*.bin" }, };
		dialog.InParentWindowHandle = Global.MainWindowHandle;
		dialog.InOutCustomizeItems =
		{
			{ Shell::FileDialogItemType::VisualGroupStart, "Database" },
			{ Shell::FileDialogItemType::Checkbox, "Export Spr DB", &options.Database_ExportSprDB },
			{ Shell::FileDialogItemType::Checkbox, "Export Aet DB", &options.Database_ExportAetDB },
			{ Shell::FileDialogItemType::Checkbox, "MData Prefix", &options.Database_MDataDBs },
			{ Shell::FileDialogItemType::Checkbox, "Merge Existing", &options.Database_MergeExisting },
			{ Shell::FileDialogItemType::VisualGroupEnd, "---" },

			{ Shell::FileDialogItemType::VisualGroupStart, "Sprite" },
			{ Shell::FileDialogItemType::Checkbox, "Export Spr Set", &options.Sprite_ExportSprSet },
			{ Shell::FileDialogItemType::Checkbox, "Compress Textures", &options.Sprite_CompressTextures },
			{ Shell::FileDialogItemType::Checkbox, "Encode YCbCr", &options.Sprite_EncodeYCbCr },
			{ Shell::FileDialogItemType::Checkbox, "Power of Two Textures", &options.Sprite_PowerOfTwoTextures },
			{ Shell::FileDialogItemType::VisualGroupEnd, "---" },

			{ Shell::FileDialogItemType::VisualGroupStart, "Sprite FArc" },
			{ Shell::FileDialogItemType::Checkbox, "Compress Content", &options.SpriteFArc_Compress },
			{ Shell::FileDialogItemType::VisualGroupEnd, "---" },

			{ Shell::FileDialogItemType::VisualGroupStart, "Debug" },
			{ Shell::FileDialogItemType::Checkbox, "Write Log File", &options.Debug_WriteLog },
			{ Shell::FileDialogItemType::VisualGroupEnd, "---" },
		};

		const Shell::FileDialogResult dialogResult = dialog.OpenSave();
		SaveLastUsedExportOptions(suites, options);

		return ExportAetSetFileDialogResult { std::move(dialog.OutFilePath), options };
	}

	struct LogAndLevel { FILE* Stream; LogLevelFlags Level; };
	static LogAndLevel OpenAetSetExportLogFile(std::string_view directory, std::string_view setName, const ExportOptions& options)
	{
		FILE* logStream = nullptr;
		LogLevelFlags logLevel = LogLevelFlags_None;

		if (options.Debug_WriteLog)
			logLevel |= LogLevelFlags_All;

		if (logLevel != LogLevelFlags_None)
		{
			__time64_t currentTime = {};
			_time64(&currentTime);

			char logFilePath[AEGP_MAX_PATH_SIZE];
			sprintf(logFilePath, "%.*s\\%.*s_%I64d.log", FmtStrViewArgs(directory), FmtStrViewArgs(setName), currentTime);

			const errno_t result = _wfopen_s(&logStream, UTF8::WideArg(logFilePath).c_str(), L"w");
		}

		return LogAndLevel { logStream, logLevel };
	}

	static std::unique_ptr<SprSet> TryExportSprSetFromAetSet(AetExporter& exporter, const ExportOptions& exportOptions, const SprSetSrcInfo* sprSetSrcInfo, const Aet::AetSet& aetSet)
	{
		if (!exportOptions.Sprite_ExportSprSet || sprSetSrcInfo == nullptr)
			return nullptr;

		auto sprSetOptions = AetExporter::SprSetExportOptions {};
		sprSetOptions.PowerOfTwo = exportOptions.Sprite_PowerOfTwoTextures;
		sprSetOptions.EnableCompression = exportOptions.Sprite_CompressTextures;
		sprSetOptions.EncodeYCbCr = exportOptions.Sprite_EncodeYCbCr;

		return exporter.CreateSprSetFromSprSetSrcInfo(*sprSetSrcInfo, aetSet, sprSetOptions);
	}

	template <typename DBType>
	static b8 TryMergeExistingDB(DBType& outNewDB, DBType* existingDB)
	{
		static_assert(std::is_same_v<DBType, AetDB> || std::is_same_v<DBType, SprDB>);

		if (outNewDB.Entries.empty() || existingDB == nullptr || existingDB->Entries.empty())
			return false;

		auto newSetEntry = std::move(outNewDB.Entries.front());
		const auto existingSetIndex = FindIndexOf(existingDB->Entries, [&](const auto& e) { return (e.ID == newSetEntry.ID || e.Name == newSetEntry.Name); });

		outNewDB.Entries = std::move(existingDB->Entries);

		if (InBounds(existingSetIndex, outNewDB.Entries))
			outNewDB.Entries[existingSetIndex] = std::move(newSetEntry);
		else
			outNewDB.Entries.emplace_back(std::move(newSetEntry));

		return true;
	}

	static A_Err ExportAetSetCommand()
	{
		AetExporter exporter = {};

		const ExportAetSetFileDialogResult fileDialog = OpenExportAetSetFileDialog(exporter.Suites(), exporter.GetAetSetNameFromProjectName());
		const std::string_view outputDirectory = Path::GetDirectoryName(fileDialog.OutputPath);
		if (fileDialog.OutputPath.empty())
			return A_Err_NONE;

		const auto aetSetName = Path::GetFileName(fileDialog.OutputPath, false);
		const auto aetSetFileName = Path::GetFileName(fileDialog.OutputPath, true);
		const auto setBaseName = std::string(ASCII::TrimPrefixInsensitive(aetSetName, AetPrefix));
		const auto sprSetName = ("spr_" + setBaseName);

		const LogAndLevel log = OpenAetSetExportLogFile(outputDirectory, ASCII::TrimPrefixInsensitive(aetSetName, AetPrefix), fileDialog.Options);
		defer { if (log.Stream != nullptr) fclose(log.Stream); };

		exporter.SetLog(log.Stream, log.Level);

		const auto[aetSet, sprSetSrcInfo] = exporter.ExportAetSet(outputDirectory, true);

		if (aetSet == nullptr)
			return A_Err_GENERIC;

		const auto aetSaveFuture = SaveFileAsync(fileDialog.OutputPath, (fileDialog.Options.Misc_ExportAetSet) ? aetSet.get() : nullptr);

		const auto sprSet = TryExportSprSetFromAetSet(exporter, fileDialog.Options, sprSetSrcInfo.get(), *aetSet);
		const auto sprSaveFuture = std::async(std::launch::async, [&]
		{
			if (sprSet == nullptr)
				return;

			FArcPacker farcPacker {};
			farcPacker.AddFile(sprSetName + ".bin", *sprSet);
			farcPacker.CreateFlushFArc(Path::Combine(outputDirectory, sprSetName + ".farc"), fileDialog.Options.SpriteFArc_Compress);
		});

		const auto dbNamePrefix = fileDialog.Options.Database_MDataDBs ? std::string("mdata_") : "";
		const auto dbNameSuffix = fileDialog.Options.Database_MDataDBs ? "" : ("_" + setBaseName);

		const auto sprDBPath = Path::Combine(outputDirectory, (dbNamePrefix + "spr_db" + dbNameSuffix + ".bin"));
		const auto aetDBPath = Path::Combine(outputDirectory, (dbNamePrefix + "aet_db" + dbNameSuffix + ".bin"));

		auto sprDB = (fileDialog.Options.Database_ExportSprDB) ? exporter.CreateSprDBFromAetSet(*aetSet, aetSetFileName, sprSet.get()) : nullptr;
		auto aetDB = (fileDialog.Options.Database_ExportAetDB) ? exporter.CreateAetDBFromAetSet(*aetSet, aetSetFileName) : nullptr;

		if (fileDialog.Options.Database_MergeExisting)
		{
			if (sprDB != nullptr) { TryMergeExistingDB(*sprDB, LoadFile<SprDB>(sprDBPath).get()); }
			if (aetDB != nullptr) { TryMergeExistingDB(*aetDB, LoadFile<AetDB>(aetDBPath).get()); }
		}

		const std::future<b8> dbSaveFutures[] =
		{
			SaveFileAsync(sprDBPath, sprDB.get()),
			SaveFileAsync(aetDBPath, aetDB.get()),
		};

		return A_Err_NONE;
	}

	static A_Err AEGP_CommandHook(AEGP_GlobalRefcon plugin_refconPV, AEGP_CommandRefcon refconPV, AEGP_Command command, AEGP_HookPriority hook_priority, A_Boolean already_handledB, A_Boolean* handledPB)
	{
		A_Err err = A_Err_NONE;

		if (command == Global.ExportAetSetCommand)
		{
			*handledPB = true;
			ERR(ExportAetSetCommand());
		}
		else
		{
			*handledPB = false;
		}

		return err;
	}

	static A_Err RegisterAetSetFileTypeExport(const SuitesData& suites)
	{
		A_Err err = A_Err_NONE;

		// TODO: Menu item for collpasing all compositions as required by the format (?)
		// TODO: Menu item to create an AetSet skeleton (+ add scenes)

		ERR(suites.CommandSuite1->AEGP_GetUniqueCommand(&Global.ExportAetSetCommand));
		ERR(suites.CommandSuite1->AEGP_InsertMenuCommand(Global.ExportAetSetCommand, "Export Project DIVA AetSet...", AEGP_Menu_EXPORT, AEGP_MENU_INSERT_SORTED));

		ERR(suites.RegisterSuite5->AEGP_RegisterCommandHook(Global.PluginID, AEGP_HP_BeforeAE, AEGP_Command_ALL, AEGP_CommandHook, nullptr));
		ERR(suites.RegisterSuite5->AEGP_RegisterUpdateMenuHook(Global.PluginID, AEGP_UpdateMenuHook, nullptr));

		return err;
	}

	static A_Err RegisterAetSetFileType(const SuitesData& suites)
	{
		A_Err err = A_Err_NONE;
		ERR(RegisterAetSetFileTypeImport(suites));
		ERR(RegisterAetSetFileTypeExport(suites));
		return err;
	}

	static A_Err AEGP_DeathHook(AEGP_GlobalRefcon unused1, AEGP_DeathRefcon unused2)
	{
		try
		{
			A_Err err = A_Err_NONE;
			struct MemStats { A_long Count, Size; } memStats = {};

			const AetPlugin::SuitesData suites {};
			ERR(suites.MemorySuite1->AEGP_GetMemStats(Global.PluginID, &memStats.Count, &memStats.Size));

			if (memStats.Count > 0 || memStats.Size > 0)
			{
				char infoBuffer[AEGP_MAX_ABOUT_STRING_SIZE];
				sprintf_s(infoBuffer, __FUNCTION__"(): Leaked %d allocations, for a total of %d kbytes", memStats.Count, memStats.Size);

				ERR(suites.UtilitySuite3->AEGP_ReportInfo(Global.PluginID, infoBuffer));
			}

			return err;
		}
		catch (const A_Err thrownErr)
		{
			return thrownErr;
		}
	}

	static A_Err RegisterDebugDeathHook(const SuitesData& suites)
	{
		A_Err err = A_Err_NONE;
#if _DEBUG
		ERR(suites.RegisterSuite5->AEGP_RegisterDeathHook(Global.PluginID, AEGP_DeathHook, nullptr));
#endif /* _DEBUG */
		return err;
	}

	static A_Err SetDebugMemoryReporting(const SuitesData& suites)
	{
		A_Err err = A_Err_NONE;
#if _DEBUG
		ERR(suites.MemorySuite1->AEGP_SetMemReportingOn(true));
#endif /* _DEBUG */
		return err;
	}

	static A_Err DebugOnStartup(const SuitesData& suites)
	{
		// OpenExportAetSetFileDialog("aet_startup_test");
		return A_Err_NONE;
	}
}

A_Err EntryPointFunc(SPBasicSuite* pica_basicP, A_long major_versionL, A_long minor_versionL, AEGP_PluginID aegp_plugin_id, AEGP_GlobalRefcon* global_refconP)
{
	AetPlugin::Global.BasicPicaSuite = pica_basicP;
	AetPlugin::Global.PluginID = aegp_plugin_id;

	const AetPlugin::SuitesData suites {};
	A_Err err = A_Err_NONE;
	ERR(suites.UtilitySuite3->AEGP_GetMainHWND(&AetPlugin::Global.MainWindowHandle));
	ERR(AetPlugin::RegisterDebugDeathHook(suites));
	ERR(AetPlugin::RegisterAetSetFileType(suites));
	ERR(AetPlugin::SetDebugMemoryReporting(suites));
	ERR(AetPlugin::DebugOnStartup(suites));
	return err;
}
