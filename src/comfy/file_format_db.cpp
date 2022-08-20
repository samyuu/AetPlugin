#include "file_format_db.h"

namespace Comfy
{
	StreamResult AetDB::Read(StreamReader& reader)
	{
		const auto baseHeader = SectionHeader::TryRead(reader, SectionSignature::AEDB);
		SectionHeader::ScanPOFSectionsForPtrSize(reader);

		if (baseHeader.has_value())
		{
			reader.SetEndianness(baseHeader->Endianness);
			reader.Seek(baseHeader->StartOfSubSectionAddress());

			if (reader.GetPtrSize() == PtrSize::Mode64Bit)
				reader.PushBaseOffset();
		}

		const auto setEntryCount = reader.ReadU32();
		if (reader.GetPtrSize() == PtrSize::Mode64Bit)
			reader.Skip(static_cast<FileAddr>(sizeof(u32)));
		const auto setOffset = reader.ReadPtr();

		const auto sceneCount = reader.ReadU32();
		if (reader.GetPtrSize() == PtrSize::Mode64Bit)
			reader.Skip(static_cast<FileAddr>(sizeof(u32)));
		const auto sceneOffset = reader.ReadPtr();

		if (setEntryCount > 0)
		{
			if (!reader.IsValidPointer(setOffset))
				return StreamResult::BadPointer;

			Entries.resize(setEntryCount);
			reader.ReadAtOffsetAware(setOffset, [this](StreamReader& reader)
			{
				for (auto& setEntry : Entries)
				{
					setEntry.ID = AetSetID(reader.ReadU32());
					if (reader.GetPtrSize() == PtrSize::Mode64Bit)
						reader.Skip(static_cast<FileAddr>(sizeof(u32)));

					setEntry.Name = reader.ReadStrPtrOffsetAware();
					setEntry.FileName = reader.ReadStrPtrOffsetAware();
					const auto index = reader.ReadU32();
					setEntry.SprSetID = SprSetID(reader.ReadU32());
				}
			});
		}

		if (sceneCount > 0)
		{
			if (!reader.IsValidPointer(sceneOffset))
				return StreamResult::BadPointer;

			reader.ReadAtOffsetAware(sceneOffset, [this, sceneCount](StreamReader& reader)
			{
				for (u32 i = 0; i < sceneCount; i++)
				{
					const auto id = AetSceneID(reader.ReadU32());
					if (reader.GetPtrSize() == PtrSize::Mode64Bit)
						reader.Skip(static_cast<FileAddr>(sizeof(u32)));

					const auto nameOffset = reader.ReadPtr();
					const auto packedData = reader.ReadU32();

					const auto sceneIndex = static_cast<u16>(packedData & 0xFFFF);
					const auto setIndex = static_cast<u16>((packedData >> 16) & 0xFFFF);

					assert(InBounds(setIndex, Entries));
					auto& setEntry = Entries[setIndex];
					auto& sceneEntry = setEntry.SceneEntries.emplace_back();

					sceneEntry.ID = id;
					sceneEntry.Name = reader.ReadStrAtOffsetAware(nameOffset);
					sceneEntry.Index = sceneIndex;
				}
			});
		}

		if (baseHeader.has_value() && reader.GetPtrSize() == PtrSize::Mode64Bit)
			reader.PopBaseOffset();

		return StreamResult::Success;
	}

	StreamResult AetDB::Write(StreamWriter& writer)
	{
		writer.WriteU32(static_cast<u32>(Entries.size()));
		writer.WriteFuncPtr([&](StreamWriter& writer)
		{
			u32 setIndex = 0;
			for (const auto& setEntry : Entries)
			{
				writer.WriteU32(static_cast<u32>(setEntry.ID));
				writer.WriteStrPtr(setEntry.Name);
				writer.WriteStrPtr(setEntry.FileName);
				writer.WriteU32(setIndex++);
				writer.WriteU32(static_cast<u32>(setEntry.SprSetID));
			}
		});

		size_t sceneCount = 0;
		for (const auto& setEntry : Entries)
			sceneCount += setEntry.SceneEntries.size();

		writer.WriteU32(static_cast<u32>(sceneCount));
		writer.WriteFuncPtr([&](StreamWriter& writer)
		{
			u16 setIndex = 0;
			for (const auto& setEntry : Entries)
			{
				u16 sceneIndex = 0;
				for (const auto& sceneEntry : setEntry.SceneEntries)
				{
					writer.WriteU32(static_cast<u32>(sceneEntry.ID));
					writer.WriteStrPtr(sceneEntry.Name);
					writer.WriteU32(static_cast<u32>(setIndex << 16) | static_cast<u32>(sceneEntry.Index));
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

		return StreamResult::Success;
	}

	StreamResult SprDB::Read(StreamReader& reader)
	{
		const auto baseHeader = SectionHeader::TryRead(reader, SectionSignature::SPDB);
		SectionHeader::ScanPOFSectionsForPtrSize(reader);

		if (baseHeader.has_value())
		{
			reader.SetEndianness(baseHeader->Endianness);
			reader.Seek(baseHeader->StartOfSubSectionAddress());

			if (reader.GetPtrSize() == PtrSize::Mode64Bit)
				reader.PushBaseOffset();
		}

		const auto sprSetEntryCount = reader.ReadU32();
		if (reader.GetPtrSize() == PtrSize::Mode64Bit)
			reader.Skip(static_cast<FileAddr>(sizeof(u32)));
		const auto sprSetOffset = reader.ReadPtr();

		const auto sprEntryCount = reader.ReadU32();
		if (reader.GetPtrSize() == PtrSize::Mode64Bit)
			reader.Skip(static_cast<FileAddr>(sizeof(u32)));
		const auto sprOffset = reader.ReadPtr();

		if (sprSetEntryCount > 0)
		{
			if (!reader.IsValidPointer(sprSetOffset))
				return StreamResult::BadPointer;

			Entries.resize(sprSetEntryCount);
			reader.ReadAtOffsetAware(sprSetOffset, [this](StreamReader& reader)
			{
				for (auto& sprSetEntry : Entries)
				{
					sprSetEntry.ID = SprSetID(reader.ReadU32());
					if (reader.GetPtrSize() == PtrSize::Mode64Bit)
						reader.Skip(static_cast<FileAddr>(sizeof(u32)));

					sprSetEntry.Name = reader.ReadStrPtrOffsetAware();
					sprSetEntry.FileName = reader.ReadStrPtrOffsetAware();
					const auto index = reader.ReadI32();
				}
			});
		}

		if (sprEntryCount > 0)
		{
			if (!reader.IsValidPointer(sprOffset))
				return StreamResult::BadPointer;

			reader.ReadAtOffsetAware(sprOffset, [this, sprEntryCount](StreamReader& reader)
			{
				for (u32 i = 0; i < sprEntryCount; i++)
				{
					const auto id = SprID(reader.ReadU32());
					if (reader.GetPtrSize() == PtrSize::Mode64Bit)
						reader.Skip(static_cast<FileAddr>(sizeof(u32)));

					const auto nameOffset = reader.ReadPtr();
					const auto packedData = reader.ReadU32();

					const auto sprIndex = (packedData & 0xFFFF);
					const auto sprSetIndex = (static_cast<u16>((packedData >> 16) & 0xFFFF) & 0xFFF);
					const auto isTexEntry = ((static_cast<u16>((packedData >> 16) & 0xFFFF) & 0x1000) == 0x1000);

					assert(InBounds(sprSetIndex, Entries));
					auto& sprSetEntry = Entries[sprSetIndex];
					auto& sprEntry = (isTexEntry ? sprSetEntry.SprTexEntries : sprSetEntry.SprEntries).emplace_back();

					sprEntry.ID = id;
					sprEntry.Name = reader.ReadStrAtOffsetAware(nameOffset);
					sprEntry.Index = sprIndex;
					assert(ASCII::StartsWith(sprEntry.Name, isTexEntry ? "SPRTEX_" : "SPR_"));

					if (reader.GetPtrSize() == PtrSize::Mode64Bit)
						reader.Skip(static_cast<FileAddr>(sizeof(u32)));
				}
			});
		}

		if (baseHeader.has_value() && reader.GetPtrSize() == PtrSize::Mode64Bit)
			reader.PopBaseOffset();

		return StreamResult::Success;
	}

	StreamResult SprDB::Write(StreamWriter& writer)
	{
		const auto startPosition = writer.GetPosition();

		writer.WriteU32(GetSprSetEntryCount());
		writer.WritePtr(FileAddr::NullPtr);
		writer.WriteU32(GetSprEntryCount());
		writer.WritePtr(FileAddr::NullPtr);

		writer.Seek(startPosition + FileAddr(0xC));
		writer.WriteFuncPtr([this](StreamWriter& writer)
		{
			i16 sprSetIndex = 0;
			for (auto& sprSetEntry : Entries)
			{
				for (auto& sprTexEntry : sprSetEntry.SprTexEntries)
				{
					writer.WriteU32(static_cast<u32>(sprTexEntry.ID));
					writer.WriteStrPtr(sprTexEntry.Name);
					writer.WriteU32(static_cast<u32>(sprTexEntry.Index) | ((static_cast<u32>(sprSetIndex) | 0x1000) << 16));
				}

				i16 sprIndex = 0;
				for (auto& sprEntry : sprSetEntry.SprEntries)
				{
					writer.WriteU32(static_cast<u32>(sprEntry.ID));
					writer.WriteStrPtr(sprEntry.Name);
					writer.WriteU32(static_cast<u32>(sprEntry.Index) | static_cast<u32>(sprSetIndex << 16));
				}

				sprSetIndex++;
			}
			writer.WriteAlignmentPadding(16);
			writer.WritePadding(16);
		});

		writer.Seek(startPosition + FileAddr(0x4));
		writer.WriteFuncPtr([this](StreamWriter& writer)
		{
			i32 index = 0;
			for (auto& sprSetEntry : Entries)
			{
				writer.WriteU32(static_cast<u32>(sprSetEntry.ID));
				writer.WriteStrPtr(sprSetEntry.Name);
				writer.WriteStrPtr(sprSetEntry.FileName);
				writer.WriteI32(index++);
			}
			writer.WriteAlignmentPadding(16);
			writer.WritePadding(16);
		});

		writer.Seek(startPosition + FileAddr(0x10));
		writer.WritePadding(16);

		writer.FlushPointerPool();
		writer.WriteAlignmentPadding(16);

		writer.FlushStringPointerPool();
		writer.WriteAlignmentPadding(16);

		return StreamResult::Success;
	}
}
