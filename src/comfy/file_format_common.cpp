#include "file_format_common.h"
#include <Windows.h>

namespace Comfy
{
	FileStream::FileStream(FileStream&& other) : FileStream()
	{
		canRead = other.canRead;
		canWrite = other.canWrite;
		position = other.position;
		fileSize = other.fileSize;
		fileHandle = other.fileHandle;

		other.canRead = false;
		other.canWrite = false;
		other.position = {};
		other.fileSize = {};
		other.fileHandle = nullptr;
	}

	void FileStream::Seek(FileAddr position)
	{
		::LARGE_INTEGER distanceToMove;
		distanceToMove.QuadPart = static_cast<LONGLONG>(position);
		::SetFilePointerEx(fileHandle, distanceToMove, NULL, FILE_BEGIN);

		this->position = position;
	}

	b8 FileStream::IsOpen() const
	{
		return (fileHandle != INVALID_HANDLE_VALUE);
	}

	size_t FileStream::ReadBuffer(void* buffer, size_t size)
	{
		assert(canRead);

		DWORD bytesRead = 0;
		::ReadFile(fileHandle, buffer, static_cast<DWORD>(size), &bytesRead, nullptr);

		position += static_cast<FileAddr>(bytesRead);
		return bytesRead;
	}

	size_t FileStream::WriteBuffer(const void* buffer, size_t size)
	{
		assert(canWrite);

		DWORD bytesWritten = 0;
		::WriteFile(fileHandle, buffer, static_cast<DWORD>(size), &bytesWritten, nullptr);

		if (position > (GetLength() - static_cast<FileAddr>(bytesWritten)))
			fileSize += static_cast<FileAddr>(bytesWritten);
		position += static_cast<FileAddr>(bytesWritten);

		return bytesWritten;
	}

	void FileStream::OpenRead(std::string_view filePath)
	{
		assert(fileHandle == nullptr || fileHandle == INVALID_HANDLE_VALUE);
		fileHandle = ::CreateFileW(UTF8::WideArg(filePath).c_str(), (GENERIC_READ), (FILE_SHARE_READ | FILE_SHARE_WRITE), NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (fileHandle != INVALID_HANDLE_VALUE)
			canRead = true;
		InternalUpdateFileSize();
	}

	void FileStream::OpenWrite(std::string_view filePath)
	{
		assert(fileHandle == nullptr || fileHandle == INVALID_HANDLE_VALUE);
		fileHandle = ::CreateFileW(UTF8::WideArg(filePath).c_str(), (GENERIC_WRITE), (FILE_SHARE_READ | FILE_SHARE_WRITE), NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (fileHandle != INVALID_HANDLE_VALUE)
			canWrite = true;
		InternalUpdateFileSize();
	}

	void FileStream::OpenReadWrite(std::string_view filePath)
	{
		assert(fileHandle == nullptr || fileHandle == INVALID_HANDLE_VALUE);
		fileHandle = ::CreateFileW(UTF8::WideArg(filePath).c_str(), (GENERIC_READ | GENERIC_WRITE), (FILE_SHARE_READ | FILE_SHARE_WRITE), NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (fileHandle != INVALID_HANDLE_VALUE)
		{
			canRead = true;
			canWrite = true;
		}
		InternalUpdateFileSize();
	}

	void FileStream::CreateWrite(std::string_view filePath)
	{
		assert(fileHandle == nullptr || fileHandle == INVALID_HANDLE_VALUE);
		fileHandle = ::CreateFileW(UTF8::WideArg(filePath).c_str(), (GENERIC_WRITE), (FILE_SHARE_READ | FILE_SHARE_WRITE), NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

		if (fileHandle != INVALID_HANDLE_VALUE)
			canWrite = true;
		InternalUpdateFileSize();
	}

	void FileStream::CreateReadWrite(std::string_view filePath)
	{
		assert(fileHandle == nullptr || fileHandle == INVALID_HANDLE_VALUE);
		fileHandle = ::CreateFileW(UTF8::WideArg(filePath).c_str(), (GENERIC_READ | GENERIC_WRITE), (FILE_SHARE_READ | FILE_SHARE_WRITE), NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

		if (fileHandle != INVALID_HANDLE_VALUE)
		{
			canRead = true;
			canWrite = true;
		}
		InternalUpdateFileSize();
	}

	void FileStream::Close()
	{
		if (fileHandle != INVALID_HANDLE_VALUE)
			::CloseHandle(fileHandle);
		fileHandle = nullptr;
	}

	void FileStream::InternalUpdateFileSize()
	{
		if (fileHandle != INVALID_HANDLE_VALUE)
		{
			::LARGE_INTEGER largeIntegerFileSize = {};
			::GetFileSizeEx(fileHandle, &largeIntegerFileSize);
			fileSize = static_cast<FileAddr>(largeIntegerFileSize.QuadPart);
		}
		else
		{
			fileSize = {};
			canRead = false;
			canWrite = false;
		}
	}

	MemoryStream::MemoryStream(MemoryStream&& other) : MemoryStream()
	{
		if (other.IsOwning())
			owningDataVector = std::move(other.owningDataVector);
		else
			dataVectorPtr = other.dataVectorPtr;

		isOpen = other.isOpen;
		position = other.position;
		dataSize = other.dataSize;

		other.Close();
	}

	size_t MemoryStream::ReadBuffer(void* buffer, size_t size)
	{
		assert(dataVectorPtr != nullptr);
		const auto remainingSize = (GetLength() - GetPosition());
		const i64 bytesRead = Min(static_cast<i64>(size), static_cast<i64>(remainingSize));

		void* source = &(*dataVectorPtr)[static_cast<size_t>(position)];
		memcpy(buffer, source, bytesRead);

		position += static_cast<FileAddr>(bytesRead);
		return bytesRead;
	}

	size_t MemoryStream::WriteBuffer(const void* buffer, size_t size)
	{
		assert(dataVectorPtr != nullptr);
		dataVectorPtr->resize(dataVectorPtr->size() + size);

		const u8* bufferStart = reinterpret_cast<const u8*>(buffer);
		const u8* bufferEnd = &bufferStart[size];
		std::copy(bufferStart, bufferEnd, std::back_inserter(*dataVectorPtr));

		return size;
	}

	void MemoryStream::FromStreamSource(std::vector<u8>& source)
	{
		isOpen = true;
		dataSize = static_cast<FileAddr>(source.size());
		dataVectorPtr = &source;
	}

	void MemoryStream::FromStream(IStream& stream)
	{
		assert(stream.CanRead());

		isOpen = true;
		dataSize = stream.GetLength() - stream.GetPosition();
		dataVectorPtr->resize(static_cast<size_t>(dataSize));

		const auto prePos = stream.GetPosition();
		stream.ReadBuffer(dataVectorPtr->data(), static_cast<size_t>(dataSize));
		stream.Seek(prePos);
	}

	void MemoryStream::OpenReadMemory(std::string_view filePath)
	{
		FileStream fileStream;
		fileStream.OpenRead(filePath);
		if (fileStream.IsOpen() && fileStream.CanRead())
			FromStream(fileStream);
	}

	void MemoryStream::Close()
	{
		isOpen = false;
		position = {};
		dataSize = {};
		dataVectorPtr = nullptr;
		owningDataVector.clear();
	}

	MemoryWriteStream::MemoryWriteStream(std::unique_ptr<u8[]>& dataBuffer) : dataBuffer(dataBuffer)
	{
		static constexpr size_t reasonableInitialSize = 0x4000;
		dataBuffer = std::make_unique<u8[]>(dataBufferAllocatedSize = reasonableInitialSize);
	}

	size_t MemoryWriteStream::WriteBuffer(const void* buffer, size_t size)
	{
		const auto newDataPosition = dataPosition + size;
		if (newDataPosition > dataBufferAllocatedSize)
		{
			const auto newAllocatedSize = Max(dataBufferAllocatedSize * 2, newDataPosition);

			auto newDataBuffer = std::make_unique<u8[]>(newAllocatedSize);
			memcpy(newDataBuffer.get(), dataBuffer.get(), dataLength);

			dataBuffer = std::move(newDataBuffer);
			dataBufferAllocatedSize = newAllocatedSize;
		}

		memcpy(dataBuffer.get() + dataPosition, buffer, size);
		dataPosition = newDataPosition;

		if (dataPosition > dataLength)
			dataLength = dataPosition;

		return size;
	}

	void StreamWriter::WritePadding(size_t size, u32 paddingValue)
	{
		if (size < 0)
			return;

		static constexpr size_t maxSize = 32;
		assert(size <= maxSize);
		u8 paddingValues[maxSize];

		std::memset(paddingValues, paddingValue, size);
		WriteBuffer(paddingValues, size);
	}

	void StreamWriter::WriteAlignmentPadding(i32 alignment, u32 paddingValue)
	{
		static constexpr b8 forceExtraPadding = false;
		static constexpr i32 maxAlignment = 32;

		assert(alignment <= maxAlignment);
		u8 paddingValues[maxAlignment];

		const i32 value = static_cast<i32>(GetPosition());
		i32 paddingSize = ((value + (alignment - 1)) & ~(alignment - 1)) - value;

		if (forceExtraPadding && paddingSize <= 0)
			paddingSize = alignment;

		if (paddingSize > 0)
		{
			std::memset(paddingValues, paddingValue, paddingSize);
			WriteBuffer(paddingValues, paddingSize);
		}
	}

	void StreamWriter::FlushStringPointerPool()
	{
		for (const auto& value : StringPointerPool)
		{
			const auto originalStringOffset = GetPosition();
			auto stringOffset = originalStringOffset;

			b8 pooledStringFound = false;
			if (Settings.PoolStrings)
			{
				if (auto pooledString = WrittenStringPool.find(std::string(value.String)); pooledString != WrittenStringPool.end())
				{
					stringOffset = pooledString->second;
					pooledStringFound = true;
				}
			}

			SeekOffsetAware(value.ReturnAddress);
			WritePtr(stringOffset);

			if (!pooledStringFound)
			{
				SeekOffsetAware(stringOffset);
				WriteStr(value.String);

				if (value.Alignment > 0)
					WriteAlignmentPadding(value.Alignment);
			}
			else
			{
				SeekOffsetAware(originalStringOffset);
			}

			if (Settings.PoolStrings && !pooledStringFound)
				WrittenStringPool.insert(std::make_pair(value.String, stringOffset));
		}

		if (Settings.PoolStrings)
			WrittenStringPool.clear();

		StringPointerPool.clear();
	}

	void StreamWriter::FlushPointerPool()
	{
		for (const auto& value : PointerPool)
		{
			const auto offset = GetPosition();

			SeekOffsetAware(value.ReturnAddress);
			WritePtr(offset - value.BaseAddress);

			SeekOffsetAware(offset);
			value.Func(*this);
		}

		PointerPool.clear();
	}

	void StreamWriter::FlushDelayedWritePool()
	{
		for (const auto& value : DelayedWritePool)
		{
			const auto offset = GetPosition();

			SeekOffsetAware(value.ReturnAddress);
			value.Func(*this);

			SeekOffsetAware(offset);
		}

		DelayedWritePool.clear();
	}

	SectionHeader SectionHeader::Read(StreamReader& reader)
	{
		const auto headerAddress = reader.GetPosition();
		const auto signature = static_cast<SectionSignature>(reader.ReadU32_LE());
		const auto sectionSize = reader.ReadU32_LE();
		const auto dataOffset = reader.ReadU32_LE();
		const auto endianness = static_cast<SectionEndianness>(reader.ReadU32_LE());
		const auto depth = reader.ReadU32_LE();
		const auto dataSize = reader.ReadU32_LE();
		const auto reserved0 = reader.ReadU32_LE();
		const auto reserved1 = reader.ReadU32_LE();

		SectionHeader header;
		header.HeaderAddress = headerAddress;
		header.DataOffset = static_cast<FileAddr>(dataOffset);
		header.SectionSize = static_cast<size_t>(sectionSize);
		header.DataSize = static_cast<size_t>(dataSize);
		header.Signature = signature;
		header.Depth = depth;
		header.Endianness = (endianness == SectionEndianness::Big) ? Endianness::Big : Endianness::Little;
		return header;
	}

	std::optional<SectionHeader> SectionHeader::TryRead(StreamReader& reader, SectionSignature expectedSignature)
	{
		const auto startPosition = reader.GetPosition();
		const auto readSignature = static_cast<SectionSignature>(reader.ReadU32_LE());
		reader.Seek(startPosition);

		if (readSignature != expectedSignature)
			return {};

		reader.HasSections = true;
		return SectionHeader::Read(reader);
	}

	void SectionHeader::ScanPOFSectionsForPtrSize(StreamReader& reader)
	{
		if (!reader.HasSections || reader.HasBeenPtrSizeScanned)
			return;

		reader.ReadAt(FileAddr::NullPtr, [](StreamReader& reader)
		{
			const auto endOfSectionHeaders = reader.GetLength() - FileAddr(32);
			while (reader.GetPosition() < endOfSectionHeaders)
			{
				const auto sectionHeader = SectionHeader::Read(reader);
				if (sectionHeader.Signature == SectionSignature::POF0) { reader.SetPtrSize(PtrSize::Mode32Bit); return; }
				if (sectionHeader.Signature == SectionSignature::POF1) { reader.SetPtrSize(PtrSize::Mode64Bit); return; }

				const auto endOfSection = sectionHeader.EndOfSubSectionAddress();
				if (endOfSection < reader.GetPosition())
					return;

				reader.Seek(endOfSection);
			}

			// NOTE: In case no relocation tables are found 32-bit should be the default
			reader.SetPtrSize(PtrSize::Mode32Bit);
		});

		reader.HasBeenPtrSizeScanned = true;
	}
}
