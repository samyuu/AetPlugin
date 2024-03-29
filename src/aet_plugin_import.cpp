#include "aet_plugin_import.h"
#include "core_io.h"

namespace AetPlugin
{
	struct TempFArc
	{
		TempFArc(std::string_view farcPath) : FArc(FArc::Open(farcPath)) {}

		template <typename Readable>
		std::unique_ptr<Readable> LoadFile(std::string_view fileName)
		{
			static_assert(std::is_base_of_v<IStreamReadable, Readable>);
			const FArcEntry* entry = (FArc != nullptr) ? FArc->FindFile(fileName) : nullptr;
			if (entry == nullptr)
				return nullptr;

			FileBuffer.resize(entry->OriginalSize);
			entry->ReadIntoBuffer(FileBuffer.data());

			auto out = std::make_unique<Readable>();
			if (out == nullptr)
				return nullptr;

			MemoryStream stream {}; stream.FromStreamSource(FileBuffer);
			StreamReader reader { stream };
			if (out->Read(reader) != StreamResult::Success)
				return nullptr;

			return out;
		}

		std::unique_ptr<FArc> FArc;
		std::vector<u8> FileBuffer;
	};

	static std::string GetAetSetName(const Aet::AetSet& set)
	{
		const std::string lowerCaseSetName = ASCII::ToLowerCopy(set.Name);
		const std::string_view setNameWithoutAet = std::string_view(lowerCaseSetName).substr(std::string_view(AetPrefix).length());
		return ASCII::ToLowerCopy(std::string(setNameWithoutAet));
	}

	static std::string FormatSceneName(const Aet::AetSet& set, const Aet::Scene& scene)
	{
		std::string result;
		result += GetAetSetName(set);
		result += "_";
		result += ASCII::ToLowerCopy(scene.Name);
		return result;
	}

	std::pair<std::unique_ptr<Aet::AetSet>, std::unique_ptr<AetDB>> AetImporter::TryLoadAetSetAndDB(std::string_view aetFilePathOrFArc)
	{
		const auto verifyResult = VerifyAetSetImportable(aetFilePathOrFArc);
		if (verifyResult != AetSetVerifyResult::Valid)
			return std::make_pair(nullptr, nullptr);

		const auto directory = Path::GetDirectoryName(aetFilePathOrFArc);
		const auto aetFileName = std::string(Path::GetFileName(aetFilePathOrFArc, false));
		const b8 isFArc = ASCII::MatchesInsensitive(Path::GetExtension(aetFilePathOrFArc), ".farc");

		std::unique_ptr<Aet::AetSet> aetSet = nullptr;
		std::unique_ptr<AetDB> aetDB = nullptr;

		if (isFArc)
		{
			TempFArc tempFArc { aetFilePathOrFArc };
			aetSet = tempFArc.LoadFile<Aet::AetSet>(aetFileName + ".aec");
			aetDB = tempFArc.LoadFile<AetDB>(aetFileName + ".aei");
		}
		else
		{
			aetSet = LoadFile<Aet::AetSet>(aetFilePathOrFArc);

			const std::string dbPathsToCheck[4] =
			{
				Path::Combine(directory, "aet_db.bin"),
				Path::Combine(directory, "mdata_aet_db.bin"),
				Path::Combine(directory, "aet_db_" + std::string(ASCII::TrimPrefixInsensitive(aetFileName, AetPrefix)) + ".bin"),
				Path::Combine(directory, std::string(aetFileName) + ".aei"),
			};

			for (auto& dbPath : dbPathsToCheck)
			{
				if ((aetDB = LoadFile<AetDB>(dbPath)) != nullptr)
					break;
			}
		}

		if (aetSet != nullptr)
			aetSet->Name = aetFileName;

		return std::make_pair(std::move(aetSet), std::move(aetDB));
	}

	std::pair<std::unique_ptr<SprSet>, std::unique_ptr<SprDB>> AetImporter::TryLoadSprSetAndDB(std::string_view aetFilePathOrFArc)
	{
		const auto directory = Path::GetDirectoryName(aetFilePathOrFArc);
		const auto sprFileName = std::string(SprPrefix) + std::string(ASCII::TrimPrefixInsensitive(Path::GetFileName(aetFilePathOrFArc, false), AetPrefix));
		const b8 isFArc = ASCII::MatchesInsensitive(Path::GetExtension(aetFilePathOrFArc), ".farc");

		std::unique_ptr<SprSet> sprSet = nullptr;
		std::unique_ptr<SprDB> sprDB = nullptr;

		if (isFArc)
		{
			const auto sprFArcPath = Path::Combine(directory, sprFileName) + ".farc";

			TempFArc tempFArc { sprFArcPath };
			sprSet = tempFArc.LoadFile<SprSet>(sprFileName + ".spr");
			sprDB = tempFArc.LoadFile<SprDB>(sprFileName + ".spi");

			if (sprSet != nullptr && sprDB != nullptr)
			{
				const auto matchingEntry = (sprDB->Entries.size() == 1) ?
					&sprDB->Entries.front() :
					FindIfOrNull(sprDB->Entries, [&](const auto& e) { return ASCII::MatchesInsensitive(e.Name, sprFileName); });

				if (matchingEntry != nullptr)
					sprSet->ApplyDBNames(*matchingEntry);
			}
		}
		else
		{
			sprSet = LoadFile<SprSet>(Path::Combine(directory, sprFileName + ".bin"));
			if (sprSet == nullptr)
			{
				TempFArc tempFArc { Path::Combine(directory, sprFileName + ".farc") };
				sprSet = tempFArc.LoadFile<SprSet>(sprFileName + ".bin");
			}

			const std::string dbPathsToCheck[4] =
			{
				Path::Combine(directory, "spr_db.bin"),
				Path::Combine(directory, "mdata_spr_db.bin"),
				Path::Combine(directory, "spr_db_" + std::string(ASCII::TrimPrefixInsensitive(sprFileName, SprPrefix)) + ".bin"),
				Path::Combine(directory, std::string(sprFileName) + ".spi"),
			};

			for (auto& dbPath : dbPathsToCheck)
			{
				if ((sprDB = LoadFile<SprDB>(dbPath)) != nullptr)
					break;
			}
		}

		if (sprSet != nullptr)
			sprSet->Name = sprFileName;

		return std::make_pair(std::move(sprSet), std::move(sprDB));
	}

	AetImporter::AetSetVerifyResult AetImporter::VerifyAetSetImportable(std::string_view aetFilePathOrFArc)
	{
		const auto fileName = Path::GetFileName(aetFilePathOrFArc, false);
		if (!ASCII::StartsWithInsensitive(fileName, AetPrefix))
			return AetSetVerifyResult::InvalidPath;

		const auto extension = Path::GetExtension(aetFilePathOrFArc);

		// NOTE: Opening every farc clicked is expensive and potentially error prone, FArcs will be error checked on load instead
		if (ASCII::MatchesInsensitive(extension, ".farc"))
			return AetSetVerifyResult::Valid;

		if (!Path::HasAnyExtension(extension, ".bin;.aec"))
			return AetSetVerifyResult::InvalidPath;

		FileStream fileStream;
		fileStream.OpenRead(aetFilePathOrFArc);
		if (!fileStream.IsOpen())
			return AetSetVerifyResult::InvalidFile;

		StreamReader reader { fileStream };

		struct SceneRawData
		{
			u32 NameOffset;
			f32 StartFrame;
			f32 EndFrame;
			f32 FrameRate;
			u32 BackgroundColor;
			i32 Width, Height;
			u32 CameraOffset;
			u32 CompositionCount, CompositionOffset;
			u32 VideoCount, VideoOffset;
			u32 AudioCount, AudioOffset;
		};

		static constexpr FileAddr validScenePlusPointerSize = static_cast<FileAddr>(sizeof(SceneRawData) + (sizeof(u32) * 2));
		if (reader.GetLength() < validScenePlusPointerSize)
			return AetSetVerifyResult::InvalidPointer;

		const auto baseHeader = SectionHeader::TryRead(reader, SectionSignature::AETC);

		// NOTE: Thanks CS3 for making it this easy
		if (baseHeader.has_value())
		{
			if (!reader.IsValidPointer(baseHeader->EndOfSectionAddress()))
				return AetSetVerifyResult::InvalidPointer;

			if (!reader.IsValidPointer(baseHeader->EndOfSubSectionAddress()))
				return AetSetVerifyResult::InvalidPointer;

			return AetSetVerifyResult::Valid;
		}

		static constexpr u32 maxReasonableSceneCount = 64;
		FileAddr scenePointers[maxReasonableSceneCount] = {};

		for (u32 i = 0; i < maxReasonableSceneCount; i++)
		{
			scenePointers[i] = reader.ReadPtr();
			if (scenePointers[i] == FileAddr::NullPtr)
				break;
			else if (!reader.IsValidPointer(scenePointers[i]))
				return AetSetVerifyResult::InvalidPointer;
		}

		const b8 insufficientSceneFileSpace = (scenePointers[0] + static_cast<FileAddr>(sizeof(SceneRawData))) >= reader.GetLength();
		if (scenePointers[0] == FileAddr::NullPtr || insufficientSceneFileSpace)
			return AetSetVerifyResult::InvalidPointer;

		SceneRawData rawScene;
		reader.Seek(scenePointers[0]);
		reader.ReadBuffer(&rawScene, sizeof(rawScene));

		const FileAddr fileOffsetsToCheck[] =
		{
			static_cast<FileAddr>(rawScene.NameOffset),
			static_cast<FileAddr>(rawScene.CameraOffset),
			static_cast<FileAddr>(rawScene.CompositionOffset),
			static_cast<FileAddr>(rawScene.VideoOffset),
			static_cast<FileAddr>(rawScene.AudioOffset),
		};

		if (!std::any_of(std::begin(fileOffsetsToCheck), std::end(fileOffsetsToCheck), [&](auto& offset) { return reader.IsValidPointer(offset); }))
			return AetSetVerifyResult::InvalidPointer;

		const u32 arraySizesToCheck[] =
		{
			rawScene.CompositionCount,
			rawScene.VideoCount,
			rawScene.AudioCount,
		};

		constexpr u32 reasonableArraySizeLimit = 999999;
		if (std::any_of(std::begin(arraySizesToCheck), std::end(arraySizesToCheck), [&](const auto size) { return size > reasonableArraySizeLimit; }))
			return AetSetVerifyResult::InvalidCount;

		static constexpr auto isReasonableFrameRangeDuration = [&](frame_t frame)
		{
			constexpr frame_t reasonableStartEndFrameLimit = 100000.0f;
			return (frame > -reasonableStartEndFrameLimit) && (frame < +reasonableStartEndFrameLimit);
		};

		static constexpr auto isReasonableFrameRate = [](frame_t frameRate)
		{
			constexpr frame_t reasonableFrameRateLimit = 999.0f;
			return (frameRate > 0.0f && frameRate <= reasonableFrameRateLimit);
		};

		static constexpr auto isReasonableSize = [](i32 size)
		{
			constexpr i32 reasonableSizeLimit = 30000;
			return (size > 0 && size <= reasonableSizeLimit);
		};

		if (!isReasonableFrameRangeDuration(rawScene.StartFrame) ||
			!isReasonableFrameRangeDuration(rawScene.EndFrame) ||
			!isReasonableFrameRate(rawScene.FrameRate) ||
			!isReasonableSize(rawScene.Width) ||
			!isReasonableSize(rawScene.Height))
			return AetSetVerifyResult::InvalidCount;

		return AetSetVerifyResult::Valid;
	}

	A_Err AetImporter::ImportAetSet(std::string_view workingDirectory, Aet::AetSet& aetSet, const SprSet* sprSet, const AetDB* aetDB, const SprDB* sprDB, AE_FIM_ImportOptions importOptions, AE_FIM_SpecialAction action, AEGP_ItemH itemHandle)
	{
		this->workingDirectory.ImportDirectory = workingDirectory;
		SetupWorkingSetData(aetSet, sprSet, aetDB, sprDB);
		CheckWorkingDirectoryFiles();

		GetProjectHandles();
		CreateProjectFolders();

		size_t sceneIndex = 0;
		for (const auto& scene : workingSet.Set->Scenes)
		{
			SetupWorkingSceneData(*scene, sceneIndex++);
			CreateSceneFolders();

			ImportAllFootage();

			if (ShouldImportVideoDBForScene(*workingScene.Scene, workingScene.SceneIndex))
				CreateImportUnreferencedSprDBFootageAndLayer();

			ImportAllCompositions();
		}

		// suites.UtilitySuite3->AEGP_ReportInfo(GlobalPluginID, "Let's hope for the best...");
		return A_Err_NONE;
	}

	const SpriteFileData* AetImporter::FindMatchingSpriteFile(std::string_view sourceName) const
	{
		return FindIfOrNull(workingDirectory.AvailableSpriteFiles, [&](const SpriteFileData& spriteFile)
		{
			return ASCII::MatchesInsensitive(spriteFile.SanitizedFileName, sourceName);
		});
	}

	void AetImporter::SetupWorkingSetData(const Aet::AetSet& aetSet, const SprSet* sprSet, const AetDB* aetDB, const SprDB* sprDB)
	{
		workingSet.Set = &aetSet;
		workingSet.SprSet = sprSet;

		workingSet.NamePrefix = GetAetSetName(aetSet);
		workingSet.NamePrefixUnderscore = workingSet.NamePrefix + "_";

		const auto aetSetEntryName = std::string(AetPrefix) + workingSet.NamePrefix;
		const auto sprSetEntryName = std::string(SprPrefix) + workingSet.NamePrefix;

		workingSet.AetEntry = (aetDB != nullptr) ? FindIfOrNull(aetDB->Entries, [&](const auto& e) { return ASCII::MatchesInsensitive(e.Name, aetSetEntryName); }) : nullptr;
		workingSet.SprEntry = (sprDB != nullptr) ? FindIfOrNull(sprDB->Entries, [&](const auto& e) { return ASCII::MatchesInsensitive(e.Name, sprSetEntryName); }) : nullptr;
	}

	void AetImporter::SetupWorkingSceneData(const Aet::Scene& scene, size_t sceneIndex)
	{
		workingScene.Scene = &scene;
		workingScene.SceneIndex = sceneIndex;
		workingScene.AE_FrameRate = { static_cast<A_long>(scene.FrameRate * AEUtil::FixedPoint), static_cast<A_u_long>(AEUtil::FixedPoint) };

		workingScene.SourcelessVideoLayerUsages.clear();
		auto setCompUsages = [&](const Aet::Composition& comp)
		{
			for (const auto& layer : comp.Layers)
				if (layer->GetVideoItem() != nullptr && layer->GetVideoItem()->Sources.empty())
					workingScene.SourcelessVideoLayerUsages[layer->GetVideoItem().get()] = layer.get();
		};
		setCompUsages(*scene.RootComposition);
		for (const auto& comp : scene.Compositions)
			setCompUsages(*comp);

		workingScene.GivenCompDurations = CreateGivenCompDurationsMap(*workingScene.Scene);

		workingScene.UnreferencedVideoComp = nullptr;
		workingScene.UnreferencedVideoLayer = nullptr;
	}

	void AetImporter::CheckWorkingDirectoryFiles()
	{
		Directory::ForEachFile(workingDirectory.ImportDirectory, [&](const std::string_view filePath)
		{
			const std::string_view fileName = Path::GetFileName(filePath);
			if (Path::HasExtension(fileName, SpriteFileData::PNGExtension))
			{
				std::string_view sanitizedFileName = Path::TrimExtension(fileName);
				sanitizedFileName = ASCII::TrimPrefixInsensitive(sanitizedFileName, SpriteFileData::SprPrefix);
				sanitizedFileName = ASCII::TrimPrefixInsensitive(sanitizedFileName, workingSet.NamePrefixUnderscore);

				SpriteFileData& spriteFileData = workingDirectory.AvailableSpriteFiles.emplace_back();
				spriteFileData.SanitizedFileName = sanitizedFileName;
				spriteFileData.FilePath = filePath;
			}
			return ControlFlow::Continue;
		});
	}

	A_Time AetImporter::FrameToAETime(frame_t frame) const
	{
		return AEUtil::FrameToAETime(frame, workingScene.Scene->FrameRate);
	}

	void AetImporter::GetProjectHandles()
	{
		suites.ProjSuite5->AEGP_GetProjectByIndex(0, &project.ProjectHandle);
		suites.ProjSuite5->AEGP_GetProjectRootFolder(project.ProjectHandle, &project.RootItemHandle);
	}

	void AetImporter::CreateProjectFolders()
	{
		const auto& setRootName = workingSet.NamePrefix;
		suites.ItemSuite1->AEGP_CreateNewFolder(setRootName.c_str(), project.RootItemHandle, &project.Folders.Root);

		if (workingSet.AetEntry != nullptr && workingSet.SprEntry != nullptr)
		{
			CommentUtil::Buffer commentBuffer = {};
			sprintf(commentBuffer.data(), "%s, 0x%X, 0x%X", workingSet.Set->Name.c_str(), workingSet.AetEntry->ID, workingSet.SprEntry->ID);
			CommentUtil::Set(suites.ItemSuite8, project.Folders.Root, { CommentUtil::Keys::AetSet, commentBuffer.data() });
		}
		else
		{
			CommentUtil::Set(suites.ItemSuite8, project.Folders.Root, { CommentUtil::Keys::AetSet, workingSet.Set->Name });
		}
	}

	b8 AetImporter::ShouldImportVideoDBForScene(const Aet::Scene& scene, size_t sceneIndex) const
	{
		// NOTE: Always import unreferenced sprites into the first scene only because duplicate videos will result in duplicate output sprites
		//		 and the first typically "MAIN" scene is where in practice all of the unreferenced sprites are expected anyways
		return (sceneIndex == 0);

		// NOTE: Ignore DIVA "TOUCH" scenes because it should be safe to assume that they won't rely on any external spr_db sprites
		static constexpr std::array<std::string_view, 1> blacklistSceneNames = { "TOUCH", };

		return std::any_of(blacklistSceneNames.begin(), blacklistSceneNames.end(), [&](const auto& blacklistName)
		{
			return (scene.Name == blacklistName);
		});
	}

	void AetImporter::CreateSceneFolders()
	{
		const auto sceneRootName = ASCII::ToSnakeCaseLowerCopy(workingScene.Scene->Name) + ProjectStructure::Names::SceneRootPrefix;
		suites.ItemSuite1->AEGP_CreateNewFolder(sceneRootName.c_str(), project.Folders.Root, &project.Folders.Scene.Root);
		suites.ItemSuite1->AEGP_CreateNewFolder(ProjectStructure::Names::SceneData, project.Folders.Scene.Root, &project.Folders.Scene.Data);
		suites.ItemSuite1->AEGP_CreateNewFolder(ProjectStructure::Names::SceneVideo, project.Folders.Scene.Data, &project.Folders.Scene.Video);
		if (workingSet.SprEntry != nullptr && ShouldImportVideoDBForScene(*workingScene.Scene, workingScene.SceneIndex))
			suites.ItemSuite1->AEGP_CreateNewFolder(ProjectStructure::Names::SceneVideoDB, project.Folders.Scene.Data, &project.Folders.Scene.VideoDB);
		suites.ItemSuite1->AEGP_CreateNewFolder(ProjectStructure::Names::SceneAudio, project.Folders.Scene.Data, &project.Folders.Scene.Audio);
		suites.ItemSuite1->AEGP_CreateNewFolder(ProjectStructure::Names::SceneComp, project.Folders.Scene.Data, &project.Folders.Scene.Comp);
	}

	void AetImporter::ImportAllFootage()
	{
		for (const auto& video : workingScene.Scene->Videos)
			ImportVideo(*video);

		for (const auto& audio : workingScene.Scene->Audios)
			ImportAudio(*audio);
	}

	// NOTE: { "TEST_SPRITE" -> 0 }, { "TEST_SPRITE_00" -> 2 }, { "TEST_SPRITE_000" -> 3 }
	static constexpr i32 GetSpriteSequenceDigitCount(std::string_view name)
	{
		i32 count = 0;
		for (auto it = name.rbegin(); it != name.rend(); it++)
		{
			if (*it == '0') count++;
			else if (*it == '_') return count;
			else break;
		}
		return count;
	}

	// NOTE: { "TEST_SPRITE_000", "TEST_SPRITE_001", "TEST_SPRITE_002", "TEST_SPRITE_003", "TEST_SPRITE_OTHER" -> 4 }
	static size_t GetSpriteSequenceFrameCount(const std::vector<SprEntry>& entries, const size_t startIndex, const i32 digitCount)
	{
		if (digitCount < 2)
			return 1;

		const auto& startEntry = entries[startIndex];
		const auto baseName = startEntry.Name.substr(0, startEntry.Name.size() - digitCount);

		for (size_t i = startIndex + 1; i < entries.size(); i++)
		{
			if (entries[i].Name.size() != startEntry.Name.size() || !ASCII::StartsWith(entries[i].Name, baseName))
				return (i - startIndex);
		}
		return 1;
	}

	static std::shared_ptr<Aet::LayerVideo> CreateStaticCenteredLayerVideo(vec2 videoSize, vec2 sceneSize)
	{
		auto layerVideo = std::make_shared<Aet::LayerVideo>();
		layerVideo->Transform.Origin.X->emplace_back(videoSize.x / 2.0f);
		layerVideo->Transform.Origin.Y->emplace_back(videoSize.y / 2.0f);
		layerVideo->Transform.Position.X->emplace_back(sceneSize.x / 2.0f);
		layerVideo->Transform.Position.Y->emplace_back(sceneSize.y / 2.0f);
		layerVideo->Transform.Rotation->emplace_back(0.0f);
		layerVideo->Transform.Scale.X->emplace_back(1.0f);
		layerVideo->Transform.Scale.Y->emplace_back(1.0f);
		layerVideo->Transform.Opacity->emplace_back(1.0f);
		return layerVideo;
	}

	void AetImporter::CreateImportUnreferencedSprDBFootageAndLayer()
	{
		if (workingSet.SprEntry == nullptr)
			return;

		const auto dbOnlyVideos = CreateUnreferencedSprDBVideos();
		if (dbOnlyVideos.empty())
			return;

		const frame_t compDuration = (workingScene.Scene->FrameRate * 1.0f);

		workingScene.UnreferencedVideoComp = std::make_shared<Aet::Composition>();
		auto& composition = *workingScene.UnreferencedVideoComp;
		composition.GivenName = ProjectStructure::Names::UnreferencedSpritesComp;
		composition.Layers.reserve(dbOnlyVideos.size());
		for (const auto& video : dbOnlyVideos)
		{
			ImportSpriteVideo(*video, true);

			auto& layer = *composition.Layers.emplace_back(std::make_shared<Aet::Layer>());
			layer.StartFrame = 0.0f;
			layer.EndFrame = compDuration;
			layer.StartOffset = 0.0f;
			layer.TimeScale = 1.0f;
			layer.Flags.VideoActive = true;
			layer.Quality = Aet::LayerQuality::Best;
			layer.ItemType = Aet::ItemType::Video;
			layer.LayerVideo = CreateStaticCenteredLayerVideo(vec2(0.0f, 0.0f), vec2(0.0f, 0.0f));
			layer.LayerVideo->TransferMode.BlendMode = Aet::BlendMode::Normal;
			layer.LayerVideo->TransferMode.TrackMatte = UnreferencedVideoRequiresSeparateSprite(*video) ? Aet::TrackMatte::Alpha : Aet::TrackMatte::NoTrackMatte;
			layer.SetName(video->Sources.front().Name);
			layer.SetItem(video);
		}

		workingScene.UnreferencedVideoLayer = std::make_shared<Aet::Layer>();
		auto& videoDBLayer = *workingScene.UnreferencedVideoLayer;
		videoDBLayer.StartFrame = 0.0f;
		videoDBLayer.EndFrame = compDuration;
		videoDBLayer.StartOffset = 0.0f;
		videoDBLayer.TimeScale = 1.0f;
		videoDBLayer.Flags.VideoActive = false;
		videoDBLayer.Quality = Aet::LayerQuality::Best;
		videoDBLayer.ItemType = Aet::ItemType::Composition;
		videoDBLayer.LayerVideo = CreateStaticCenteredLayerVideo(vec2(workingScene.Scene->Resolution), vec2(workingScene.Scene->Resolution));
		videoDBLayer.LayerVideo->TransferMode.BlendMode = Aet::BlendMode::Normal;
		videoDBLayer.LayerVideo->TransferMode.TrackMatte = Aet::TrackMatte::NoTrackMatte;
		videoDBLayer.SetName(ProjectStructure::Names::UnreferencedSpritesLayer);
		videoDBLayer.SetItem(workingScene.UnreferencedVideoComp);
	}

	std::vector<std::shared_ptr<Aet::Video>> AetImporter::CreateUnreferencedSprDBVideos() const
	{
		std::vector<std::shared_ptr<Aet::Video>> dbOnlyVideos;
		assert(workingSet.SprEntry != nullptr);

		const auto& sprEntries = workingSet.SprEntry->SprEntries;
		for (size_t i = 0; i < sprEntries.size(); i++)
		{
			const auto& currentSprEntry = sprEntries[i];

			constexpr b8 tryImportSequence = false;
			const size_t frameCount = (tryImportSequence) ? GetSpriteSequenceFrameCount(sprEntries, i, GetSpriteSequenceDigitCount(currentSprEntry.Name)) : 1;

			const auto existingVideo = FindIfOrNull(workingScene.Scene->Videos, [&](const auto& video)
			{
				return (FindIfOrNull(video->Sources, [&](const auto& source) { return (source.ID == currentSprEntry.ID); })) != nullptr;
			});

			if (existingVideo == nullptr)
			{
				auto& video = dbOnlyVideos.emplace_back(std::make_shared<Aet::Video>());
				video->Color = 0x00FFFFFF;
				video->Size = ivec2(128, 128);
				video->FilesPerFrame = 1.0f;
				video->Sources.reserve(frameCount);

				for (size_t frameIndex = 0; frameIndex < frameCount; frameIndex++)
				{
					const auto sprFrameEntry = sprEntries[i + frameIndex];
					auto& source = video->Sources.emplace_back();

					source.Name = ASCII::TrimPrefixInsensitive(sprFrameEntry.Name, SprPrefix);
					source.ID = sprFrameEntry.ID;
				}
			}

			i += (frameCount - 1);
		}

		return dbOnlyVideos;
	}

	b8 AetImporter::UnreferencedVideoRequiresSeparateSprite(const Aet::Video& video) const
	{
		if (video.Sources.empty())
			return false;

		if (workingSet.SprSet == nullptr)
			return false;

		const auto& sprSet = *workingSet.SprSet;

		for (const auto& spr : sprSet.Sprites)
		{
			if (!InBounds(spr.TextureIndex, sprSet.TexSet.Textures))
				continue;

			if (!ASCII::MatchesInsensitive(spr.Name, ASCII::TrimPrefixInsensitive(video.Sources.front().Name, workingSet.NamePrefixUnderscore)))
				continue;

			const auto& texture = sprSet.TexSet.Textures[spr.TextureIndex];
			const b8 isNoMergeTexture = ASCII::Contains(texture->GetName(), "NOMERGE");

			if (isNoMergeTexture)
				return true;
		}

		return false;
	}

	void AetImporter::ImportAllCompositions()
	{
		ImportSceneComps();

		ImportAllLayersInComp(*workingScene.Scene->RootComposition);
		std::for_each(workingScene.Scene->Compositions.rbegin(), workingScene.Scene->Compositions.rend(), [&](const auto& comp)
		{
			ImportAllLayersInComp(*comp);
		});

		if (workingScene.UnreferencedVideoComp != nullptr && workingScene.UnreferencedVideoLayer != nullptr)
		{
			ImportComposition(*workingScene.UnreferencedVideoComp, true);
			ImportAllLayersInComp(*workingScene.UnreferencedVideoComp);
			ImportLayer(*workingScene.Scene->RootComposition, *workingScene.UnreferencedVideoLayer);
		}
	}

	void AetImporter::ImportVideo(const Aet::Video& video)
	{
		if (video.Sources.empty())
			ImportPlaceholderVideo(video);
		else
			ImportSpriteVideo(video);
	}

	void AetImporter::ImportPlaceholderVideo(const Aet::Video& video)
	{
		const AEGP_ColorVal videoColor = AEUtil::ColorRGB8(video.Color);

		if (auto layerUsage = workingScene.SourcelessVideoLayerUsages.find(&video); layerUsage != workingScene.SourcelessVideoLayerUsages.end())
		{
			suites.FootageSuite5->AEGP_NewSolidFootage(layerUsage->second->GetName().c_str(), video.Size.x, video.Size.y, &videoColor, &exData.Get(video).AE_Footage);
		}
		else
		{
			char placeholderNameBuffer[AEGP_MAX_ITEM_NAME_SIZE];
			sprintf(placeholderNameBuffer, "Placeholder (%dx%d)", video.Size.x, video.Size.y);

			suites.FootageSuite5->AEGP_NewSolidFootage(placeholderNameBuffer, video.Size.x, video.Size.y, &videoColor, &exData.Get(video).AE_Footage);
		}

		ImportVideoAddItemToProject(video);
	}

	void AetImporter::ImportSpriteVideo(const Aet::Video& video, b8 dbVideo)
	{
		const auto frontSourceNameWithoutSetName = ASCII::TrimPrefixInsensitive(video.Sources.front().Name, workingSet.NamePrefixUnderscore);
		if (const auto* matchingSpriteFile = FindMatchingSpriteFile(frontSourceNameWithoutSetName); matchingSpriteFile != nullptr)
		{
			AEGP_FootageLayerKey footageLayerKey = {};
			footageLayerKey.layer_idL = AEGP_LayerID_UNKNOWN;
			footageLayerKey.layer_indexL = 0;

			AEGP_FileSequenceImportOptions sequenceImportOptions = {};
			sequenceImportOptions.all_in_folderB = (video.Sources.size() > 1);
			sequenceImportOptions.force_alphabeticalB = false;
			sequenceImportOptions.start_frameL = 0;
			sequenceImportOptions.end_frameL = static_cast<A_long>(video.Sources.size()) - 1;

			const auto wideFilePath = UTF8::WideArg(matchingSpriteFile->FilePath);
			suites.FootageSuite5->AEGP_NewFootage(Global.PluginID, AEUtil::UTF16Cast(wideFilePath.c_str()), &footageLayerKey, &sequenceImportOptions, false, nullptr, &exData.Get(video).AE_Footage);
		}
		else
		{
			constexpr frame_t placeholderDuration = 270.0f;

			const A_Time duration = FrameToAETime(placeholderDuration);
			suites.FootageSuite5->AEGP_NewPlaceholderFootage(Global.PluginID, frontSourceNameWithoutSetName.data(), video.Size.x, video.Size.y, &duration, &exData.Get(video).AE_Footage);
		}

		ImportVideoAddItemToProject(video, dbVideo);
		ImportVideoSetSprIDComment(video);
		ImportVideoSetSequenceInterpretation(video);
		ImportVideoSetItemName(video);
	}

	void AetImporter::ImportVideoAddItemToProject(const Aet::Video& video, b8 dbVideo)
	{
		auto& videoExtraData = exData.Get(video);

		if (videoExtraData.AE_Footage != nullptr)
			suites.FootageSuite5->AEGP_AddFootageToProject(videoExtraData.AE_Footage, dbVideo ? project.Folders.Scene.VideoDB : project.Folders.Scene.Video, &videoExtraData.AE_FootageItem);
	}

	void AetImporter::ImportVideoSetSprIDComment(const Aet::Video& video)
	{
		if (video.Sources.empty())
			return;

		CommentUtil::Buffer commentBuffer = {};
		for (auto& source : video.Sources)
		{
			char appendBuffer[32];
			sprintf(appendBuffer, "0x%02X", source.ID);
			std::strcat(commentBuffer.data(), appendBuffer);

			if (&source != &video.Sources.back())
				std::strcat(commentBuffer.data(), ", ");
		}

		CommentUtil::Set(suites.ItemSuite8, exData.Get(video).AE_FootageItem, { CommentUtil::Keys::Spr, commentBuffer.data() });
	}

	void AetImporter::ImportVideoSetSequenceInterpretation(const Aet::Video& video)
	{
		if (video.Sources.size() <= 1)
			return;

		auto& videoExtraData = exData.Get(video);
		const frame_t frameRate = (workingScene.Scene->FrameRate * video.FilesPerFrame);

		AEGP_FootageInterp interpretation;
		suites.FootageSuite5->AEGP_GetFootageInterpretation(videoExtraData.AE_FootageItem, false, &interpretation);
		interpretation.native_fpsF = static_cast<A_FpLong>(frameRate);
		interpretation.conform_fpsF = static_cast<A_FpLong>(frameRate);
		suites.FootageSuite5->AEGP_SetFootageInterpretation(videoExtraData.AE_FootageItem, false, &interpretation);
	}

	void AetImporter::ImportVideoSetItemName(const Aet::Video& video)
	{
		if (video.Sources.empty())
			return;

		auto& videoExtraData = exData.Get(video);

		AEGP_MemHandle nameHandle;
		suites.ItemSuite8->AEGP_GetItemName(Global.PluginID, videoExtraData.AE_FootageItem, &nameHandle);

		const auto itemFileName = UTF8::Narrow(AEUtil::MoveFreeUTF16String(suites.MemorySuite1, nameHandle));
		auto cleanName = ASCII::ToLowerCopy(std::string(ASCII::TrimPrefixInsensitive(ASCII::TrimPrefixInsensitive(itemFileName, SprPrefix), workingSet.NamePrefixUnderscore)));

		if (video.Sources.size() > 1)
		{
			for (auto it = cleanName.rbegin(); it != cleanName.rend(); it++)
			{
				if (*it == '}') { *it = ']'; }
				if (*it == '{') { *it = '['; break; }
			}
		}

		suites.ItemSuite8->AEGP_SetItemName(videoExtraData.AE_FootageItem, AEUtil::UTF16Cast(UTF8::WideArg(cleanName).c_str()));
	}

	void AetImporter::ImportAudio(const Aet::Audio& audio)
	{
		auto& audioExtraData = exData.Get(audio);

		const Aet::Layer* usedLayer = nullptr;

		// TODO: Might wanna create a lookup map instead
		workingScene.Scene->ForEachComp([&](const auto& comp)
		{
			for (const auto& layer : comp->Layers)
			{
				if (layer->GetAudioItem().get() == &audio)
					usedLayer = layer.get();
			}
		});

		const auto videoColor = AEGP_ColorVal { 0.0, 0.70, 0.78, 0.70 };
		suites.FootageSuite5->AEGP_NewSolidFootage((usedLayer != nullptr) ? usedLayer->GetName().c_str() : "unused_audio.aif", 100, 100, &videoColor, &exData.Get(audio).AE_Footage);

		if (audioExtraData.AE_Footage != nullptr)
			suites.FootageSuite5->AEGP_AddFootageToProject(audioExtraData.AE_Footage, project.Folders.Scene.Audio, &audioExtraData.AE_FootageItem);
	}

	void AetImporter::ImportSceneComps()
	{
		const auto& scene = *workingScene.Scene;

		const A_Time sceneDuration = FrameToAETime(scene.EndFrame);
		const auto sceneName = FormatSceneName(*workingSet.Set, scene);

		auto& rootCompExtraData = exData.Get(*scene.RootComposition);

		suites.CompSuite7->AEGP_CreateComp(project.Folders.Scene.Root, AEUtil::UTF16Cast(UTF8::WideArg(sceneName).c_str()), scene.Resolution.x, scene.Resolution.y, &AEUtil::OneToOneRatio, &sceneDuration, &workingScene.AE_FrameRate, &rootCompExtraData.AE_Comp);
		suites.CompSuite7->AEGP_GetItemFromComp(rootCompExtraData.AE_Comp, &rootCompExtraData.AE_CompItem);

		const auto backgroundColor = AEUtil::ColorRGB8(scene.BackgroundColor);
		suites.CompSuite7->AEGP_SetCompBGColor(rootCompExtraData.AE_Comp, &backgroundColor);

		for (const auto& comp : scene.Compositions)
			ImportComposition(*comp);

		if (workingSet.AetEntry != nullptr && workingScene.SceneIndex < workingSet.AetEntry->SceneEntries.size())
		{
			CommentUtil::Buffer commentBuffer = {};
			sprintf(commentBuffer.data(), "%s, 0x%X", scene.Name.c_str(), workingSet.AetEntry->SceneEntries[workingScene.SceneIndex].ID);
			CommentUtil::Set(suites.ItemSuite8, rootCompExtraData.AE_CompItem, { CommentUtil::Keys::AetScene, commentBuffer.data(), workingScene.SceneIndex });
		}
		else
		{
			CommentUtil::Set(suites.ItemSuite8, rootCompExtraData.AE_CompItem, { CommentUtil::Keys::AetScene, scene.Name, workingScene.SceneIndex });
		}
	}

	void AetImporter::ImportComposition(const Aet::Composition& comp, b8 videoDB)
	{
		const auto foundDuration = workingScene.GivenCompDurations.find(&comp);
		const frame_t givenDuration = (foundDuration != workingScene.GivenCompDurations.end()) ? foundDuration->second : 0.0f;
		const frame_t longestEndFrame = FindLongestLayerEndFrameInComp(comp);

		// NOTE: Neither the comp content nor the layer usage can be used to definitively recover the original comp duration
		//		 so we try to choose the longest one to minimize the risk of messing up any layer being shorter than expected
		const frame_t frameDuration = std::max(longestEndFrame, givenDuration);
		const A_Time duration = FrameToAETime((frameDuration > 0.0f) ? frameDuration : workingScene.Scene->FrameRate);

		const auto resolution = workingScene.Scene->Resolution;
		auto& compExtraData = exData.Get(comp);

		suites.CompSuite7->AEGP_CreateComp(videoDB ? project.Folders.Scene.VideoDB : project.Folders.Scene.Comp, AEUtil::UTF16Cast(UTF8::WideArg(comp.GivenName).c_str()), resolution.x, resolution.y, &AEUtil::OneToOneRatio, &duration, &workingScene.AE_FrameRate, &compExtraData.AE_Comp);
		suites.CompSuite7->AEGP_GetItemFromComp(compExtraData.AE_Comp, &compExtraData.AE_CompItem);
	}

	void AetImporter::ImportAllLayersInComp(const Aet::Composition& comp)
	{
		std::for_each(comp.Layers.rbegin(), comp.Layers.rend(), [&](const auto& layer)
		{
			ImportLayer(comp, *layer);
		});

		std::for_each(comp.Layers.begin(), comp.Layers.end(), [&](const auto& layer)
		{
			SetLayerRefParentLayer(*layer);
		});
	}

	void AetImporter::ImportLayer(const Aet::Composition& parentComp, const Aet::Layer& layer)
	{
		ImportLayerItemToComp(parentComp, layer);

		auto& layerExtraData = exData.Get(layer);
		if (layerExtraData.AE_Layer == nullptr)
			return;

		// NOTE: Make sure to import the layer flags first to ensure the 3D layer video properties won't be discarded
		ImportLayerTiming(layer, layerExtraData);
		ImportLayerName(layer, layerExtraData);
		ImportLayerFlags(layer, layerExtraData);
		ImportLayerQuality(layer, layerExtraData);
		ImportLayerMarkers(layer, layerExtraData);

		if (layer.LayerVideo != nullptr)
			ImportLayerVideo(layer, layerExtraData);

		if (layer.LayerAudio != nullptr)
			ImportLayerAudio(layer, layerExtraData);
	}

	void AetImporter::ImportLayerItemToComp(const Aet::Composition& parentComp, const Aet::Layer& layer)
	{
		auto tryAddAEFootageLayerToComp = [&](const auto* layerItem)
		{
			if (layerItem == nullptr)
				return;

			auto& layerItemExtraData = exData.Get(*layerItem);
			if (layerItemExtraData.AE_FootageItem != nullptr)
				suites.LayerSuite7->AEGP_AddLayer(layerItemExtraData.AE_FootageItem, exData.Get(parentComp).AE_Comp, &exData.Get(layer).AE_Layer);
		};

		auto tryAddAECompLayerToComp = [&](const auto* layerItem)
		{
			if (layerItem == nullptr)
				return;

			auto& layerItemExtraData = exData.Get(*layerItem);
			if (layerItemExtraData.AE_CompItem != nullptr)
				suites.LayerSuite7->AEGP_AddLayer(layerItemExtraData.AE_CompItem, exData.Get(parentComp).AE_Comp, &exData.Get(layer).AE_Layer);
		};

		if (layer.ItemType == Aet::ItemType::Video)
			tryAddAEFootageLayerToComp(layer.GetVideoItem());
		else if (layer.ItemType == Aet::ItemType::Audio)
			tryAddAEFootageLayerToComp(layer.GetAudioItem());
		else if (layer.ItemType == Aet::ItemType::Composition)
			tryAddAECompLayerToComp(layer.GetCompItem());
	}

	std::unordered_map<const Aet::Composition*, frame_t> AetImporter::CreateGivenCompDurationsMap(const Aet::Scene& scene)
	{
		std::unordered_map<const Aet::Composition*, frame_t> givenCompDurations;

		auto registerGivenCompDurations = [&givenCompDurations](const Aet::Composition& compToRegister)
		{
			for (const auto& layer : compToRegister.Layers)
			{
				if (layer->ItemType != Aet::ItemType::Composition || layer->GetCompItem() == nullptr)
					continue;

				frame_t& mapDuration = givenCompDurations[layer->GetCompItem().get()];
				mapDuration = std::max(layer->EndFrame, mapDuration);
			}
		};

		givenCompDurations.reserve(scene.Compositions.size() + 1);
		givenCompDurations[scene.RootComposition.get()] = scene.EndFrame;

		registerGivenCompDurations(*scene.RootComposition);

		for (const auto& comp : scene.Compositions)
			registerGivenCompDurations(*comp);

		return givenCompDurations;
	}

	frame_t AetImporter::FindLongestLayerEndFrameInComp(const Aet::Composition& comp)
	{
		frame_t longestTime = 0.0f;
		for (const auto& layer : comp.Layers)
		{
			if (layer->EndFrame > longestTime)
				longestTime = layer->EndFrame;
		}
		return longestTime;
	}

	b8 AetImporter::LayerMakesUseOfStartOffset(const Aet::Layer& layer)
	{
		if (layer.ItemType == Aet::ItemType::Video)
		{
			if (layer.GetVideoItem() != nullptr && layer.GetVideoItem()->Sources.size() > 1)
				return true;

			return false;
		}
		else if (layer.ItemType == Aet::ItemType::Composition)
			return true;
		else
			return false;
	}

	void AetImporter::ImportLayerVideo(const Aet::Layer& layer, AetExDataTag& layerExtraData)
	{
		ImportLayerTransferMode(layer, layer.LayerVideo->TransferMode);
		ImportLayerVideoStream(layer, *layer.LayerVideo);
	}

	void AetImporter::ImportLayerTransferMode(const Aet::Layer& layer, const Aet::LayerTransferMode& transferMode)
	{
		AEGP_LayerTransferMode layerTransferMode = {};
		layerTransferMode.mode = (static_cast<PF_TransferMode>(transferMode.BlendMode) - 1);
		layerTransferMode.flags = static_cast<AEGP_TransferFlags>(*reinterpret_cast<const u8*>(&transferMode.Flags));
		layerTransferMode.track_matte = static_cast<AEGP_TrackMatte>(transferMode.TrackMatte);

		suites.LayerSuite7->AEGP_SetLayerTransferMode(exData.Get(layer).AE_Layer, &layerTransferMode);
	}

	struct AEKeyFrameHelper
	{
		AEKeyFrameHelper(SuitesData& suites, AEGP_LayerH layer, AEGP_LayerStream streamType) : suites(suites)
		{
			suites.StreamSuite4->AEGP_GetNewLayerStream(Global.PluginID, layer, streamType, &streamValue2.streamH);
			valueScaleFactor = StreamUtil::GetAetToAEStreamFactor(streamType);
		}

		void SetValue(vec3 value)
		{
			streamValue2.val.three_d.x = value.x * valueScaleFactor;
			streamValue2.val.three_d.y = value.y * valueScaleFactor;
			streamValue2.val.three_d.z = value.z * valueScaleFactor;
			suites.StreamSuite4->AEGP_SetStreamValue(Global.PluginID, streamValue2.streamH, &streamValue2);
		}

		void Start()
		{
			suites.KeyframeSuite3->AEGP_StartAddKeyframes(streamValue2.streamH, &addKeyFrameInfo);
		}

		void AddKeyFrame(A_Time time, vec3 value)
		{
			AEGP_KeyframeIndex index;
			suites.KeyframeSuite3->AEGP_AddKeyframes(addKeyFrameInfo, AEGP_LTimeMode_LayerTime, &time, &index);

			AEGP_StreamValue streamValue = {};
			streamValue.streamH = streamValue2.streamH;
			streamValue.val.three_d.x = value.x * valueScaleFactor;
			streamValue.val.three_d.y = value.y * valueScaleFactor;
			streamValue.val.three_d.z = value.z * valueScaleFactor;
			suites.KeyframeSuite3->AEGP_SetAddKeyframe(addKeyFrameInfo, index, &streamValue);
		}

		void End()
		{
			suites.KeyframeSuite3->AEGP_EndAddKeyframes(true, addKeyFrameInfo);
		}

	private:
		SuitesData& suites;
		f32 valueScaleFactor;
		AEGP_StreamValue2 streamValue2;
		AEGP_AddKeyframesInfoH addKeyFrameInfo;
	};

	static f32 GetInitialValue(const Aet::FCurve* fcurve) { return (fcurve == nullptr || fcurve->Keys.empty()) ? 0.0f : fcurve->Keys.front().Value; }

	template <class KeyFrameType>
	static const KeyFrameType* FindKeyFrameAt(frame_t frame, const std::vector<KeyFrameType>& keyFrames) { return FindIfOrNull(keyFrames, [&](auto& k) { return ApproxmiatelySame(k.Frame, frame, 0.001f); }); }

	template <class KeyFrameType>
	static const KeyFrameType* FindKeyFrameAt(frame_t frame, const Aet::FCurve* fcurve) { return (fcurve != nullptr) ? FindKeyFrameAt(frame, fcurve->Keys) : nullptr; }

	static f32 TrySampleFCurveAt(const Aet::FCurve* fcurve, frame_t frame) { return (fcurve != nullptr) ? fcurve->SampleAt(frame) : 0.0f; }

	void AetImporter::ImportLayerVideoStream(const Aet::Layer& layer, const Aet::LayerVideo& layerVideo)
	{
		const auto& layerExtraData = exData.Get(layer);

		// TODO: Is this correct (?)
		const auto keyFrameStartOffset = (LayerMakesUseOfStartOffset(layer) ? layer.StartOffset : 0.0f) - layer.StartFrame;

		auto importStream = [&](AEGP_LayerStream streamType, const auto propX, const auto propY, const auto propZ)
		{
			if (propX == nullptr)
				return;

			AEKeyFrameHelper keyFramer = { suites, layerExtraData.AE_Layer, streamType };
			keyFramer.SetValue(vec3(GetInitialValue(propX), GetInitialValue(propY), GetInitialValue(propZ)));

			if (CombinePropertyKeyFrames(propX, propY, propZ, combinedKeyFramesCache); combinedKeyFramesCache.size() > 1)
			{
				keyFramer.Start();
				for (const auto& keyFrame : combinedKeyFramesCache)
					keyFramer.AddKeyFrame(FrameToAETime(keyFrame.Frame + keyFrameStartOffset), keyFrame.Value);
				keyFramer.End();
			}
		};

		const auto combinedLayerVideo = StreamUtil::CombinedAetLayerVideo2D3D(*const_cast<Aet::LayerVideo*>(&layerVideo));
		importStream(AEGP_LayerStream_ANCHORPOINT, combinedLayerVideo.OriginX, combinedLayerVideo.OriginY, combinedLayerVideo.OriginZ);
		importStream(AEGP_LayerStream_POSITION, combinedLayerVideo.PositionX, combinedLayerVideo.PositionY, combinedLayerVideo.PositionZ);
		importStream(AEGP_LayerStream_ROTATE_X, combinedLayerVideo.RotationX, nullptr, nullptr);
		importStream(AEGP_LayerStream_ROTATE_Y, combinedLayerVideo.RotationY, nullptr, nullptr);
		importStream(AEGP_LayerStream_ROTATE_Z, combinedLayerVideo.RotationZ, nullptr, nullptr);
		importStream(AEGP_LayerStream_SCALE, combinedLayerVideo.ScaleX, combinedLayerVideo.ScaleY, combinedLayerVideo.ScaleZ);
		importStream(AEGP_LayerStream_OPACITY, combinedLayerVideo.Opacity, nullptr, nullptr);
		importStream(AEGP_LayerStream_ORIENTATION, combinedLayerVideo.DirectionX, combinedLayerVideo.DirectionY, combinedLayerVideo.DirectionZ);
	}

	void AetImporter::CombinePropertyKeyFrames(const Aet::FCurve* curveX, const Aet::FCurve* curveY, const Aet::FCurve* curveZ, std::vector<KeyFrameVec3>& outCombined) const
	{
		assert(curveX != nullptr);

		outCombined.clear();
		outCombined.reserve(curveX->Keys.size());

		if (curveY == nullptr && curveZ == nullptr)
		{
			for (const auto& xKeyFrame : curveX->Keys)
				outCombined.push_back({ xKeyFrame.Frame, vec3(xKeyFrame.Value, 0.0f, 0.0f), xKeyFrame.Tangent });
			return;
		}

		for (const auto& xKeyFrame : curveX->Keys)
		{
			const auto matchingYKeyFrame = FindKeyFrameAt<Aet::KeyFrame>(xKeyFrame.Frame, curveY);
			const auto matchingZKeyFrame = FindKeyFrameAt<Aet::KeyFrame>(xKeyFrame.Frame, curveZ);
			const vec3 value =
			{
				xKeyFrame.Value,
				(matchingYKeyFrame != nullptr) ? matchingYKeyFrame->Value : TrySampleFCurveAt(curveY, xKeyFrame.Frame),
				(matchingZKeyFrame != nullptr) ? matchingZKeyFrame->Value : TrySampleFCurveAt(curveZ, xKeyFrame.Frame),
			};
			outCombined.push_back({ xKeyFrame.Frame, value, xKeyFrame.Tangent });
		}

		if (curveY != nullptr)
		{
			for (const auto& yKeyFrame : curveY->Keys)
			{
				if (auto existingKeyFrame = FindKeyFrameAt<KeyFrameVec3>(yKeyFrame.Frame, outCombined); existingKeyFrame != nullptr)
					continue;

				const f32 xValue = TrySampleFCurveAt(curveX, yKeyFrame.Frame);
				const f32 zValue = TrySampleFCurveAt(curveZ, yKeyFrame.Frame);
				outCombined.push_back({ yKeyFrame.Frame, vec3(xValue, yKeyFrame.Value, zValue), yKeyFrame.Tangent });
			}
		}

		if (curveZ != nullptr)
		{
			for (const auto& zKeyFrame : curveZ->Keys)
			{
				if (auto existingKeyFrame = FindKeyFrameAt<KeyFrameVec3>(zKeyFrame.Frame, outCombined); existingKeyFrame != nullptr)
					continue;

				const f32 xValue = TrySampleFCurveAt(curveX, zKeyFrame.Frame);
				const f32 yValue = TrySampleFCurveAt(curveY, zKeyFrame.Frame);
				outCombined.push_back({ zKeyFrame.Frame, vec3(xValue, yValue, zKeyFrame.Value), zKeyFrame.Tangent });
			}
		}

		std::sort(outCombined.begin(), outCombined.end(), [](const auto& a, const auto& b) { return a.Frame < b.Frame; });
	}

	std::string_view AetImporter::GetLayerItemName(const Aet::Layer& layer) const
	{
		if (layer.GetVideoItem() != nullptr)
		{
			if (layer.GetVideoItem()->Sources.size() == 1)
				return layer.GetVideoItem()->Sources.front().Name;
			else if (auto layerUsage = workingScene.SourcelessVideoLayerUsages.find(layer.GetVideoItem()); layerUsage != workingScene.SourcelessVideoLayerUsages.end())
				return layerUsage->second->GetName();
			else
				return "";
		}
		else if (layer.GetAudioItem() != nullptr)
			return "";
		else if (layer.GetCompItem() != nullptr)
			return layer.GetCompItem()->GivenName;
		else
			return "";
	}

	void AetImporter::ImportLayerAudio(const Aet::Layer& layer, AetExDataTag& layerExtraData)
	{
		// NOTE: This is unused by the game so this can be ignored for now
	}

	void AetImporter::ImportLayerTiming(const Aet::Layer& layer, AetExDataTag& layerExtraData)
	{
		const frame_t startOffset = LayerMakesUseOfStartOffset(layer) ? layer.StartOffset : 0.0f;

		const frame_t frameLayerDuration = (layer.EndFrame - layer.StartFrame);
		const frame_t frameCompDuration = (frameLayerDuration + startOffset);

		const A_Time inPoint = FrameToAETime(startOffset);
		const A_Time duration = FrameToAETime(frameCompDuration * layer.TimeScale);
		suites.LayerSuite7->AEGP_SetLayerInPointAndDuration(layerExtraData.AE_Layer, AEGP_LTimeMode_CompTime, &inPoint, &duration);

		const A_Time offset = FrameToAETime(layer.StartFrame - startOffset);
		suites.LayerSuite7->AEGP_SetLayerOffset(layerExtraData.AE_Layer, &offset);

		if (layer.TimeScale != 1.0f)
		{
			const A_Ratio stretch = AEUtil::Ratio(1.0f / layer.TimeScale);
			suites.LayerSuite1->AEGP_SetLayerStretch(layerExtraData.AE_Layer, &stretch);
		}
	}

	void AetImporter::ImportLayerName(const Aet::Layer& layer, AetExDataTag& layerExtraData)
	{
		// NOTE: To ensure square brackets appear around the layer name in the layer name view
		if (GetLayerItemName(layer) != layer.GetName())
			suites.LayerSuite1->AEGP_SetLayerName(layerExtraData.AE_Layer, layer.GetName().c_str());
	}

	void AetImporter::ImportLayerFlags(const Aet::Layer& layer, AetExDataTag& layerExtraData)
	{
		const u16 layerFlags = *reinterpret_cast<const u16*>(&layer.Flags);
		for (size_t flagsBitIndex = 0; flagsBitIndex < sizeof(layerFlags) * CHAR_BIT; flagsBitIndex++)
		{
			const u16 flagsBitMask = (1 << flagsBitIndex);
			suites.LayerSuite1->AEGP_SetLayerFlag(layerExtraData.AE_Layer, static_cast<AEGP_LayerFlags>(flagsBitMask), static_cast<A_Boolean>(layerFlags & flagsBitMask));
		}

		// NOTE: Prevent the dummy import solid from being visible
		if (layer.ItemType == Aet::ItemType::Audio)
			suites.LayerSuite1->AEGP_SetLayerFlag(layerExtraData.AE_Layer, AEGP_LayerFlag_VIDEO_ACTIVE, false);

		// NOTE: Makes sure underlying transfer modes etc are being preserved as well as the underlying layers aren't being cut off outside the comp region
		if (layer.ItemType == Aet::ItemType::Composition)
			suites.LayerSuite1->AEGP_SetLayerFlag(layerExtraData.AE_Layer, AEGP_LayerFlag_COLLAPSE, true);

		if (layer.LayerVideo != nullptr && layer.LayerVideo->Transform3D != nullptr)
			suites.LayerSuite1->AEGP_SetLayerFlag(layerExtraData.AE_Layer, AEGP_LayerFlag_LAYER_IS_3D, true);
	}

	void AetImporter::ImportLayerQuality(const Aet::Layer& layer, AetExDataTag& layerExtraData)
	{
		suites.LayerSuite1->AEGP_SetLayerQuality(layerExtraData.AE_Layer, (static_cast<AEGP_LayerQuality>(layer.Quality) - 1));
	}

	void AetImporter::ImportLayerMarkers(const Aet::Layer& layer, AetExDataTag& layerExtraData)
	{
		if (layer.Markers.empty())
			return;

		AEGP_StreamValue2 streamValue2 = {};
		suites.StreamSuite4->AEGP_GetNewLayerStream(Global.PluginID, layerExtraData.AE_Layer, AEGP_LayerStream_MARKER, &streamValue2.streamH);

		for (const auto& marker : layer.Markers)
		{
			AEGP_MarkerVal markerValue = {};
			AEGP_MarkerVal* markerValuePtr = &markerValue;
			std::memcpy(markerValue.nameAC, marker->Name.data(), marker->Name.size());

			AEGP_StreamValue streamValue = {};
			streamValue.streamH = streamValue2.streamH;
			streamValue.val.markerH = &markerValuePtr;

			const A_Time time = FrameToAETime(marker->Frame);
			AEGP_KeyframeIndex index;

			// NOTE: AEGP_StartAddKeyframes / AEGP_EndAddKeyframes is not supported for markers
			// suites.KeyframeSuite3->AEGP_InsertKeyframe(streamValue.streamH, AEGP_LTimeMode_LayerTime, &time, &index);
			suites.KeyframeSuite3->AEGP_InsertKeyframe(streamValue.streamH, AEGP_LTimeMode_CompTime, &time, &index);
			suites.KeyframeSuite3->AEGP_SetKeyframeValue(streamValue.streamH, index, &streamValue);
		}
	}

	void AetImporter::SetLayerRefParentLayer(const Aet::Layer& layer)
	{
		if (layer.GetRefParentLayer() == nullptr)
			return;

		auto& layerExtraData = exData.Get(layer);
		if (layerExtraData.AE_Layer == nullptr)
			return;

		auto& parentLayerExtraData = exData.Get(*layer.GetRefParentLayer());
		if (parentLayerExtraData.AE_Layer == nullptr)
			return;

		suites.LayerSuite7->AEGP_SetLayerParent(layerExtraData.AE_Layer, parentLayerExtraData.AE_Layer);
	}
}
