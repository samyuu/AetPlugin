#include "AetPlugin.h"
#include "AetImport.h"
#include "AetExport.h"
#include <IO/File.h>
#include <IO/Path.h>
#include <IO/Shell.h>
#include <IO/Directory.h>
#include <IO/Archive/FArcPacker.h>
#include <Misc/StringUtil.h>
#include <Time/TimeUtilities.h>

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
			const auto verifyResult = AetImporter::VerifyAetSetImportable(UTF8::Narrow(AEUtil::WCast(filePath)));
			*a_canImport = (verifyResult == AetImporter::AetSetVerifyResult::Valid);

			return A_Err_NONE;
		}

		A_Err AEGP_FileImportCallbackHandler(const A_UTF16Char* filePath, AE_FIM_ImportOptions importOptions, AE_FIM_SpecialAction action, AEGP_ItemH itemHandle, AE_FIM_Refcon refcon)
		{
			const auto filePathString = UTF8::Narrow(AEUtil::WCast(filePath));
			const auto aetSet = AetImporter::LoadAetSet(filePathString);

			if (aetSet == nullptr || aetSet->GetScenes().empty())
				return A_Err_GENERIC;

			A_Err err = A_Err_NONE;

			auto importer = AetImporter(IO::Path::GetDirectoryName(filePathString));
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
				bool ParseSprIDComments = true;
			} Database;
			struct SpriteData
			{
				bool ExportSprSet = true;
				bool PowerOfTwoTextures = true;
				// TODO: 
				// bool CompressTextures = false;
				// bool GenerateMipMaps = false;
			} Sprite;
			struct LogData
			{
				bool WriteLog = false;
			} Log;
			struct MiscData
			{
				bool ExportAetSet = true;
				bool RunPostExportScript = true;
			} Misc;
		};

		std::pair<std::string, ExportOptions> OpenExportAetSetFileDialog(std::string_view fileName)
		{
			ExportOptions options = {};

			auto dialog = IO::Shell::FileDialog();
			dialog.FileName = fileName;
			dialog.DefaultExtension = "bin";
			dialog.Filters = { { "Project DIVA AetSet (*.bin)", "*.bin" }, };
			dialog.ParentWindowHandle = EvilGlobalState.MainWindowHandle;
			dialog.CustomizeItems =
			{
				{ IO::Shell::Custom::ItemType::VisualGroupStart, "Database" },
				{ IO::Shell::Custom::ItemType::Checkbox, "Export Spr DB", &options.Database.ExportSprDB },
				{ IO::Shell::Custom::ItemType::Checkbox, "Export Aet DB", &options.Database.ExportAetDB },
				{ IO::Shell::Custom::ItemType::Checkbox, "Spr ID Comments", &options.Database.ParseSprIDComments },
				{ IO::Shell::Custom::ItemType::VisualGroupEnd, "---" },

				{ IO::Shell::Custom::ItemType::VisualGroupStart, "Sprite" },
				{ IO::Shell::Custom::ItemType::Checkbox, "Export Spr Set", &options.Sprite.ExportSprSet },
				{ IO::Shell::Custom::ItemType::Checkbox, "Compress Textures", nullptr /*&options.Sprite.CompressTextures*/ },
				{ IO::Shell::Custom::ItemType::Checkbox, "Power of Two Textures", &options.Sprite.PowerOfTwoTextures },
				{ IO::Shell::Custom::ItemType::VisualGroupEnd, "---" },

				{ IO::Shell::Custom::ItemType::VisualGroupStart, "Log" },
				{ IO::Shell::Custom::ItemType::Checkbox, "Write Log", &options.Log.WriteLog },
				{ IO::Shell::Custom::ItemType::VisualGroupEnd, "---" },
			};

			const auto result = dialog.OpenSave();
			return std::make_pair(dialog.OutFilePath, options);
		}

		std::pair<FILE*, LogLevel> OpenAetSetExportLogFile(std::string_view directory, std::string_view setName, const ExportOptions& options)
		{
			FILE* logStream = nullptr;
			LogLevel logLevel = LogLevel_None;

			if (options.Log.WriteLog)
				logLevel |= LogLevel_All;

			if (logLevel != LogLevel_None)
			{
				__time64_t currentTime = {};
				_time64(&currentTime);

				char logFilePath[AEGP_MAX_PATH_SIZE];
				sprintf(logFilePath, "%.*s\\%.*s_%I64d.log",
					static_cast<int>(directory.size()),
					directory.data(),
					static_cast<int>(setName.size()),
					setName.data(),
					static_cast<int64_t>(currentTime));

				const errno_t result = _wfopen_s(&logStream, UTF8::WideArg(logFilePath).c_str(), L"w");
			}

			return std::make_pair(logStream, logLevel);
		}

		void CloseAetSetExportLogFile(FILE* logFile)
		{
			if (logFile != nullptr)
				fclose(logFile);
		}

		A_Err ExportAetSetCommand()
		{
			auto exporter = AetExporter();

			const auto setName = exporter.GetAetSetNameFromProjectName();
			const auto[outputFilePath, exportOptions] = OpenExportAetSetFileDialog(setName);

			const auto outputDirectory = IO::Path::GetDirectoryName(outputFilePath);

			const auto setFileName = IO::Path::GetFileName(outputFilePath, true);
			const auto aetBaseName = std::string(IO::Path::TrimExtension(Util::StripPrefixInsensitive(setName, AetPrefix)));

			if (outputFilePath.empty())
				return A_Err_NONE;

			auto[logFile, logLevel] = OpenAetSetExportLogFile(outputDirectory, Util::StripPrefixInsensitive(setName, AetPrefix), exportOptions);
			exporter.SetLog(logFile, logLevel);

			auto[aetSet, sprSetSrcInfo] = exporter.ExportAetSet(outputDirectory, exportOptions.Database.ParseSprIDComments);
			CloseAetSetExportLogFile(logFile);

			if (aetSet == nullptr)
				return A_Err_GENERIC;

			if (exportOptions.Misc.ExportAetSet)
				IO::File::Save(outputFilePath, *aetSet);

			std::unique_ptr<SprSet> sprSet = nullptr;
			if (exportOptions.Sprite.ExportSprSet && sprSetSrcInfo != nullptr)
			{
				sprSet = exporter.CreateSprSetFromSprSetSrcInfo(*sprSetSrcInfo, *aetSet, exportOptions.Sprite.PowerOfTwoTextures);
				const auto sprName = ("spr_" + aetBaseName);

				if (sprSet != nullptr)
				{
					auto farcPacker = IO::FArcPacker();
					farcPacker.AddFile(IO::Path::ChangeExtension(sprName, ".bin"), *sprSet);
					farcPacker.CreateFlushFArc(IO::Path::Combine(outputDirectory, IO::Path::ChangeExtension(sprName, ".farc")));
				}
			}

			if (exportOptions.Database.ExportSprDB)
			{
				auto sprDB = exporter.CreateSprDBFromAetSet(*aetSet, setFileName, sprSet.get());
				IO::File::Save(IO::Path::Combine(outputDirectory, ("spr_db_" + aetBaseName + ".bin")), sprDB);
			}
			if (exportOptions.Database.ExportAetDB)
			{
				auto aetDB = exporter.CreateAetDBFromAetSet(*aetSet, setFileName);
				IO::File::Save(IO::Path::Combine(outputDirectory, ("aet_db_" + aetBaseName + ".bin")), aetDB);
			}

			return A_Err_NONE;
		}

		A_Err AEGP_CommandHook(AEGP_GlobalRefcon plugin_refconPV, AEGP_CommandRefcon refconPV, AEGP_Command command, AEGP_HookPriority hook_priority, A_Boolean already_handledB, A_Boolean* handledPB)
		{
			A_Err err = A_Err_NONE;

			if (command == EvilGlobalState.ExportAetSetCommand)
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

		A_Err RegisterAetSetFileTypeExport(const SuitesData& suites)
		{
			A_Err err = A_Err_NONE;

			// TODO: Menu item for collpasing all compositions as required by the format (?)
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
