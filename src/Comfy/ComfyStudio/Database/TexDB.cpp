#include "TexDB.h"
#include "FileSystem/BinaryReader.h"
#include "FileSystem/BinaryWriter.h"

namespace Comfy::Database
{
	using namespace FileSystem;

	void TexDB::Read(BinaryReader& reader)
	{
		uint32_t texEntryCount = reader.ReadU32();
		FileAddr texOffset = reader.ReadPtr();

		if (texEntryCount > 0 && texOffset != FileAddr::NullPtr)
		{
			Entries.resize(texEntryCount);
			reader.ReadAt(texOffset, [this](BinaryReader& reader)
			{
				for (auto& texEntry : Entries)
				{
					texEntry.ID = TexID(reader.ReadU32());
					texEntry.Name = reader.ReadStrPtr();
				}
			});
		}
	}

	void TexDB::Write(BinaryWriter& writer)
	{
		writer.WriteU32(static_cast<uint32_t>(Entries.size()));
		writer.WritePtr([this](BinaryWriter& writer)
		{
			for (auto& texEntry : Entries)
			{
				writer.WriteU32(static_cast<uint32_t>(texEntry.ID));
				writer.WriteStrPtr(texEntry.Name);
			}

			writer.WriteAlignmentPadding(16);
			writer.WritePadding(16);
		});

		writer.FlushPointerPool();
		writer.WriteAlignmentPadding(16);

		writer.FlushStringPointerPool();
		writer.WriteAlignmentPadding(16);
	}
	
	const TexEntry* TexDB::GetTexEntry(TexID id) const
	{
		auto found = std::find_if(Entries.begin(), Entries.end(), [id](auto& e) { return e.ID == id; });
		return (found == Entries.end()) ? nullptr : &(*found);
	}

	const TexEntry* TexDB::GetTexEntry(std::string_view name) const
	{
		auto found = std::find_if(Entries.begin(), Entries.end(), [name](auto& e) { return e.Name == name; });
		return (found == Entries.end()) ? nullptr : &(*found);
	}
}
