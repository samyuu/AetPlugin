#pragma once
#include "Database.h"

namespace Comfy::Database
{
	struct SprEntry : BinaryDatabase::Entry
	{
		SprID ID;
		std::string Name;
		int16_t Index;
	};

	struct SprSetEntry : BinaryDatabase::Entry, BinaryDatabase::FileEntry
	{
		SprSetID ID;
		std::string Name;
		std::string FileName;

		std::vector<SprEntry> SprEntries;
		std::vector<SprEntry> SprTexEntries;

		SprEntry* GetSprEntry(SprID id);
		SprEntry* GetSprEntry(std::string_view name);
		SprEntry* GetSprTexEntry(std::string_view name);
	};

	class SprDB final : public BinaryDatabase
	{
	public:
		std::vector<SprSetEntry> Entries;

		void Read(FileSystem::BinaryReader& reader) override;
		void Write(FileSystem::BinaryWriter& writer) override;
		SprSetEntry* GetSprSetEntry(std::string_view name);

		uint32_t GetSprSetEntryCount();
		uint32_t GetSprEntryCount();

	private:
	};
}
