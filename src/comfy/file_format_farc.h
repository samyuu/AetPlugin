#pragma once
#include "core_types.h"
#include "file_format_common.h"
#include <array>

namespace Comfy
{
	namespace FArcEncryption
	{
		constexpr size_t IVSize = 16, KeySize = 16;

		// NOTE: project_diva.bin
		constexpr std::array<u8, KeySize> ClassicKey = { 'p', 'r', 'o', 'j', 'e', 'c', 't', '_', 'd', 'i', 'v', 'a', '.', 'b', 'i', 'n' };

		// NOTE: 1372D57B6E9E31EBA239B83C1557C6BB
		constexpr std::array<u8, KeySize> ModernKey = { 0x13, 0x72, 0xD5, 0x7B, 0x6E, 0x9E, 0x31, 0xEB, 0xA2, 0x39, 0xB8, 0x3C, 0x15, 0x57, 0xC6, 0xBB };

		// NOTE: CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC
		constexpr std::array<u8, IVSize> DummyIV = { 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC };

		constexpr size_t GetPaddedSize(size_t dataSize, size_t alignment = 16) { return (dataSize + (alignment - 1)) & ~(alignment - 1); }
	}

	enum class FArcSignature : u32
	{
		UnCompressed = 'FArc',
		Compressed = 'FArC',
		Extended = 'FARC',
		Reserved = 'FARc',
	};

	enum FArcFlags : u32
	{
		FArcFlags_None = 0,
		FArcFlags_Reserved = 1 << 0,
		FArcFlags_Compressed = 1 << 1,
		FArcFlags_Encrypted = 1 << 2,
	};

	enum class FArcEncryptionFormat : u8 { None, Classic, Modern };

	struct FArc;
	struct FArcEntry
	{
		FArc& InternalParentFArc;
		std::string Name;
		FileAddr Offset;
		size_t CompressedSize;
		size_t OriginalSize;

		// NOTE: Output buffer has to be large enough to store all of OriginalSize
		void ReadIntoBuffer(void* outFileContent) const;
	};

	struct FArc
	{
		std::vector<FArcEntry> Entries;
		FileStream Stream = {};
		FArcSignature Signature = FArcSignature::UnCompressed;
		FArcFlags Flags = FArcFlags_None;
		u32 Alignment = 0;
		b8 IsModern = false;
		FArcEncryptionFormat EncryptionFormat = FArcEncryptionFormat::None;
		std::array<u8, FArcEncryption::IVSize> AesIV = FArcEncryption::DummyIV;

		FArc() = default;
		~FArc() { Stream.Close(); }

		static std::unique_ptr<FArc> Open(std::string_view filePath);
		const FArcEntry* FindFile(std::string_view name, b8 caseSensitive = false);

		b8 InternalOpenStream(std::string_view filePath);
		void InternalReadEntryIntoBuffer(const FArcEntry& entry, void* outFileContent);
		b8 InternalParseHeaderAndEntries();
		b8 InternalParseAdvanceSingleEntry(const u8*& headerDataPointer, const u8* const headerEnd);
		b8 InternalParseAllEntriesByRange(const u8* headerData, const u8* headerEnd);
		b8 InternalParseAllEntriesByCount(const u8* headerData, size_t entryCount, const u8* const headerEnd);
		b8 InternalDecryptFileContent(const u8* encryptedData, u8* decryptedData, size_t dataSize);
	};

	struct FArcPacker
	{
		// NOTE: All input file references are expected to stay valid at least until CreateFlushFArc() has been called
		void AddFile(std::string fileName, IStreamWritable& writable);
		void AddFile(std::string fileName, const void* fileContent, size_t fileSize);
		b8 CreateFlushFArc(std::string_view filePath, b8 compressed, u32 alignment = 16);

		struct StreamWritableEntry { std::string FileName; IStreamWritable& Writable; size_t FileSizeOnceWritten, CompressedFileSizeOnceWritten; };
		struct DataPointerEntry { std::string FileName; const void* Data; size_t DataSize, CompressedFileSizeOnceWritten; };
		std::vector<StreamWritableEntry> WritableEntries;
		std::vector<DataPointerEntry> DataPointerEntries;
	};
}
