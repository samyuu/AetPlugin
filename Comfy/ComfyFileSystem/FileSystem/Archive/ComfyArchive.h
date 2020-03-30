#pragma once
#include "Types.h"
#include "CoreTypes.h"
#include "FileSystem/Stream/Stream.h"

namespace Comfy::FileSystem
{
	enum class EntryType : uint32_t
	{
		File = 'elif',
		Directory = 'rid',
		Root = 'toor',
		None = 'llun',
	};

	struct ComfyEntryFlags
	{
		// NOTE: Created by the original tool chain
		uint32_t Verified : 1;
		// TODO: To be determined
		uint32_t IsEncrypted : 1;
		uint32_t IsCompressed : 1;
		uint32_t Reserved : 1;
	};

	struct ComfyEntry
	{
		EntryType Type;
		ComfyEntryFlags Flags;

		const char* Name;
		size_t Size;
		uint64_t Offset;
	};

	struct ComfyDirectory
	{
		EntryType Type;
		ComfyEntryFlags Flags;

		const char* Name;
		size_t EntryCount;
		ComfyEntry* Entries;

		size_t SubDirectoryCount;
		ComfyDirectory* SubDirectories;
	};

	struct ComfyVersion
	{
		uint8_t Major;
		uint8_t Minor;
		uint16_t Reserved;
	};

	struct ComfyArchiveFlags
	{
		// NOTE: 64-bit pointers and sizes
		uint64_t WideAddresses : 1;
		// NOTE: Encrypted file and directory names
		uint64_t EncryptedStrings : 1;
		// NOTE: Created by the original tool chain
		uint64_t Verified : 1;
	};

	struct ComfyArchiveHeader
	{
		std::array<uint8_t, 4> Magic;

		ComfyVersion Version;

		std::array<uint8_t, 4> CreatorID;
		std::array<uint8_t, 4> ReservedID;

		__time64_t CreationDate;
		ComfyArchiveFlags Flags;

		std::array<uint8_t, 16> IV;

		size_t DataSize;
		uint64_t DataOffset;
	};

	class ComfyArchive : NonCopyable
	{
	public:
		static constexpr std::array<uint8_t, 4> Magic = { 0xCF, 0x5C, 0xAC, 0x90 };
		static constexpr ComfyVersion Version = { 0x01, 0x00 };

		static constexpr char DirectorySeparator = '/';

	public:
		ComfyArchive();
		~ComfyArchive();

	public:
		void Mount(const std::string_view filePath);
		void UnMount();

		const ComfyArchiveHeader& GetHeader() const;
		const ComfyDirectory& GetRootDirectory() const;

		const ComfyEntry* FindFile(std::string_view filePath) const;
		const ComfyEntry* FindFileInDirectory(const ComfyDirectory* directory, std::string_view fileName) const;

		const ComfyDirectory* FindDirectory(std::string_view directoryPath) const;
		
		bool ReadFileIntoBuffer(std::string_view filePath, std::vector<uint8_t> &buffer);

		bool ReadEntryIntoBuffer(const ComfyEntry* entry, void* outputBuffer);

	private:
		const ComfyDirectory* FindNestedDirectory(const ComfyDirectory* parent, std::string_view directory) const;
		const ComfyDirectory* FindDirectory(const ComfyDirectory* parent, std::string_view directoryName) const;

	private:
		void ParseEntries();
		void LinkRemapPointers();
		void DecryptStrings();

	private:
		bool isMounted = false;

		ComfyArchiveHeader header = {};
		ComfyDirectory* rootDirectory = nullptr;
		
		UniquePtr<uint8_t[]> dataBuffer = nullptr;
		UniquePtr<IStream> dataStream = nullptr;
	};
}
