#include "AetPlugin.h"
#include "AetImport.h"
#include "AetExport.h"
#include "FileDialogUtil.h"
#include "FileSystem/FileHelper.h"
#include "Misc/StringHelper.h"
#include <ctime>

namespace AetPlugin
{
	PluginStateData EvilGlobalState = {};

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

			A_Err err = A_Err_NONE;

			auto importer = AetImporter(FileSystem::GetDirectory(filePathString));
			ERR(importer.ImportAetSet(*aetSet, importOptions, action, itemHandle));

			return err;
		}

		A_Err RegisterAetSetFileTypeImport(const SuitesData& suites)
		{
			std::array<AEIO_FileKind, 1> fileTypes = {};
			for (auto& fileType : fileTypes)
			{
				fileType.mac.type = 'AETC';
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

		A_Err AEGP_UpdateMenuHook(AEGP_GlobalRefcon plugin_refconPV, AEGP_UpdateMenuRefcon refconPV, AEGP_WindowType active_window)
		{
			if (!EvilGlobalState.ExportAetSetCommand)
				return A_Err_NONE;

			const SuitesData suites;

			A_Err err = A_Err_NONE;
			if (AetExporter::IsProjectExportable(suites))
				ERR(suites.CommandSuite1->AEGP_EnableCommand(EvilGlobalState.ExportAetSetCommand));
			else
				ERR(suites.CommandSuite1->AEGP_DisableCommand(EvilGlobalState.ExportAetSetCommand));
			return err;
		}

		struct ExportOptions
		{
			struct DatabaseData
			{
				bool ExportSprDB = true;
				bool ExportAetDB = true;
			} Database;
			struct SpriteData
			{
				bool ExportSprSet = true;
				bool HashSprIDs = false;
				bool IncludeAll = false;
			} Sprite;
			struct LogData
			{
				bool WriteInfoLog = true;
				bool WriteWarningLog = true;
				bool WriteErrorLog = true;
			} Log;
			struct MiscData
			{
				bool ExportAetSet = true;
			} Misc;
		};

		std::pair<std::wstring, ExportOptions> OpenExportAetSetFileDialog(std::string_view fileName)
		{
			ExportOptions options = {};

			FileDialogUtil::SaveFileDialogInput dialog = {};
			dialog.FileName = fileName;
			dialog.DefaultExtension = "bin";
			dialog.Filters = { { "Project DIVA AetSet (*.bin)", "*.bin" }, };
			dialog.ParentWindowHandle = EvilGlobalState.MainWindowHandle;
			dialog.CustomizeItems =
			{
				{ FileDialogUtil::Customize::ItemType::VisualGroupStart, "Database" },
				{ FileDialogUtil::Customize::ItemType::Checkbox, "Export Spr DB", &options.Database.ExportSprDB },
				{ FileDialogUtil::Customize::ItemType::Checkbox, "Export Aet DB", &options.Database.ExportAetDB },
				{ FileDialogUtil::Customize::ItemType::VisualGroupEnd, "---" },

				{ FileDialogUtil::Customize::ItemType::VisualGroupStart, "Misc" },
				{ FileDialogUtil::Customize::ItemType::Checkbox, "Export AetSet", &options.Misc.ExportAetSet },
				{ FileDialogUtil::Customize::ItemType::VisualGroupEnd, "---" },

				{ FileDialogUtil::Customize::ItemType::VisualGroupStart, "Sprite" },
				{ FileDialogUtil::Customize::ItemType::Checkbox, "Export Spr Set", &options.Sprite.ExportSprSet },
				{ FileDialogUtil::Customize::ItemType::Checkbox, "Hash Spr IDs", &options.Sprite.HashSprIDs },
				{ FileDialogUtil::Customize::ItemType::Checkbox, "Include All Footage", &options.Sprite.IncludeAll },
				{ FileDialogUtil::Customize::ItemType::VisualGroupEnd, "---" },

				{ FileDialogUtil::Customize::ItemType::VisualGroupStart, "Log" },
				{ FileDialogUtil::Customize::ItemType::Checkbox, "Write Info Log", &options.Log.WriteInfoLog },
				{ FileDialogUtil::Customize::ItemType::Checkbox, "Write Warning Log", &options.Log.WriteWarningLog },
				{ FileDialogUtil::Customize::ItemType::Checkbox, "Write Error Log", &options.Log.WriteErrorLog },
				{ FileDialogUtil::Customize::ItemType::VisualGroupEnd, "---" },
			};

			const auto result = FileDialogUtil::OpenDialog(dialog);
			return std::make_pair(dialog.OutFilePath, options);
		}

		std::pair<FILE*, LogLevel> OpenAetSetExportLogFile(std::wstring_view directory, std::wstring_view setName, const ExportOptions& options)
		{
			FILE* logStream = nullptr;
			LogLevel logLevel = LogLevel_None;

			if (options.Log.WriteInfoLog)
				logLevel |= LogLevel_Info;
			if (options.Log.WriteWarningLog)
				logLevel |= LogLevel_Warning;
			if (options.Log.WriteErrorLog)
				logLevel |= LogLevel_Error;

			if (logLevel != LogLevel_None)
			{
				__time64_t currentTime = {};
				_time64(&currentTime);

				wchar_t logFilePath[AEGP_MAX_PATH_SIZE] = {};
				_swprintf(logFilePath, L"%.*s\\%.*s_%I64d.log",
					static_cast<int>(directory.size()),
					directory.data(),
					static_cast<int>(setName.size()),
					setName.data(),
					static_cast<int64_t>(currentTime));

				const errno_t result = _wfopen_s(&logStream, logFilePath, L"w");
			}

			return std::make_pair(logStream, logLevel);
		}

		void CloseAetSetExportLogFile(FILE* logFile)
		{
			if (logFile != nullptr)
				fclose(logFile);
		}

		A_Err AEGP_CommandHook(AEGP_GlobalRefcon plugin_refconPV, AEGP_CommandRefcon refconPV, AEGP_Command command, AEGP_HookPriority hook_priority, A_Boolean already_handledB, A_Boolean* handledPB)
		{
			*handledPB = false;
			if (command != EvilGlobalState.ExportAetSetCommand)
				return A_Err_NONE;

			*handledPB = true;

			auto exporter = AetExporter();

			const auto setNameU8 = exporter.GetAetSetNameFromProjectName();
			const auto setNameU16 = Utf8ToUtf16(setNameU8);

			const auto[outputFilePathU16, exportOptions] = OpenExportAetSetFileDialog(setNameU8);
			const auto outputFilePathU8 = Utf16ToUtf8(outputFilePathU16);

			const auto outputDirectoryU16 = FileSystem::GetDirectory(outputFilePathU16);
			const auto outputDirectoryU8 = Utf16ToUtf8(outputDirectoryU16);

			const auto setFileNameU8 = FileSystem::GetFileName(outputFilePathU8, true);

			if (outputFilePathU16.empty())
				return A_Err_NONE;

			auto[logFile, logLevel] = OpenAetSetExportLogFile(outputDirectoryU16, FormatUtil::StripPrefixIfExists(setNameU16, AetPrefixW), exportOptions);
			exporter.SetLog(logFile, logLevel);

			auto[aetSet, sprSetSrcInfo] = exporter.ExportAetSet(outputDirectoryU16);
			CloseAetSetExportLogFile(logFile);

			if (aetSet == nullptr)
				return A_Err_GENERIC;

			const auto dbBasePathU8 = outputDirectoryU8 + "\\" + std::string(FormatUtil::StripPrefixIfExists(setNameU8, AetPrefix)) + "_";

			if (exportOptions.Misc.ExportAetSet)
			{
				aetSet->Save(outputFilePathU16);
			}

			UniquePtr<SprSet> sprSet = nullptr;
			if (exportOptions.Sprite.ExportSprSet && sprSetSrcInfo != nullptr)
			{
				// TODO: Pack into farc
				sprSet = exporter.CreateSprSetFromSprSetSrcInfo(*sprSetSrcInfo, *aetSet);
				sprSet->Save(outputDirectoryU8 + "\\spr_farc\\spr_" + std::string(FormatUtil::StripPrefixIfExists(setFileNameU8, AetPrefix)));
			}

			if (exportOptions.Database.ExportSprDB)
			{
				auto sprDB = exporter.CreateSprDBFromAetSet(*aetSet, setFileNameU8, sprSet.get());
				sprDB.Save(dbBasePathU8 + "spr_db.bin");
			}
			if (exportOptions.Database.ExportAetDB)
			{
				auto aetDB = exporter.CreateAetDBFromAetSet(*aetSet, setFileNameU8);
				aetDB.Save(dbBasePathU8 + "aet_db.bin");
			}

			return A_Err_NONE;
		}

		A_Err RegisterAetSetFileTypeExport(const SuitesData& suites)
		{
			A_Err err = A_Err_NONE;

			// TODO: Menu item for exporting databases (aet_db + spr_db) containing only the info for this AetSet (?)
			// TODO: Menu item for setting all (or only the missing) SprID comments (to their murmur hash) (?)
			// TODO: On export optionally (?) write log file and or show message of all the aep unsupported data that could not be exported (effects, text, etc.) (?)
			// TODO: Menu item for checking supported features instead of doing so on export (?)
			// TODO: Menu item for collpasing all compositions as required by the format (?)
			// TODO: Menu item to export spr (RGBA8 at first)
			// TODO: Menu item to create an AetSet skeleton (+ add scenes)

			ERR(suites.CommandSuite1->AEGP_GetUniqueCommand(&EvilGlobalState.ExportAetSetCommand));
			ERR(suites.CommandSuite1->AEGP_InsertMenuCommand(EvilGlobalState.ExportAetSetCommand, "Export Project DIVA AetSet...", AEGP_Menu_EXPORT, AEGP_MENU_INSERT_SORTED));

			ERR(suites.RegisterSuite5->AEGP_RegisterCommandHook(EvilGlobalState.PluginID, AEGP_HP_BeforeAE, AEGP_Command_ALL, AEGP_CommandHook, nullptr));
			ERR(suites.RegisterSuite5->AEGP_RegisterUpdateMenuHook(EvilGlobalState.PluginID, AEGP_UpdateMenuHook, nullptr));

			return err;
		}

		A_Err RegisterAetSetFileType(const SuitesData& suites)
		{
			A_Err err = A_Err_NONE;
			ERR(RegisterAetSetFileTypeImport(suites));
			ERR(RegisterAetSetFileTypeExport(suites));
			return err;
		}

		A_Err AEGP_DeathHook(AEGP_GlobalRefcon unused1, AEGP_DeathRefcon unused2)
		{
			try
			{
				const AetPlugin::SuitesData suites;
				A_Err err = A_Err_NONE;

				struct MemStats
				{
					A_long Count;
					A_long Size;
				} memStats = {};

				ERR(suites.MemorySuite1->AEGP_GetMemStats(EvilGlobalState.PluginID, &memStats.Count, &memStats.Size));

				if (memStats.Count > 0 || memStats.Size > 0)
				{
					char infoBuffer[AEGP_MAX_ABOUT_STRING_SIZE];
					sprintf_s(infoBuffer, __FUNCTION__"(): Leaked %d allocations, for a total of %d kbytes", memStats.Count, memStats.Size);

					ERR(suites.UtilitySuite3->AEGP_ReportInfo(EvilGlobalState.PluginID, infoBuffer));
				}

				return err;
			}
			catch (const A_Err thrownErr)
			{
				return thrownErr;
			}
		}

		A_Err RegisterDebugDeathHook(const SuitesData& suites)
		{
			A_Err err = A_Err_NONE;
#if _DEBUG
			ERR(suites.RegisterSuite5->AEGP_RegisterDeathHook(EvilGlobalState.PluginID, AEGP_DeathHook, nullptr));
#endif /* _DEBUG */
			return err;
		}

		A_Err SetDebugMemoryReporting(const SuitesData& suites)
		{
			A_Err err = A_Err_NONE;
#if _DEBUG
			ERR(suites.MemorySuite1->AEGP_SetMemReportingOn(true));
#endif /* _DEBUG */
			return err;
		}

		A_Err DebugOnStartup(const SuitesData& suites)
		{
			// OpenExportAetSetFileDialog("aet_startup_test");
			return A_Err_NONE;
		}
	}
}

A_Err EntryPointFunc(SPBasicSuite* pica_basicP, A_long major_versionL, A_long minor_versionL, AEGP_PluginID aegp_plugin_id, AEGP_GlobalRefcon* global_refconP)
{
	AetPlugin::EvilGlobalState.BasicPicaSuite = pica_basicP;
	AetPlugin::EvilGlobalState.PluginID = aegp_plugin_id;

	const AetPlugin::SuitesData suites;

	A_Err err = A_Err_NONE;
	ERR(suites.UtilitySuite3->AEGP_GetMainHWND(&AetPlugin::EvilGlobalState.MainWindowHandle));
	ERR(AetPlugin::RegisterDebugDeathHook(suites));
	ERR(AetPlugin::RegisterAetSetFileType(suites));
	ERR(AetPlugin::SetDebugMemoryReporting(suites));
	ERR(AetPlugin::DebugOnStartup(suites));
	return err;
}
