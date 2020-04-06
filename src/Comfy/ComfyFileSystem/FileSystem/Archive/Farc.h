#pragma once
#include "FileArchive.h"
#include "FileSystem/Stream/FileStream.h"
#include "FileSystem/BinaryReader.h"

namespace Comfy::FileSystem
{
	enum FarcSignature : uint32_t
	{
		FarcSignature_Normal = 'FArc',
		FarcSignature_Compressed = 'FArC',
		FarcSignature_Encrypted = 'FARC',
	};

	enum FarcFlags : uint32_t
	{
		FarcFlags_None = 0,
		FarcFlags_Compressed = 1 << 1,
		FarcFlags_Encrypted = 1 << 2,
	};

	enum class FarcEncryptionFormat
	{
		None, 
		ProjectDivaBin, 
		OrbisFutureTone
	};

	class Farc final : public FileArchive, NonCopyable
	{
	public:
		static constexpr size_t IVSize = 16;
		static constexpr size_t KeySize = 16;

		// NOTE: project_diva.bin
		static constexpr std::array<uint8_t, KeySize>  ProjectDivaBinKey = { 'p', 'r', 'o', 'j', 'e', 'c', 't', '_', 'd', 'i', 'v', 'a', '.', 'b', 'i', 'n' };

		// NOTE: 1372D57B6E9E31EBA239B83C1557C6BB
		static constexpr std::array<uint8_t, KeySize> OrbisFutureToneKey = { 0x13, 0x72, 0xD5, 0x7B, 0x6E, 0x9E, 0x31, 0xEB, 0xA2, 0x39, 0xB8, 0x3C, 0x15, 0x57, 0xC6, 0xBB };

	public:
		Farc();
		~Farc();

		static RefPtr<Farc> Open(std::string_view filePath);

	protected:
		FileStream stream;
		BinaryReader reader;
		uint32_t alignment = {};
		FarcFlags flags = {};
		FarcEncryptionFormat encryptionFormat = {};
		std::array<uint8_t, IVSize> aesIV = {};

	protected:
		bool OpenStream(std::wstring_view filePath);
		bool ParseEntries();

		void ReadArchiveEntry(const ArchiveEntry& entry, void* fileContentOut) override;

	private:
		bool ParseEntryInternal(const uint8_t*& headerDataPointer);
		bool ParseEntriesInternal(const uint8_t* headerData, const uint8_t* headerEnd);
		bool ParseEntriesInternal(const uint8_t* headerData, uint32_t entryCount);
		bool DecryptFileInternal(const uint8_t* encryptedData, uint8_t* decryptedData, uint32_t dataSize);
	};
}
