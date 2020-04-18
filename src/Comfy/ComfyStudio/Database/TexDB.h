#pragma once
#include "Database.h"

namespace Comfy::Database
{
	struct TexEntry : BinaryDatabase::Entry
	{
		TexID ID;
		std::string Name;
	};

	class TexDB final : public BinaryDatabase
	{
	public:
		std::vector<TexEntry> Entries;

		void Read(FileSystem::BinaryReader& reader) override;
		void Write(FileSystem::BinaryWriter& writer) override;

		const TexEntry* GetTexEntry(TexID id) const;
		const TexEntry* GetTexEntry(std::string_view name) const;

	private:
	};
}
