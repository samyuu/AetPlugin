#include "file_format_farc.h"
#include <zlib.h>

#include <Windows.h>
#include <bcrypt.h>
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

namespace Comfy::Crypto
{
	enum class Win32BlockCipherMode : u32 { CBC, ECB, };

	struct Win32AlgorithmProvider
	{
		BCRYPT_ALG_HANDLE Handle = NULL;

		Win32AlgorithmProvider(Win32BlockCipherMode cipherMode)
		{
			NTSTATUS status = ::BCryptOpenAlgorithmProvider(&Handle, BCRYPT_AES_ALGORITHM, NULL, 0);
			if (NT_SUCCESS(status))
			{
				switch (cipherMode)
				{
				case Win32BlockCipherMode::CBC: status = ::BCryptSetProperty(Handle, BCRYPT_CHAINING_MODE, (PBYTE)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0); break;
				case Win32BlockCipherMode::ECB: status = ::BCryptSetProperty(Handle, BCRYPT_CHAINING_MODE, (PBYTE)BCRYPT_CHAIN_MODE_ECB, sizeof(BCRYPT_CHAIN_MODE_ECB), 0); break;
				default: assert(false); return;
				}
				if (!NT_SUCCESS(status))
					printf(__FUNCTION__"(): BCryptSetProperty() failed with 0x%X", status);
			}
			else
			{
				printf(__FUNCTION__"(): BCryptOpenAlgorithmProvider() failed with 0x%X", status);
			}
		}

		~Win32AlgorithmProvider() { if (Handle) ::BCryptCloseAlgorithmProvider(Handle, 0); }
	};

	struct Win32SymmetricKey
	{
		BCRYPT_KEY_HANDLE Handle = NULL;
		std::vector<u8> KeyObject;

		Win32SymmetricKey(Win32AlgorithmProvider& algorithm, std::array<u8, 16>& key)
		{
			if (!algorithm.Handle) { printf(__FUNCTION__"(): Invalid AlgorithmProvider::Handle"); return; }

			NTSTATUS status;
			ULONG keyObjectSize;
			DWORD copiedDataSize;
			status = ::BCryptGetProperty(algorithm.Handle, BCRYPT_OBJECT_LENGTH, (PBYTE)&keyObjectSize, sizeof(DWORD), &copiedDataSize, 0);
			if (NT_SUCCESS(status))
			{
				KeyObject.resize(keyObjectSize);
				status = ::BCryptGenerateSymmetricKey(algorithm.Handle, &Handle, KeyObject.data(), keyObjectSize, key.data(), static_cast<ULONG>(key.size()), 0);

				if (!NT_SUCCESS(status))
					printf(__FUNCTION__"(): BCryptGenerateSymmetricKey() failed with 0x%X", status);
			}
			else
			{
				printf(__FUNCTION__"(): BCryptGetProperty() failed with 0x%X", status);
			}
		}

		~Win32SymmetricKey() { if (Handle) ::BCryptDestroyKey(Handle); }
	};

	static b8 Win32DecryptInternal(Win32SymmetricKey& key, u8* encryptedData, u8* decryptedData, size_t dataSize, std::array<u8, 16>* iv)
	{
		if (!key.Handle) { printf(__FUNCTION__"(): Invalid SymmetricKey::Handle"); return false; }

		ULONG copiedDataSize;
		NTSTATUS status = (iv == nullptr) ?
			::BCryptDecrypt(key.Handle, encryptedData, static_cast<ULONG>(dataSize), NULL, nullptr, 0, decryptedData, static_cast<ULONG>(dataSize), &copiedDataSize, 0) :
			::BCryptDecrypt(key.Handle, encryptedData, static_cast<ULONG>(dataSize), NULL, iv->data(), static_cast<ULONG>(iv->size()), decryptedData, static_cast<ULONG>(dataSize), &copiedDataSize, 0);

		if (!NT_SUCCESS(status)) { printf(__FUNCTION__"(): BCryptDecrypt() failed with 0x%X", status); return false; }
		return true;
	}

	static b8 DecryptAesEcb(const u8* encryptedData, u8* decryptedData, size_t dataSize, std::array<u8, FArcEncryption::KeySize> key)
	{
		Win32AlgorithmProvider algorithmProvider = { Win32BlockCipherMode::ECB };
		Win32SymmetricKey symmetricKey = { algorithmProvider, key };
		return Win32DecryptInternal(symmetricKey, const_cast<u8*>(encryptedData), decryptedData, dataSize, nullptr);
	}

	static b8 DecryptAesCbc(const u8* encryptedData, u8* decryptedData, size_t dataSize, std::array<u8, FArcEncryption::KeySize> key, std::array<u8, FArcEncryption::IVSize> iv)
	{
		Win32AlgorithmProvider algorithmProvider = { Win32BlockCipherMode::CBC };
		Win32SymmetricKey symmetricKey = { algorithmProvider, key };
		return Win32DecryptInternal(symmetricKey, const_cast<u8*>(encryptedData), decryptedData, dataSize, &iv);
	}
}

namespace Comfy
{
	void FArcEntry::ReadIntoBuffer(void* outFileContent) const
	{
		InternalParentFArc.InternalReadEntryIntoBuffer(*this, outFileContent);
	}

	std::unique_ptr<FArc> FArc::Open(std::string_view filePath)
	{
		auto farc = std::make_unique<FArc>();
		if (!farc->InternalOpenStream(filePath)) { printf(__FUNCTION__"(): Unable to open '%.*s", FmtStrViewArgs(filePath)); return nullptr; }
		if (!farc->InternalParseHeaderAndEntries()) { printf(__FUNCTION__"(): Unable to parse '%.*s", FmtStrViewArgs(filePath)); return nullptr; }
		return farc;
	}

	const FArcEntry* FArc::FindFile(std::string_view name, b8 caseSensitive)
	{
		return (caseSensitive) ?
			FindIfOrNull(Entries, [&](auto& e) { return (e.Name == name); }) :
			FindIfOrNull(Entries, [&](auto& e) { return ASCII::MatchesInsensitive(e.Name, name); });
	}

	b8 FArc::InternalOpenStream(std::string_view filePath)
	{
		Stream.OpenRead(filePath);
		return Stream.IsOpen();
	}

	void FArc::InternalReadEntryIntoBuffer(const FArcEntry& entry, void* outFileContent)
	{
		if (outFileContent == nullptr || !Stream.IsOpen())
			return;

		// NOTE: Could this be related to the IV size?
		const size_t dataOffset = (EncryptionFormat == FArcEncryptionFormat::Modern) ? 16 : 0;

		if (Flags & FArcFlags_Compressed)
		{
			Stream.Seek(entry.Offset);

			// NOTE: Since the farc file size is only stored in a 32bit integer, decompressing it as a single block should be safe enough (?)
			const auto paddedSize = Min(FArcEncryption::GetPaddedSize(entry.CompressedSize, Alignment) + 16, static_cast<size_t>(Stream.GetLength()));

			std::unique_ptr<u8[]> encryptedData = nullptr;
			std::unique_ptr<u8[]> compressedData = std::make_unique<u8[]>(paddedSize);

			if (Flags & FArcFlags_Encrypted)
			{
				encryptedData = std::make_unique<u8[]>(paddedSize);
				Stream.ReadBuffer(encryptedData.get(), paddedSize);

				InternalDecryptFileContent(encryptedData.get(), compressedData.get(), paddedSize);
			}
			else
			{
				Stream.ReadBuffer(compressedData.get(), paddedSize);
			}

			z_stream zStream;
			zStream.zalloc = Z_NULL;
			zStream.zfree = Z_NULL;
			zStream.opaque = Z_NULL;
			zStream.avail_in = static_cast<uInt>(paddedSize);
			zStream.next_in = reinterpret_cast<Bytef*>(compressedData.get() + dataOffset);
			zStream.avail_out = static_cast<uInt>(entry.OriginalSize);
			zStream.next_out = reinterpret_cast<Bytef*>(outFileContent);

			const int initResult = inflateInit2(&zStream, 31);
			assert(initResult == Z_OK);

			// NOTE: This will sometimes fail with Z_DATA_ERROR "incorrect data check" which I believe to be caused by alignment issues with the very last data block of the file
			//		 The file content should however still have been inflated correctly
			const int inflateResult = inflate(&zStream, Z_FINISH);
			// assert(inflateResult == Z_STREAM_END && zStream.msg == nullptr);

			const int endResult = inflateEnd(&zStream);
			assert(endResult == Z_OK);
		}
		else if (Flags & FArcFlags_Encrypted)
		{
			Stream.Seek(entry.Offset);

			const auto paddedSize = FArcEncryption::GetPaddedSize(entry.OriginalSize) + dataOffset;
			auto encryptedData = std::make_unique<u8[]>(paddedSize);

			Stream.ReadBuffer(encryptedData.get(), paddedSize);
			u8* fileOutput = reinterpret_cast<u8*>(outFileContent);

			if (paddedSize == entry.OriginalSize)
			{
				InternalDecryptFileContent(encryptedData.get(), fileOutput, paddedSize);
			}
			else
			{
				// NOTE: Suboptimal temporary file copy to avoid AES padding issues. All encrypted farcs should however always be either compressed or have correct alignment
				auto decryptedData = std::make_unique<u8[]>(paddedSize);

				InternalDecryptFileContent(encryptedData.get(), decryptedData.get(), paddedSize);

				const u8* decryptedOffsetData = decryptedData.get() + dataOffset;
				std::copy(decryptedOffsetData, decryptedOffsetData + entry.OriginalSize, fileOutput);
			}
		}
		else
		{
			Stream.Seek(entry.Offset);
			Stream.ReadBuffer(outFileContent, entry.OriginalSize);
		}
	}

	b8 FArc::InternalParseHeaderAndEntries()
	{
		if (Stream.GetLength() <= FileAddr(sizeof(u32[2])))
			return false;

		std::array<u32, 2> parsedSignatureData;
		Stream.ReadBuffer(parsedSignatureData.data(), sizeof(parsedSignatureData));

		Signature = static_cast<FArcSignature>(ByteSwapU32(parsedSignatureData[0]));
		const auto parsedHeaderSize = ByteSwapU32(parsedSignatureData[1]);

		// NOTE: Empty but valid FArc
		if (Signature == FArcSignature::UnCompressed && parsedHeaderSize <= sizeof(u32))
			return true;

		if (Stream.GetLength() <= (Stream.GetPosition() + static_cast<FileAddr>(parsedHeaderSize)))
			return false;

		if (Signature == FArcSignature::UnCompressed || Signature == FArcSignature::Compressed)
		{
			EncryptionFormat = FArcEncryptionFormat::None;

			u32 parsedAlignment;
			Stream.ReadBuffer(&parsedAlignment, sizeof(parsedAlignment));

			Alignment = ByteSwapU32(parsedAlignment);
			Flags = (Signature == FArcSignature::Compressed) ? FArcFlags_Compressed : FArcFlags_None;

			const auto headerSize = (parsedHeaderSize - sizeof(Alignment));

			auto headerData = std::make_unique<u8[]>(headerSize);
			Stream.ReadBuffer(headerData.get(), headerSize);

			u8* currentHeaderPosition = headerData.get();
			const u8* headerEnd = headerData.get() + headerSize;

			InternalParseAllEntriesByRange(currentHeaderPosition, headerEnd);
		}
		else if (Signature == FArcSignature::Extended)
		{
			std::array<u32, 2> parsedFormatData;
			Stream.ReadBuffer(parsedFormatData.data(), sizeof(parsedFormatData));

			Flags = static_cast<FArcFlags>(ByteSwapU32(parsedFormatData[0]));

			// NOTE: Peek at the next 8 bytes which are either the alignment value followed by padding or the start of the AES IV
			std::array<u32, 2> parsedNextData;
			Stream.ReadBuffer(parsedNextData.data(), sizeof(parsedNextData));

			Alignment = ByteSwapU32(parsedNextData[0]);
			IsModern = (parsedNextData[1] != 0);

			// NOTE: If the padding is not zero and the potential alignment value is unreasonably high we treat it as an encrypted entry table
			constexpr u32 reasonableAlignmentThreshold = 0x1000;
			const b8 encryptedEntries = (Flags & FArcFlags_Encrypted) && IsModern && (Alignment >= reasonableAlignmentThreshold);

			EncryptionFormat = (Flags & FArcFlags_Encrypted) ? (encryptedEntries ? FArcEncryptionFormat::Modern : FArcEncryptionFormat::Classic) : FArcEncryptionFormat::None;

			if (encryptedEntries)
			{
				Stream.Seek(Stream.GetPosition() - FileAddr(sizeof(parsedNextData)));
				Stream.ReadBuffer(AesIV.data(), AesIV.size());

				const auto paddedHeaderSize = FArcEncryption::GetPaddedSize(parsedHeaderSize);

				// NOTE: Allocate encrypted and decrypted data as a continuous block
				auto headerData = std::make_unique<u8[]>(paddedHeaderSize * 2);

				u8* encryptedHeaderData = headerData.get();
				u8* decryptedHeaderData = headerData.get() + paddedHeaderSize;

				Stream.ReadBuffer(encryptedHeaderData, paddedHeaderSize);
				InternalDecryptFileContent(encryptedHeaderData, decryptedHeaderData, paddedHeaderSize);

				u8* currentHeaderPosition = decryptedHeaderData;
				const u8* headerEnd = decryptedHeaderData + parsedHeaderSize;

				// NOTE: Example data: 00 00 00 10 | 00 00 00 01 | 00 00 00 01 | 00 00 00 10
				Alignment = ByteSwapU32(*reinterpret_cast<u32*>(currentHeaderPosition));
				currentHeaderPosition += sizeof(u32);
				currentHeaderPosition += sizeof(u32);

				const u32 entryCount = ByteSwapU32(*reinterpret_cast<u32*>(currentHeaderPosition));
				currentHeaderPosition += sizeof(u32);
				currentHeaderPosition += sizeof(u32);

				InternalParseAllEntriesByCount(currentHeaderPosition, entryCount, headerEnd);
				assert(Entries.size() == entryCount);
			}
			else
			{
				Stream.Seek(Stream.GetPosition() - FileAddr(sizeof(u32)));

				const auto headerSize = (parsedHeaderSize - 12);

				auto headerData = std::make_unique<u8[]>(headerSize);
				Stream.ReadBuffer(headerData.get(), headerSize);

				u8* currentHeaderPosition = headerData.get();
				const u8* headerEnd = currentHeaderPosition + headerSize;

				if (IsModern)
				{
					// NOTE: Example data: 00 00 00 01 | 00 00 00 01 | 00 00 00 10
					const u32 reserved = ByteSwapU32(*reinterpret_cast<u32*>(currentHeaderPosition));
					currentHeaderPosition += sizeof(u32);

					const u32 entryCount = ByteSwapU32(*reinterpret_cast<u32*>(currentHeaderPosition));
					currentHeaderPosition += sizeof(u32);

					Alignment = ByteSwapU32(*reinterpret_cast<u32*>(currentHeaderPosition));
					currentHeaderPosition += sizeof(u32);

					InternalParseAllEntriesByCount(currentHeaderPosition, entryCount, headerEnd);
					assert(Entries.size() == entryCount);
				}
				else
				{
					const u32 reserved0 = ByteSwapU32(*reinterpret_cast<u32*>(currentHeaderPosition));
					currentHeaderPosition += sizeof(u32);

					const u32 reserved1 = ByteSwapU32(*reinterpret_cast<u32*>(currentHeaderPosition));
					currentHeaderPosition += sizeof(u32);

					InternalParseAllEntriesByRange(currentHeaderPosition, headerEnd);
				}
			}
		}
		else
		{
			// NOTE: Not a FArc file or invalid format, might wanna add some error logging
			return false;
		}

		return true;
	}

	b8 FArc::InternalParseAdvanceSingleEntry(const u8*& headerDataPointer, const u8* const headerEnd)
	{
		FArcEntry newEntry = { *this, std::string(reinterpret_cast<cstr>(headerDataPointer)) };
		assert(!newEntry.Name.empty());

		headerDataPointer += newEntry.Name.size() + sizeof(char);
		assert(headerDataPointer <= headerEnd);

		newEntry.Offset = static_cast<FileAddr>(ByteSwapU32(*reinterpret_cast<const u32*>(headerDataPointer)));

		headerDataPointer += sizeof(u32);
		assert(headerDataPointer <= headerEnd);

		newEntry.CompressedSize = ByteSwapU32(*reinterpret_cast<const u32*>(headerDataPointer));

		headerDataPointer += sizeof(u32);
		assert(headerDataPointer <= headerEnd);

		if (Signature == FArcSignature::UnCompressed)
		{
			newEntry.OriginalSize = newEntry.CompressedSize;
		}
		else
		{
			newEntry.OriginalSize = ByteSwapU32(*reinterpret_cast<const u32*>(headerDataPointer));

			headerDataPointer += sizeof(u32);
			assert(headerDataPointer <= headerEnd);
		}

		if (newEntry.Offset + static_cast<FileAddr>(newEntry.CompressedSize) > Stream.GetLength())
		{
			assert(false);
			false;
		}

		if (IsModern)
		{
			const u32 parsedReserved = ByteSwapU32(*reinterpret_cast<const u32*>(headerDataPointer));
			headerDataPointer += sizeof(u32);
			assert(headerDataPointer <= headerEnd);
		}

		Entries.push_back(std::move(newEntry));
		return true;
	}

	b8 FArc::InternalParseAllEntriesByRange(const u8* headerStart, const u8* headerEnd)
	{
		while (headerStart < headerEnd)
		{
			if (!InternalParseAdvanceSingleEntry(headerStart, headerEnd))
				break;
		}
		return true;
	}

	b8 FArc::InternalParseAllEntriesByCount(const u8* headerData, size_t entryCount, const u8* const headerEnd)
	{
		Entries.reserve(entryCount);
		for (size_t i = 0; i < entryCount; i++)
		{
			if (!InternalParseAdvanceSingleEntry(headerData, headerEnd))
				break;
		}
		return true;
	}

	b8 FArc::InternalDecryptFileContent(const u8* encryptedData, u8* decryptedData, size_t dataSize)
	{
		if (EncryptionFormat == FArcEncryptionFormat::Classic)
			return Crypto::DecryptAesEcb(encryptedData, decryptedData, dataSize, FArcEncryption::ClassicKey);
		else if (EncryptionFormat == FArcEncryptionFormat::Modern)
			return Crypto::DecryptAesCbc(encryptedData, decryptedData, dataSize, FArcEncryption::ModernKey, AesIV);
		else
			assert(false);

		return false;
	}

	static size_t CompressBufferIntoStream(const void* inData, size_t inDataSize, StreamWriter& outWriter)
	{
		static constexpr size_t chunkStepSize = 0x4000;

		z_stream zStream = {};
		zStream.zalloc = Z_NULL;
		zStream.zfree = Z_NULL;
		zStream.opaque = Z_NULL;

		int errorCode = deflateInit2(&zStream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY);
		assert(errorCode == Z_OK);

		const u8* inDataReadHeader = static_cast<const u8*>(inData);
		size_t remainingSize = inDataSize;
		size_t compressedSize = 0;

		while (remainingSize > 0)
		{
			const size_t chunkSize = Min(remainingSize, chunkStepSize);

			zStream.avail_in = static_cast<uInt>(chunkSize);
			zStream.next_in = reinterpret_cast<const Bytef*>(inDataReadHeader);

			inDataReadHeader += chunkSize;
			remainingSize -= chunkSize;

			do
			{
				std::array<u8, chunkStepSize> outputBuffer;

				zStream.avail_out = chunkStepSize;
				zStream.next_out = outputBuffer.data();

				errorCode = deflate(&zStream, remainingSize == 0 ? Z_FINISH : Z_NO_FLUSH);
				assert(errorCode != Z_STREAM_ERROR);

				const auto compressedChunkSize = chunkStepSize - zStream.avail_out;
				outWriter.WriteBuffer(outputBuffer.data(), compressedChunkSize);

				compressedSize += compressedChunkSize;
			}
			while (zStream.avail_out == 0);
			assert(zStream.avail_in == 0);
		}

		deflateEnd(&zStream);

		assert(errorCode == Z_STREAM_END);
		return compressedSize;
	}

	void FArcPacker::AddFile(std::string fileName, IStreamWritable& writable)
	{
		WritableEntries.push_back(StreamWritableEntry { std::move(fileName), writable });
	}

	void FArcPacker::AddFile(std::string fileName, const void* fileContent, size_t fileSize)
	{
		if (fileContent != nullptr)
			DataPointerEntries.push_back(DataPointerEntry { std::move(fileName), fileContent, fileSize });
	}

	b8 FArcPacker::CreateFlushFArc(std::string_view filePath, b8 compressed, u32 alignment)
	{
		defer { WritableEntries.clear(); DataPointerEntries.clear(); };
		if (filePath.empty())
			return false;

		FileStream outputFileStream; outputFileStream.CreateWrite(filePath);
		if (!outputFileStream.IsOpen())
			return false;

		StreamWriter farcWriter { outputFileStream };
		farcWriter.SetEndianness(Endianness::Big);
		farcWriter.SetPtrSize(PtrSize::Mode32Bit);

		farcWriter.WriteU32(static_cast<u32>(compressed ? FArcSignature::Compressed : FArcSignature::UnCompressed));
		u32 delayedHeaderSize = 0;
		farcWriter.WriteDelayedPtr([&delayedHeaderSize](StreamWriter& writer) {writer.WriteU32(delayedHeaderSize); });
		farcWriter.WriteU32(alignment);

		for (auto& entry : WritableEntries)
		{
			farcWriter.WriteStr(entry.FileName);
			farcWriter.WriteFuncPtr([&](StreamWriter& writer)
			{
				std::unique_ptr<u8[]> fileDataBuffer;
				MemoryWriteStream fileWriteMemoryStream { fileDataBuffer };
				StreamWriter fileWriter { fileWriteMemoryStream };

				entry.Writable.Write(fileWriter);
				entry.FileSizeOnceWritten = static_cast<size_t>(fileWriteMemoryStream.GetLength());
				entry.CompressedFileSizeOnceWritten = entry.FileSizeOnceWritten;

				if (compressed)
					entry.CompressedFileSizeOnceWritten = CompressBufferIntoStream(fileDataBuffer.get(), entry.FileSizeOnceWritten, writer);
				else
					farcWriter.WriteBuffer(fileDataBuffer.get(), entry.FileSizeOnceWritten);

				farcWriter.WriteAlignmentPadding(alignment);
			});

			if (compressed)
				farcWriter.WriteDelayedPtr([&](StreamWriter& writer) { writer.WriteSize(entry.CompressedFileSizeOnceWritten); });

			farcWriter.WriteDelayedPtr([&](StreamWriter& writer) { writer.WriteSize(entry.FileSizeOnceWritten); });
		}

		for (auto& entry : DataPointerEntries)
		{
			farcWriter.WriteStr(entry.FileName);
			farcWriter.WriteFuncPtr([&](StreamWriter& writer)
			{
				if (compressed)
					entry.CompressedFileSizeOnceWritten = CompressBufferIntoStream(entry.Data, entry.DataSize, writer);
				else
					farcWriter.WriteBuffer(entry.Data, entry.DataSize);

				farcWriter.WriteAlignmentPadding(alignment);
			});

			if (compressed)
				farcWriter.WriteDelayedPtr([&](StreamWriter& writer) { writer.WriteSize(entry.CompressedFileSizeOnceWritten); });

			farcWriter.WriteSize(entry.DataSize);
		}

		delayedHeaderSize = static_cast<u32>(farcWriter.GetPosition()) - (sizeof(u32) * 2);
		farcWriter.WriteAlignmentPadding(alignment);

		farcWriter.FlushPointerPool();
		farcWriter.FlushDelayedWritePool();
		farcWriter.WriteAlignmentPadding(alignment);
		return true;
	}
}
