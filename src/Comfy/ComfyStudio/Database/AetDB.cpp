#include "AetDB.h"
#include "FileSystem/BinaryReader.h"
#include "FileSystem/BinaryWriter.h"

namespace Comfy::Database
{
	using namespace FileSystem;

	AetSceneEntry* AetSetEntry::GetSceneEntry(std::string_view name)
	{
		for (auto& entry : SceneEntries)
			if (entry.Name == name)
				return &entry;
		return nullptr;
	}

	void AetDB::Read(BinaryReader& reader)
	{
		uint32_t setEntryCount = reader.ReadU32();
		FileAddr setOffset = reader.ReadPtr();

		uint32_t sceneCount = reader.ReadU32();
		FileAddr sceneOffset = reader.ReadPtr();

		if (setEntryCount > 0 && setOffset != FileAddr::NullPtr)
		{
			Entries.resize(setEntryCount);
			reader.ReadAt(setOffset, [this](BinaryReader& reader)
			{
				for (auto& setEntry : Entries)
				{
					setEntry.ID = AetSetID(reader.ReadU32());
					setEntry.Name = reader.ReadStrPtr();
					setEntry.FileName = reader.ReadStrPtr();
					uint32_t index = reader.ReadU32();
					setEntry.SprSetID = SprSetID(reader.ReadU32());
				}
			});
		}

		if (sceneCount > 0 && sceneOffset != FileAddr::NullPtr)
		{
			reader.ReadAt(sceneOffset, [this, sceneCount](BinaryReader& reader)
			{
				for (uint32_t i = 0; i < sceneCount; i++)
				{
					AetSceneID id = AetSceneID(reader.ReadU32());
					FileAddr nameOffset = reader.ReadPtr();
					uint16_t sceneIndex = reader.ReadU16();
					uint16_t setIndex = reader.ReadU16();

					AetSetEntry& setEntry = Entries[setIndex];

					setEntry.SceneEntries.emplace_back();
					AetSceneEntry& sceneEntry = setEntry.SceneEntries.back();

					sceneEntry.ID = id;
					sceneEntry.Name = reader.ReadStrAt(nameOffset);
				}
			});
		}
	}

	void AetDB::Write(BinaryWriter& writer)
	{
		writer.WriteU32(static_cast<uint32_t>(Entries.size()));
		writer.WritePtr([&](BinaryWriter& writer)
		{
			uint32_t setIndex = 0;
			for (const auto& setEntry : Entries)
			{
				writer.WriteU32(static_cast<uint32_t>(setEntry.ID));
				writer.WriteStrPtr(setEntry.Name);
				writer.WriteStrPtr(setEntry.FileName);
				writer.WriteU32(setIndex++);
				writer.WriteU32(static_cast<uint32_t>(setEntry.SprSetID));
			}
		});

		size_t sceneCount = 0;
		for (const auto& setEntry : Entries)
			sceneCount += setEntry.SceneEntries.size();

		writer.WriteU32(static_cast<uint32_t>(sceneCount));
		writer.WritePtr([&](BinaryWriter& writer)
		{
			uint16_t setIndex = 0;
			for (const auto& setEntry : Entries)
			{
				uint16_t sceneIndex = 0;
				for (const auto& sceneEntry : setEntry.SceneEntries)
				{
					writer.WriteU32(static_cast<uint32_t>(sceneEntry.ID));
					writer.WriteStrPtr(sceneEntry.Name);
					writer.WriteU16(sceneIndex);
					writer.WriteU16(setIndex);
					sceneIndex++;
				}
				setIndex++;
			}
		});

		writer.WritePadding(16);
		writer.WriteAlignmentPadding(16);

		writer.FlushPointerPool();
		writer.WriteAlignmentPadding(16);

		writer.FlushStringPointerPool();
		writer.WriteAlignmentPadding(16);
	}

	AetSetEntry* AetDB::GetAetSetEntry(std::string_view name)
	{
		for (auto& entry : Entries)
			if (entry.Name == name)
				return &entry;
		return nullptr;
	}
}
