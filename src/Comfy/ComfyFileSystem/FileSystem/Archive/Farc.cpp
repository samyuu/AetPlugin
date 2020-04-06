#include "Farc.h"
#include "Misc/StringHelper.h"
#include "Core/Logger.h"
#include "Win32Crypto.h"
#include <zlib.h>
#include <assert.h>

namespace Comfy::FileSystem
{
	namespace
	{
		inline uint32_t GetAesDataAlignmentSize(const uint64_t dataSize)
		{
			constexpr uint32_t aesAlignmentSize = 16;
			return (static_cast<uint32_t>(dataSize) + (aesAlignmentSize - 1)) & ~(aesAlignmentSize - 1);
		}
	}

	Farc::Farc()
	{
	}

	Farc::~Farc()
	{
		stream.Close();
	}

	RefPtr<Farc> Farc::Open(std::string_view filePath)
	{
		const std::wstring widePath = Utf8ToUtf16(filePath);

		RefPtr<Farc> farc = MakeRef<Farc>();
		if (!farc->OpenStream(widePath))
		{
			Logger::LogErrorLine(__FUNCTION__"(): Unable to open '%s'", filePath.data());
			return nullptr;
		}

		if (!farc->ParseEntries())
		{
			Logger::LogErrorLine(__FUNCTION__"(): Unable to parse '%s'", filePath.data());
			return nullptr;
		}

		return farc;
	}

	bool Farc::OpenStream(std::wstring_view filePath)
	{
		stream.OpenRead(filePath);

		if (stream.IsOpen())
		{
			reader.OpenStream(stream);
			reader.SetEndianness(Endianness::Big);
			return true;
		}

		return false;
	}

	bool Farc::ParseEntries()
	{
		if (reader.GetLength() <= FileAddr(sizeof(uint32_t[2])))
			return false;

		std::array<uint32_t, 2> farcInfo;
		reader.ReadBuffer(farcInfo.data(), sizeof(farcInfo));

		uint32_t signature = ByteSwapU32(farcInfo[0]);
		uint32_t headerSize = ByteSwapU32(farcInfo[1]);

		if (reader.GetLength() <= (reader.GetPosition() + static_cast<FileAddr>(headerSize)))
			return false;

		if (signature == FarcSignature_Normal || signature == FarcSignature_Compressed)
		{
			encryptionFormat = FarcEncryptionFormat::None;

			alignment = reader.ReadU32();
			flags = (signature == FarcSignature_Compressed) ? FarcFlags_Compressed : FarcFlags_None;

			headerSize -= sizeof(alignment);

			auto headerData = MakeUnique<uint8_t[]>(headerSize);
			reader.ReadBuffer(headerData.get(), headerSize);

			uint8_t* currentHeaderPosition = headerData.get();
			uint8_t* headerEnd = headerData.get() + headerSize;

			ParseEntriesInternal(currentHeaderPosition, headerEnd);
		}
		else if (signature == FarcSignature_Encrypted)
		{
			std::array<uint32_t, 2> formatData;
			reader.ReadBuffer(formatData.data(), sizeof(formatData));
			flags = static_cast<FarcFlags>(ByteSwapU32(formatData[0]));
			alignment = ByteSwapU32(formatData[1]);

			// NOTE: Peek at the next 8 bytes which are either the alignment value followed by padding or the start of the AES IV
			std::array<uint32_t, 2> nextData;
			reader.ReadBuffer(nextData.data(), sizeof(nextData));
			reader.SetPosition(reader.GetPosition() - FileAddr(sizeof(nextData)));

			// NOTE: If the padding is not zero and the potential alignment value is unreasonably high we treat it as an encrypted entry table
			constexpr uint32_t reasonableAlignmentThreshold = 0x1000;
			bool encryptedEntries = (flags & FarcFlags_Encrypted) && (nextData[1] != 0) && (ByteSwapU32(nextData[0]) >= reasonableAlignmentThreshold);

			if (encryptedEntries)
			{
				encryptionFormat = FarcEncryptionFormat::OrbisFutureTone;
				reader.ReadBuffer(aesIV.data(), aesIV.size());

				uint32_t paddedHeaderSize = GetAesDataAlignmentSize(headerSize);

				// NOTE: Allocate encrypted and decrypted data as a continuous block
				auto headerData = MakeUnique<uint8_t[]>(paddedHeaderSize * 2);

				uint8_t* encryptedHeaderData = headerData.get();
				uint8_t* decryptedHeaderData = headerData.get() + paddedHeaderSize;

				reader.ReadBuffer(encryptedHeaderData, paddedHeaderSize);

				DecryptFileInternal(encryptedHeaderData, decryptedHeaderData, paddedHeaderSize);

				uint8_t* currentHeaderPosition = decryptedHeaderData;
				uint8_t* headerEnd = decryptedHeaderData + headerSize;

				alignment = ByteSwapU32(*reinterpret_cast<uint32_t*>(currentHeaderPosition));
				currentHeaderPosition += sizeof(uint32_t);
				currentHeaderPosition += sizeof(uint32_t);

				uint32_t entryCount = ByteSwapU32(*reinterpret_cast<uint32_t*>(currentHeaderPosition));
				currentHeaderPosition += sizeof(uint32_t);
				currentHeaderPosition += sizeof(uint32_t);

				ParseEntriesInternal(currentHeaderPosition, entryCount);
			}
			else
			{
				encryptionFormat = FarcEncryptionFormat::ProjectDivaBin;

				headerSize -= sizeof(formatData);

				auto headerData = MakeUnique<uint8_t[]>(headerSize);
				reader.ReadBuffer(headerData.get(), headerSize);

				uint8_t* currentHeaderPosition = headerData.get();
				uint8_t* headerEnd = headerData.get() + headerSize;

				alignment = ByteSwapU32(*reinterpret_cast<uint32_t*>(currentHeaderPosition));
				currentHeaderPosition += sizeof(uint32_t);

				// NOTE: Padding (?)
				currentHeaderPosition += sizeof(uint32_t);
				currentHeaderPosition += sizeof(uint32_t);

				ParseEntriesInternal(currentHeaderPosition, headerEnd);
			}
		}
		else
		{
			// NOTE: Not a farc file or invalid format, might wanna add some error logging
			return false;
		}

		return true;
	}

	void Farc::ReadArchiveEntry(const ArchiveEntry& entry, void* fileContentOut)
	{
		stream.Seek(static_cast<FileAddr>(entry.FileOffset));

		if (flags == FarcFlags_None)
		{
			reader.ReadBuffer(fileContentOut, entry.FileSize);
		}
		else if (flags & FarcFlags_Compressed)
		{
			// NOTE: Since the farc file size is only stored in a 32bit integer, decompressing it as a single block should be safe enough (?)
			const uint32_t paddedSize = GetAesDataAlignmentSize(entry.CompressedSize) + 16;

			UniquePtr<uint8_t[]> encryptedData = nullptr;
			UniquePtr<uint8_t[]> compressedData = MakeUnique<uint8_t[]>(paddedSize);

			if (flags & FarcFlags_Encrypted)
			{
				encryptedData = MakeUnique<uint8_t[]>(paddedSize);
				reader.ReadBuffer(encryptedData.get(), entry.CompressedSize);

				DecryptFileInternal(encryptedData.get(), compressedData.get(), paddedSize);
			}
			else
			{
				reader.ReadBuffer(compressedData.get(), entry.CompressedSize);
			}

			// NOTE: Could this be related to the IV size?
			const uint32_t dataOffset = (encryptionFormat == FarcEncryptionFormat::OrbisFutureTone) ? 16 : 0;

			z_stream zStream;
			zStream.zalloc = Z_NULL;
			zStream.zfree = Z_NULL;
			zStream.opaque = Z_NULL;
			zStream.avail_in = static_cast<uInt>(entry.CompressedSize);
			zStream.next_in = reinterpret_cast<Bytef*>(compressedData.get() + dataOffset);
			zStream.avail_out = static_cast<uInt>(entry.FileSize);
			zStream.next_out = reinterpret_cast<Bytef*>(fileContentOut);

			const int initResult = inflateInit2(&zStream, 31);
			assert(initResult == Z_OK);

			// NOTE: This will sometimes fail with Z_DATA_ERROR "incorrect data check" which I believe to be caused by alignment issues with the very last data block of the file
			// NOTE: The file content should however still have been inflated correctly
			int inflateResult = inflate(&zStream, Z_FINISH);
			// assert(inflateResult == Z_STREAM_END && zStream.msg == nullptr);

			const int endResult = inflateEnd(&zStream);
			assert(endResult == Z_OK);
		}
		else if (flags & FarcFlags_Encrypted)
		{
			const uint32_t paddedSize = GetAesDataAlignmentSize(entry.CompressedSize);
			auto encryptedData = MakeUnique<uint8_t[]>(paddedSize);

			reader.ReadBuffer(encryptedData.get(), entry.FileSize);
			uint8_t* fileOutput = reinterpret_cast<uint8_t*>(fileContentOut);

			if (paddedSize == entry.FileSize)
			{
				DecryptFileInternal(encryptedData.get(), fileOutput, paddedSize);
			}
			else
			{
				// NOTE: Suboptimal temporary file copy to avoid AES padding issues. All encrypted farcs should however always be either compressed or have correct alignment
				auto decryptedData = MakeUnique<uint8_t[]>(paddedSize);
				
				DecryptFileInternal(encryptedData.get(), decryptedData.get(), paddedSize);
				std::copy(decryptedData.get(), decryptedData.get() + entry.FileSize, fileOutput);
			}
		}
		else
		{
			// NOTE: Invalid FarcFlags
			assert(false);
		}
	}

	bool Farc::ParseEntryInternal(const uint8_t*& headerDataPointer)
	{
		ArchiveEntry entry { this };

		entry.Name = std::string(reinterpret_cast<const char*>(headerDataPointer));
		headerDataPointer += entry.Name.size() + sizeof(char);

		entry.FileOffset = ByteSwapU32(*reinterpret_cast<const uint32_t*>(headerDataPointer));
		headerDataPointer += sizeof(uint32_t);

		const uint32_t fileSize = ByteSwapU32(*reinterpret_cast<const uint32_t*>(headerDataPointer));
		headerDataPointer += sizeof(uint32_t);

		if (flags & FarcFlags_Compressed)
		{
			entry.CompressedSize = fileSize;
			entry.FileSize = ByteSwapU32(*reinterpret_cast<const uint32_t*>(headerDataPointer));

			headerDataPointer += sizeof(uint32_t);
		}
		else
		{
			entry.CompressedSize = fileSize;
			entry.FileSize = fileSize;
		}

		if (entry.FileOffset + entry.CompressedSize > static_cast<uint64_t>(reader.GetLength()))
		{
			assert(false);
			false;
		}

		if (encryptionFormat == FarcEncryptionFormat::OrbisFutureTone)
		{
			const uint32_t unknown = ByteSwapU32(*reinterpret_cast<const uint32_t*>(headerDataPointer));
			headerDataPointer += sizeof(uint32_t);
		}

		archiveEntries.push_back(entry);

		return true;
	}

	bool Farc::ParseEntriesInternal(const uint8_t* headerStart, const uint8_t* headerEnd)
	{
		while (headerStart < headerEnd)
		{
			if (!ParseEntryInternal(headerStart))
				break;
		}

		return true;
	}

	bool Farc::ParseEntriesInternal(const uint8_t* headerData, uint32_t entryCount)
	{
		for (uint32_t i = 0; i < entryCount; i++)
		{
			if (!ParseEntryInternal(headerData))
				break;
		}

		return true;
	}

	bool Farc::DecryptFileInternal(const uint8_t* encryptedData, uint8_t* decryptedData, uint32_t dataSize)
	{
		if (encryptionFormat == FarcEncryptionFormat::ProjectDivaBin)
		{
			return Crypto::Win32DecryptAesEcb(encryptedData, decryptedData, dataSize, ProjectDivaBinKey);
		}
		else if (encryptionFormat == FarcEncryptionFormat::OrbisFutureTone)
		{
			return Crypto::Win32DecryptAesCbc(encryptedData, decryptedData, dataSize, OrbisFutureToneKey, aesIV);
		}
		else
		{
			assert(false);
		}

		return false;
	}
}
