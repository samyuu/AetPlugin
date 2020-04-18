#pragma once
#include "FarcUtil.h"
#include "FileSystem/Stream/Stream.h"
#include "FileSystem/Stream/FileStream.h"
#include "FileSystem/BinaryWriter.h"
#include "Misc/StringHelper.h"
#include "Misc/EndianHelper.h"

namespace FarcUtil
{
	using namespace Comfy;

	namespace
	{
		// TODO: Properly implement as part of Comfy
		class MemoryWriteStream : public FileSystem::IStream
		{
		public:
			MemoryWriteStream(UniquePtr<uint8_t[]>& dataBuffer) : dataBuffer(dataBuffer)
			{
				constexpr size_t initialSize = 0x4000;
				dataBuffer = MakeUnique<uint8_t[]>(dataBufferAllocatedSize = initialSize);
			}
			~MemoryWriteStream() = default;

		public:
			bool EndOfFile() const override { return GetPosition() >= GetLength(); }
			void Skip(FileAddr amount) override { Seek(GetPosition() + amount); }
			void Rewind() override { Seek(FileAddr(0)); }

			void Seek(FileAddr position) override { dataPosition = std::min(static_cast<int64_t>(position), dataLength); }

			FileAddr RemainingBytes() const override { return GetLength() - GetPosition(); }
			FileAddr GetPosition() const override { return static_cast<FileAddr>(dataPosition); }
			FileAddr GetLength() const override { return static_cast<FileAddr>(dataLength); }

			bool IsOpen() const override { return true; }
			bool CanRead() const override { return false; }
			bool CanWrite() const override { return true; }

			size_t ReadBuffer(void* buffer, size_t size) override { return 0; }
			size_t WriteBuffer(const void* buffer, size_t size) override
			{
				const auto newDataPosition = dataPosition + size;
				if (newDataPosition > dataBufferAllocatedSize)
				{
					const auto newAllocatedSize = std::max(dataBufferAllocatedSize * 2, newDataPosition);

					auto newDataBuffer = MakeUnique<uint8_t[]>(newAllocatedSize);
					std::memcpy(newDataBuffer.get(), dataBuffer.get(), dataLength);

					dataBuffer = std::move(newDataBuffer);
					dataBufferAllocatedSize = newAllocatedSize;
				}

				std::memcpy(dataBuffer.get() + dataPosition, buffer, size);
				dataPosition = newDataPosition;

				if (dataPosition > dataLength)
					dataLength = dataPosition;

				return size;
			}

			void Close() {}

		protected:
			UniquePtr<uint8_t[]>& dataBuffer;
			size_t dataBufferAllocatedSize = 0;

			int64_t dataPosition = 0, dataLength = 0;
		};
	}

	void WriteUncompressedFarc(std::string_view filePath, FileSystem::IBinaryWritable& writable, std::string_view fileName)
	{
		UniquePtr<uint8_t[]> fileDataBuffer;
		auto fileMemoryStream = MemoryWriteStream(fileDataBuffer);
		auto fileBinaryWriter = FileSystem::BinaryWriter(fileMemoryStream);
		writable.Write(fileBinaryWriter);

		constexpr auto alignment = 0x10;
		auto farcFileStream = FileSystem::FileStream();
		farcFileStream.CreateWrite(Utf8ToUtf16(filePath));
		auto farcWriter = FileSystem::BinaryWriter(farcFileStream);
		farcWriter.WriteU32(ByteSwapU32('FArc'));
		const FileAddr headerSizeOffset = farcWriter.GetPosition();
		farcWriter.WritePadding(sizeof(uint32_t));
		farcWriter.WriteU32(ByteSwapU32(alignment));
		farcWriter.WriteStr(fileName);
		const FileAddr headerSize = farcWriter.GetPosition();
		farcWriter.SetPosition(headerSizeOffset);
		farcWriter.WriteU32(ByteSwapU32(static_cast<uint32_t>(headerSize)));
		farcWriter.SetPosition(headerSize);
		const FileAddr fileOffsetOffset = farcWriter.GetPosition();
		farcWriter.WritePadding(sizeof(uint32_t));
		farcWriter.WriteU32(ByteSwapU32(static_cast<uint32_t>(fileBinaryWriter.GetLength())));
		farcWriter.WriteAlignmentPadding(alignment);
		const FileAddr fileOffset = farcWriter.GetPosition();
		farcWriter.WriteBuffer(fileDataBuffer.get(), static_cast<size_t>(fileBinaryWriter.GetLength()));
		farcWriter.WriteAlignmentPadding(alignment);
		farcWriter.SetPosition(fileOffsetOffset);
		farcWriter.WriteU32(ByteSwapU32(static_cast<uint32_t>(fileOffset)));
	}
}
