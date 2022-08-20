#pragma once
#include "core_types.h"
#include "file_format_common.h"

namespace Comfy
{
	constexpr u32 InvalidIDValue = 0xFFFFFFFF;
	enum class TexID : u32 { Invalid = InvalidIDValue };
	enum class TexSetID : u32 { Invalid = InvalidIDValue };
	enum class SprID : u32 { Invalid = InvalidIDValue };
	enum class SprSetID : u32 { Invalid = InvalidIDValue };
	enum class AetSceneID : u32 { Invalid = InvalidIDValue };
	enum class AetSetID : u32 { Invalid = InvalidIDValue };

	struct AetSceneEntry
	{
		AetSceneID ID;
		std::string Name;
		i16 Index;
	};

	struct AetSetEntry
	{
		AetSetID ID;
		std::string Name;
		SprSetID SprSetID;
		std::vector<AetSceneEntry> SceneEntries;
		std::string FileName;

		inline AetSceneEntry* FindSceneEntry(std::string_view name) { return FindIfOrNull(SceneEntries, [&](auto& e) { return e.Name == name; }); }
	};

	struct AetDB final : IStreamReadable, IStreamWritable
	{
		std::vector<AetSetEntry> Entries;

		inline AetSetEntry* FindAetSetEntry(std::string_view name) { return FindIfOrNull(Entries, [&](auto& e) { return e.Name == name; }); }
		StreamResult Read(StreamReader& reader) override;
		StreamResult Write(StreamWriter& writer) override;
	};

	struct SprEntry
	{
		SprID ID;
		std::string Name;
		i16 Index;
	};

	struct SprSetEntry
	{
		SprSetID ID;
		std::string Name;
		std::string FileName;
		std::vector<SprEntry> SprEntries;
		std::vector<SprEntry> SprTexEntries;

		inline SprEntry* FindSprEntry(SprID id) { return FindIfOrNull(SprEntries, [&](auto& e) { return (e.ID == id); }); }
		inline SprEntry* FindSprEntry(std::string_view name) { return FindIfOrNull(SprEntries, [&](auto& e) { return (e.Name == name); }); }
		inline SprEntry* FindSprTexEntry(std::string_view name) { return FindIfOrNull(SprTexEntries, [&](auto& e) { return (e.Name == name); }); }
	};

	struct SprDB final : IStreamReadable, IStreamWritable
	{
		std::vector<SprSetEntry> Entries;

		inline SprSetEntry* FindSprSetEntry(std::string_view name) { return FindIfOrNull(Entries, [&](auto& e) { return (e.Name == name); }); }
		inline u32 GetSprSetEntryCount() const { return static_cast<u32>(Entries.size()); }
		inline u32 GetSprEntryCount() const { u32 c = 0; for (auto& e : Entries) c += static_cast<u32>(e.SprEntries.size() + e.SprTexEntries.size()); return c; }
		StreamResult Read(StreamReader& reader) override;
		StreamResult Write(StreamWriter& writer) override;
	};

	constexpr u32 MurmurHashU32(std::string_view stringToHash)
	{
		constexpr u32 hashSeed = 0xDEADBEEF, hashM = 0x7FD652AD, hashR = 0x10;
		size_t inSize = stringToHash.size(); const char* in = stringToHash.data();

		u32 out = hashSeed;
		while (inSize >= sizeof(u32))
		{
			out += static_cast<u32>((static_cast<u8>(in[0]) << 0) | (static_cast<u8>(in[1]) << 8) | (static_cast<u8>(in[2]) << 16) | (static_cast<u8>(in[3]) << 24));
			out *= hashM; out ^= out >> hashR;
			in += sizeof(u32); inSize -= sizeof(u32);
		}

		switch (inSize)
		{
		case 3: out += static_cast<u32>(in[2] << 16);
		case 2: out += static_cast<u32>(in[1] << 8);
		case 1: out += static_cast<u32>(in[0] << 0); out *= hashM; out ^= out >> hashR;
			break;
		}

		out *= hashM; out ^= out >> 10;
		out *= hashM; out ^= out >> 17;
		return out;
	}

	template <typename IDType>
	constexpr IDType MurmurHashID(std::string_view stringToHash)
	{
		static_assert(sizeof(IDType) == sizeof(u32) && std::is_enum_v<IDType>);
		return static_cast<IDType>(MurmurHashU32(stringToHash));
	}
}
