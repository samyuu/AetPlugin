#include "AetPlugin.h"
#include "AetImport.h"
#include "AetExport.h"
#include <IO/File.h>
#include <IO/Path.h>
#include <IO/Shell.h>
#include <IO/Directory.h>
#include <IO/Archive/FArcPacker.h>
#include <Misc/StringUtil.h>
#include <Graphics/Utilities/SpriteExtraction.h>
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

		std::unique_ptr<SprSet> TryLoadAetSetMatchingSprSet(std::string_view aetSetPath, std::string_view workingDirectory)
		{
			// NOTE: "aet_dummy.bin" -> "dummy"
			const auto baseFileName = Util::StripPrefixInsensitive(IO::Path::GetFileName(aetSetPath, false), AetPrefix);
			// NOTE: "dummy" -> "spr_dummy"
			const auto sprBaseFileName = std::string(SprPrefix) + std::string(baseFileName);
			// NOTE: "spr_dummy" -> "spr_dummy.farc"
			const auto sprFArcFileName = IO::Path::ChangeExtension(sprBaseFileName, ".farc");
			// NOTE: "spr_dummy.farc" -> "export/dir/spr_dummy.farc"
			const auto sprFArcFilePath = IO::Path::Combine(workingDirectory, sprFArcFileName);

			const auto sprFArc = IO::FArc::Open(sprFArcFilePath);
			if (sprFArc == nullptr)
				return nullptr;

			// NOTE: "spr_dummy.bin" or "spr_dummy.spr"
			const auto sprEntry = FindIfOrNull(sprFArc->GetEntries(), [&](const IO::FArcEntry& entry)
			{
				return IO::Path::TrimExtension(entry.Name) == sprBaseFileName && IO::Path::DoesAnyPackedExtensionMatch(IO::Path::GetExtension(entry.Name), ".bin;.spr");
			});

			if (sprEntry == nullptr)
				return nullptr;

			std::vector<u8> fileContent;
			fileContent.resize(sprEntry->OriginalSize);
			sprEntry->ReadIntoBuffer(fileContent.data());

			auto stream = IO::MemoryStream();
			stream.FromStreamSource(fileContent);

			auto reader = IO::StreamReader(stream);
			auto sprSet = std::make_unique<SprSet>();

			if (sprSet == nullptr)
				return nullptr;

			if (auto streamResult = sprSet->Read(reader); streamResult != IO::StreamResult::Success)
				return nullptr;

			return sprSet;
		}

		A_Err AEGP_FileImportCallbackHandler(const A_UTF16Char* filePath, AE_FIM_ImportOptions importOptions, AE_FIM_SpecialAction action, AEGP_ItemH itemHandle, AE_FIM_Refcon refcon)
		{
			const auto aetSetPath = UTF8::Narrow(AEUtil::WCast(filePath));
			const auto aetSet = AetImporter::LoadAetSet(aetSetPath);

			if (aetSet == nullptr || aetSet->GetScenes().empty())
				return A_Err_GENERIC;

			A_Err err = A_Err_NONE;

			const auto workingDirectory = IO::Path::GetDirectoryName(aetSetPath);
			const auto sprSet = TryLoadAetSetMatchingSprSet(aetSetPath, workingDirectory);

			if (sprSet != nullptr)
				Utilities::ExtractAllSprPNGs(workingDirectory, *sprSet);

			auto importer = AetImporter(workingDirectory);
			ERR(importer.ImportAetSet(*aetSet, sprSet.get(), importOptions, action, itemHandle));

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
			// NOTE: Arbitrary buffer size to store at least the date time compile time ID
			using IDType = std::array<char, 128>;

			static constexpr auto StorageValueKey = COMFY_STRINGIFY(ExportOptions);
			static constexpr auto CompileTimeID = IDType { COMFY_STRINGIFY(ExportOptions) " { { " __DATE__ " }, { " __TIME__ " } };" };

			// NOTE: To identify and compare the serialized object in memory
			size_t ObjectSize = sizeof(ExportOptions);
			IDType ObjectID = CompileTimeID;

			struct DatabaseData
			{
				bool ExportSprDB = true;
				bool ExportAetDB = true;
				bool ParseSprIDComments = true;
				bool MDataDBs = true;
				bool MergeExisting = true;
			} Database;
			struct SpriteData
			{
				bool ExportSprSet = true;
				bool PowerOfTwoTextures = true;
				bool CompressTextures = true;
				bool EncodeYCbCr = true;
			} Sprite;
			struct DebugData
			{
				bool WriteLog = false;
			} Debug;
			struct FArcData
			{
				bool Compress = true;
			} FArc;
			struct MiscData
			{
				bool ExportAetSet = true;
				bool RunPostExportScript = true;
			} Misc;
		};

		ExportOptions RetrieveLastUsedExportOptions(const SuitesData& suites)
		{
			A_Err err = A_Err_NONE;

			const ExportOptions defaultOptions = {};
			ExportOptions optionsResult = {};

			AEGP_PersistentBlobH persistentBlob = {};
			ERR(suites.PersistentDataSuite3->AEGP_GetApplicationBlob(&persistentBlob));
			ERR(suites.PersistentDataSuite3->AEGP_GetData(persistentBlob, PersistentDataPluginSectionKey, ExportOptions::StorageValueKey, static_cast<A_u_long>(sizeof(ExportOptions)), &defaultOptions, &optionsResult));

			if (err != A_Err_NONE)
				return defaultOptions;

			// NOTE: The perstistent data is stored as a raw object memory dump so any version that even has the potentail of having a different memory layout or version will be discarded.
			//		 This means previous options will always be invalidated after recompilation
			if (optionsResult.ObjectSize != sizeof(ExportOptions) || optionsResult.ObjectID != ExportOptions::CompileTimeID)
				return defaultOptions;

			return optionsResult;
		}

		void SaveLastUsedExportOptions(const SuitesData& suites, const ExportOptions& options)
		{
			A_Err err = A_Err_NONE;

			AEGP_PersistentBlobH persistentBlob = {};
			ERR(suites.PersistentDataSuite3->AEGP_GetApplicationBlob(&persistentBlob));
			ERR(suites.PersistentDataSuite3->AEGP_SetData(persistentBlob, PersistentDataPluginSectionKey, ExportOptions::StorageValueKey, static_cast<A_u_long>(sizeof(options)), &options));
		}

		std::pair<std::string, ExportOptions> OpenExportAetSetFileDialog(const SuitesData& suites, std::string_view fileName)
		{
			auto options = RetrieveLastUsedExportOptions(suites);

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
				// NOTE: Made unaccessible for now since SprID comments are almost always desired and in the rare cases that they aren't can be removed manually
				// { IO::Shell::Custom::ItemType::Checkbox, "Spr ID Comments", &options.Database.ParseSprIDComments },
				{ IO::Shell::Custom::ItemType::Checkbox, "MData Prefix", &options.Database.MDataDBs },
				{ IO::Shell::Custom::ItemType::Checkbox, "Merge Existing", &options.Database.MergeExisting },
				{ IO::Shell::Custom::ItemType::VisualGroupEnd, "---" },

				{ IO::Shell::Custom::ItemType::VisualGroupStart, "Sprite" },
				{ IO::Shell::Custom::ItemType::Checkbox, "Export Spr Set", &options.Sprite.ExportSprSet },
				{ IO::Shell::Custom::ItemType::Checkbox, "Compress Textures", &options.Sprite.CompressTextures },
				{ IO::Shell::Custom::ItemType::Checkbox, "Encode YCbCr", &options.Sprite.EncodeYCbCr },
				{ IO::Shell::Custom::ItemType::Checkbox, "Power of Two Textures", &options.Sprite.PowerOfTwoTextures },
				{ IO::Shell::Custom::ItemType::VisualGroupEnd, "---" },

				{ IO::Shell::Custom::ItemType::VisualGroupStart, "Sprite FArc" },
				{ IO::Shell::Custom::ItemType::Checkbox, "Compress Content", &options.FArc.Compress },
				{ IO::Shell::Custom::ItemType::VisualGroupEnd, "---" },

				{ IO::Shell::Custom::ItemType::VisualGroupStart, "Debug" },
				{ IO::Shell::Custom::ItemType::Checkbox, "Write Log File", &options.Debug.WriteLog },
				{ IO::Shell::Custom::ItemType::VisualGroupEnd, "---" },
			};

			const auto result = dialog.OpenSave();
			SaveLastUsedExportOptions(suites, options);

			return std::make_pair(dialog.OutFilePath, options);
		}

		std::pair<FILE*, LogLevel> OpenAetSetExportLogFile(std::string_view directory, std::string_view setName, const ExportOptions& options)
		{
			FILE* logStream = nullptr;
			LogLevel logLevel = LogLevel_None;

			if (options.Debug.WriteLog)
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

		std::unique_ptr<SprSet> TryExportSprSetFromAetSet(AetExporter& exporter, const ExportOptions& exportOptions, const SprSetSrcInfo* sprSetSrcInfo, const Aet::AetSet& aetSet)
		{
			if (!exportOptions.Sprite.ExportSprSet || sprSetSrcInfo == nullptr)
				return nullptr;

			auto sprSetOptions = AetExporter::SprSetExportOptions {};
			sprSetOptions.PowerOfTwo = exportOptions.Sprite.PowerOfTwoTextures;
			sprSetOptions.EnableCompression = exportOptions.Sprite.CompressTextures;
			sprSetOptions.EncodeYCbCr = exportOptions.Sprite.EncodeYCbCr;

			return exporter.CreateSprSetFromSprSetSrcInfo(*sprSetSrcInfo, aetSet, sprSetOptions);
		}

		int RunDebugPostExportScript(std::string_view outputDirectory)
		{
			const auto scriptDirectory = std::string(outputDirectory) + "\\..";

			const auto scriptFileName = "aetplugin_post_export.bat";
			const auto scriptFilePath = (scriptDirectory + "\\" + scriptFileName);

			if (!IO::File::Exists(scriptFilePath))
				return 0;

			// NOTE: This is of course **not** safe and should not be shipped with final builds
			const auto command = "cd \"" + scriptDirectory + "\" && " + scriptFileName;
			const int commandError = _wsystem(UTF8::WideArg(command).c_str());

			return commandError;
		}

		template <typename DBType>
		bool TryMergeExistingDB(DBType& outNewDB, DBType* existingDB)
		{
			static_assert(std::is_same_v<DBType, Database::AetDB> || std::is_same_v<DBType, Database::SprDB>);

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

		A_Err ExportAetSetCommand()
		{
			auto exporter = AetExporter();

			const auto[outputFilePath, exportOptions] = OpenExportAetSetFileDialog(exporter.Suites(), exporter.GetAetSetNameFromProjectName());
			const auto outputDirectory = IO::Path::GetDirectoryName(outputFilePath);

			if (outputFilePath.empty())
				return A_Err_NONE;

			const auto aetSetName = IO::Path::GetFileName(outputFilePath, false);
			const auto aetSetFileName = IO::Path::GetFileName(outputFilePath, true);
			const auto setBaseName = std::string(Util::StripPrefixInsensitive(aetSetName, AetPrefix));
			const auto sprSetName = ("spr_" + setBaseName);

			const auto[logFile, logLevel] = OpenAetSetExportLogFile(outputDirectory, Util::StripPrefixInsensitive(aetSetName, AetPrefix), exportOptions);
			exporter.SetLog(logFile, logLevel);

			const auto[aetSet, sprSetSrcInfo] = exporter.ExportAetSet(outputDirectory, exportOptions.Database.ParseSprIDComments);
			CloseAetSetExportLogFile(logFile);

			if (aetSet == nullptr)
				return A_Err_GENERIC;

			const auto aetSaveFuture = IO::File::SaveAsync(outputFilePath, (exportOptions.Misc.ExportAetSet) ? aetSet.get() : nullptr);

			const auto sprSet = TryExportSprSetFromAetSet(exporter, exportOptions, sprSetSrcInfo.get(), *aetSet);
			const auto sprSaveFuture = std::async(std::launch::async, [&]
			{
				if (sprSet == nullptr)
					return;

				auto farcPacker = IO::FArcPacker();
				farcPacker.AddFile(sprSetName + ".bin", *sprSet);
				farcPacker.CreateFlushFArc(IO::Path::Combine(outputDirectory, sprSetName + ".farc"), exportOptions.FArc.Compress);
			});

			const auto dbNamePrefix = exportOptions.Database.MDataDBs ? std::string("mdata_") : "";
			const auto dbNameSuffix = exportOptions.Database.MDataDBs ? "" : ("_" + setBaseName);

			const auto sprDBPath = IO::Path::Combine(outputDirectory, (dbNamePrefix + "spr_db" + dbNameSuffix + ".bin"));
			const auto aetDBPath = IO::Path::Combine(outputDirectory, (dbNamePrefix + "aet_db" + dbNameSuffix + ".bin"));

			auto sprDB = (exportOptions.Database.ExportSprDB) ? exporter.CreateSprDBFromAetSet(*aetSet, aetSetFileName, sprSet.get()) : nullptr;
			auto aetDB = (exportOptions.Database.ExportAetDB) ? exporter.CreateAetDBFromAetSet(*aetSet, aetSetFileName) : nullptr;

			if (exportOptions.Database.MergeExisting)
			{
				if (sprDB != nullptr) { TryMergeExistingDB(*sprDB, IO::File::Load<Database::SprDB>(sprDBPath).get()); }
				if (aetDB != nullptr) { TryMergeExistingDB(*aetDB, IO::File::Load<Database::AetDB>(aetDBPath).get()); }
			}

			const auto dbSaveFutures = std::array
			{
				IO::File::SaveAsync(sprDBPath, sprDB.get()),
				IO::File::SaveAsync(aetDBPath, aetDB.get()),
			};

#if 0 // DEBUG:
			if (exportOptions.Misc.RunPostExportScript)
				RunDebugPostExportScript(outputDirectory);
#endif /* 1 */

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
