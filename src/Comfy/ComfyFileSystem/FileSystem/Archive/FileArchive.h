#pragma once
#include "Types.h"
#include "CoreTypes.h"

namespace Comfy::FileSystem
{
	class FileArchive;

	class ArchiveEntry
	{
	public:
		ArchiveEntry(FileArchive* parent);

	public:
		std::string Name;
		uint64_t FileOffset;
		uint64_t CompressedSize;
		uint64_t FileSize;

		std::vector<uint8_t> ReadVector() const;
		void Read(void* fileContentOut) const;

	private:
		FileArchive* parent;
	};

	class FileArchive
	{
		friend class ArchiveEntry;
	
	public:
		inline auto begin() const { return archiveEntries.begin(); };
		inline auto end() const { return archiveEntries.end(); };
		size_t size() const;

		const ArchiveEntry* GetFile(std::string_view name) const;

	protected:
		std::vector<ArchiveEntry> archiveEntries;

		std::vector<uint8_t> ReadArchiveEntryIntoVector(const ArchiveEntry& entry);
		virtual void ReadArchiveEntry(const ArchiveEntry& entry, void* fileContentOut) = 0;
	};
}
