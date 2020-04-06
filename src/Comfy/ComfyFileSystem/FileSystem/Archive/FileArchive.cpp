#include "FileArchive.h"
#include "Misc/StringHelper.h"

namespace Comfy::FileSystem
{
	ArchiveEntry::ArchiveEntry(FileArchive* parent) : parent(parent)
	{
		assert(parent != nullptr);
	}

	std::vector<uint8_t> ArchiveEntry::ReadVector() const
	{
		return parent->ReadArchiveEntryIntoVector(*this);
	}

	void ArchiveEntry::Read(void* fileContentOut) const
	{
		parent->ReadArchiveEntry(*this, fileContentOut);
	}

	size_t FileArchive::size() const
	{
		return archiveEntries.size();
	}

	const ArchiveEntry* FileArchive::GetFile(std::string_view name) const
	{
		for (const ArchiveEntry& entry : archiveEntries)
		{
			// NOTE: Should this be case insensitive? Maybe.
			// if (entry.Name == name)

			if (MatchesInsensitive(entry.Name, name))
				return &entry;
		}

		return nullptr;
	}

	std::vector<uint8_t> FileArchive::ReadArchiveEntryIntoVector(const ArchiveEntry& entry)
	{
		std::vector<uint8_t> fileData(entry.FileSize);
		ReadArchiveEntry(entry, fileData.data());
		return fileData;
	}
}
