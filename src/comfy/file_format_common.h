#pragma once
#include "core_types.h"
#include "core_string.h"
#include <vector>
#include <list>
#include <map>
#include <stack>
#include <functional>
#include <optional>
#include <future>

namespace Comfy
{
	struct IStream
	{
		virtual ~IStream() = default;
		virtual void Seek(FileAddr position) = 0;
		virtual FileAddr GetPosition() const = 0;
		virtual FileAddr GetLength() const = 0;
		virtual b8 IsOpen() const = 0;
		virtual b8 CanRead() const = 0;
		virtual b8 CanWrite() const = 0;
		virtual size_t ReadBuffer(void* buffer, size_t size) = 0;
		virtual size_t WriteBuffer(const void* buffer, size_t size) = 0;
		virtual void Close() = 0;
	};

	struct FileStream final : IStream, NonCopyable
	{
		FileStream() = default;
		FileStream(FileStream&& other);
		~FileStream() { Close(); }

		void Seek(FileAddr position) override;
		inline FileAddr GetPosition() const override { return position; }
		inline FileAddr GetLength() const override { return fileSize; }

		b8 IsOpen() const override;
		inline b8 CanRead() const override { return canRead; }
		inline b8 CanWrite() const override { return canWrite; }

		size_t ReadBuffer(void* buffer, size_t size) override;
		size_t WriteBuffer(const void* buffer, size_t size) override;

		void OpenRead(std::string_view filePath);
		void OpenWrite(std::string_view filePath);
		void OpenReadWrite(std::string_view filePath);
		void CreateWrite(std::string_view filePath);
		void CreateReadWrite(std::string_view filePath);
		void Close() override;

	protected:
		void InternalUpdateFileSize();

		b8 canRead = false;
		b8 canWrite = false;
		FileAddr position = {};
		FileAddr fileSize = {};
		void* fileHandle = nullptr;
	};

	struct MemoryStream final : IStream, NonCopyable
	{
		MemoryStream() { dataVectorPtr = &owningDataVector; }
		MemoryStream(MemoryStream&& other);
		~MemoryStream() { Close(); }

		inline void Seek(FileAddr position) override { this->position = Min(position, GetLength()); }
		inline FileAddr GetPosition() const override { return position; }
		inline FileAddr GetLength() const override { return dataSize; }

		inline b8 IsOpen() const override { return (isOpen && dataVectorPtr != nullptr); }
		inline b8 CanRead() const override { return (isOpen && dataVectorPtr != nullptr); }
		inline b8 CanWrite() const override { return false; }
		inline b8 IsOwning() const { return (dataVectorPtr == &owningDataVector); }

		size_t ReadBuffer(void* buffer, size_t size) override;
		size_t WriteBuffer(const void* buffer, size_t size) override;

		void FromStreamSource(std::vector<u8>& source);
		void FromStream(IStream& stream);
		void OpenReadMemory(std::string_view filePath);

		template <typename Func>
		void FromBuffer(size_t size, Func fromBufferFunc)
		{
			isOpen = true;
			owningDataVector.resize(size);
			fromBufferFunc(static_cast<void*>(owningDataVector.data()), size);
			dataSize = static_cast<FileAddr>(size);
		}

		void Close() override;

	protected:
		b8 isOpen = false;
		FileAddr position = {};
		FileAddr dataSize = {};
		std::vector<u8>* dataVectorPtr = nullptr;
		std::vector<u8> owningDataVector;
	};

	struct MemoryWriteStream : IStream, NonCopyable
	{
		MemoryWriteStream(std::unique_ptr<u8[]>& dataBuffer);
		~MemoryWriteStream() = default;

		inline void Seek(FileAddr position) override { dataPosition = Clamp(static_cast<i64>(position), 0i64, dataLength); }
		inline FileAddr GetPosition() const override { return static_cast<FileAddr>(dataPosition); }
		inline FileAddr GetLength() const override { return static_cast<FileAddr>(dataLength); }

		inline b8 IsOpen() const override { return true; }
		inline b8 CanRead() const override { return false; }
		inline b8 CanWrite() const override { return true; }

		inline size_t ReadBuffer(void* buffer, size_t size) override { return 0; }
		size_t WriteBuffer(const void* buffer, size_t size) override;

		inline void Close() override {}

	protected:
		std::unique_ptr<u8[]>& dataBuffer;
		size_t dataBufferAllocatedSize = 0;
		i64 dataPosition = 0, dataLength = 0;
	};

	enum class PtrSize : u8 { Mode32Bit, Mode64Bit };

	struct StreamReadWriteBase
	{
		IStream* Stream = nullptr;
		b8 Is64 = false;
		b8 IsBE = false;
		b8 HasSections = false;
		b8 HasBeenPtrSizeScanned = false;
		FileAddr BaseOffset = FileAddr::NullPtr;
		std::stack<FileAddr> BaseOffsetStack;

		StreamReadWriteBase(IStream& stream) : Stream(&stream) {}
		inline void Seek(FileAddr position) { return Stream->Seek(position); }
		inline void SeekOffsetAware(FileAddr position) { return Stream->Seek(position + BaseOffset); }
		inline void Skip(FileAddr increment) { return Stream->Seek(Stream->GetPosition() + increment); }
		inline void PushBaseOffset() { BaseOffsetStack.push(BaseOffset = GetPosition()); }
		inline void PopBaseOffset() { BaseOffsetStack.pop(); BaseOffset = (BaseOffsetStack.empty() ? FileAddr::NullPtr : BaseOffsetStack.top()); }
		inline FileAddr GetPosition() const { return Stream->GetPosition(); }
		inline FileAddr GetPositionOffsetAware() const { return Stream->GetPosition() - BaseOffset; }
		inline FileAddr GetLength() const { return Stream->GetLength(); }
		inline FileAddr GetRemaining() const { return Stream->GetLength() - Stream->GetPosition(); }
		inline b8 IsEOF() const { return Stream->GetPosition() >= Stream->GetLength(); }
		inline PtrSize GetPtrSize() const { return Is64 ? PtrSize::Mode64Bit : PtrSize::Mode32Bit; }
		inline void SetPtrSize(PtrSize value) { Is64 = (value == PtrSize::Mode64Bit); }
		inline Endianness GetEndianness() const { return IsBE ? Endianness::Big : Endianness::Little; }
		inline void SetEndianness(Endianness value) { IsBE = (value == Endianness::Big); }
	};

	struct StreamReader final : StreamReadWriteBase
	{
		struct SettingsData
		{
			b8 EmptyNullStringPointers = false;
		} Settings;

		explicit StreamReader(IStream& stream) : StreamReadWriteBase(stream) { assert(stream.CanRead()); }
		inline void SeekAlign(i32 alignment) { i64 p = static_cast<i64>(GetPosition()); i64 d = ((p + (alignment - 1)) & ~(alignment - 1)) - p; if (d > 0) Skip(static_cast<FileAddr>(d)); }
		template <typename T> inline T ReadT_Native() { T value; ReadBuffer(&value, sizeof(value)); return value; }
		inline size_t ReadBuffer(void* buffer, size_t size) { return Stream->ReadBuffer(buffer, size); }
		inline FileAddr ReadPtr_32() { return static_cast<FileAddr>(ReadI32()); }
		inline FileAddr ReadPtr_64() { return static_cast<FileAddr>(ReadI64()); }
		inline size_t ReadSize_32() { return static_cast<size_t>(ReadU32()); }
		inline size_t ReadSize_64() { return static_cast<size_t>(ReadU64()); }
		inline i16 ReadI16_LE() { return ReadT_Native<i16>(); }
		inline u16 ReadU16_LE() { return ReadT_Native<u16>(); }
		inline i32 ReadI32_LE() { return ReadT_Native<i32>(); }
		inline u32 ReadU32_LE() { return ReadT_Native<u32>(); }
		inline i64 ReadI64_LE() { return ReadT_Native<i64>(); }
		inline u64 ReadU64_LE() { return ReadT_Native<u64>(); }
		inline f32 ReadF32_LE() { return ReadT_Native<f32>(); }
		inline f64 ReadF64_LE() { return ReadT_Native<f64>(); }
		inline i16 ReadI16_BE() { return ByteSwapI16(ReadI16_LE()); }
		inline u16 ReadU16_BE() { return ByteSwapU16(ReadU16_LE()); }
		inline i32 ReadI32_BE() { return ByteSwapI32(ReadI32_LE()); }
		inline u32 ReadU32_BE() { return ByteSwapU32(ReadU32_LE()); }
		inline i64 ReadI64_BE() { return ByteSwapI64(ReadI64_LE()); }
		inline u64 ReadU64_BE() { return ByteSwapU64(ReadU64_LE()); }
		inline f32 ReadF32_BE() { return ByteSwapF32(ReadF32_LE()); }
		inline f64 ReadF64_BE() { return ByteSwapF64(ReadF64_LE()); }

		inline FileAddr ReadPtr() { return Is64 ? ReadPtr_64() : ReadPtr_32(); }
		inline size_t ReadSize() { return Is64 ? ReadSize_64() : ReadSize_32(); }
		inline b8 ReadBool() { return ReadT_Native<b8>(); }
		inline char ReadChar() { return ReadT_Native<char>(); }
		inline i8 ReadI8() { return ReadT_Native<i8>(); }
		inline u8 ReadU8() { return ReadT_Native<u8>(); }
		inline i16 ReadI16() { return IsBE ? ReadI16_BE() : ReadI16_LE(); }
		inline u16 ReadU16() { return IsBE ? ReadU16_BE() : ReadU16_LE(); }
		inline i32 ReadI32() { return IsBE ? ReadI32_BE() : ReadI32_LE(); }
		inline u32 ReadU32() { return IsBE ? ReadU32_BE() : ReadU32_LE(); }
		inline i64 ReadI64() { return IsBE ? ReadI64_BE() : ReadI64_LE(); }
		inline u64 ReadU64() { return IsBE ? ReadU64_BE() : ReadU64_LE(); }
		inline f32 ReadF32() { return IsBE ? ReadF32_BE() : ReadF32_LE(); }
		inline f64 ReadF64() { return IsBE ? ReadF64_BE() : ReadF64_LE(); }
		inline vec2 ReadVec2() { vec2 v; v.x = ReadF32(); v.y = ReadF32(); return v; }
		inline vec4 ReadVec4() { vec4 v; v.x = ReadF32(); v.y = ReadF32(); v.z = ReadF32(); v.w = ReadF32(); return v; }
		inline ivec2 ReadIVec2() { ivec2 v; v.x = ReadI32(); v.y = ReadI32(); return v; }

		inline b8 IsValidPointer(FileAddr address, b8 offsetAware = true) const { return (address > FileAddr::NullPtr) && ((offsetAware ? address + BaseOffset : address) <= Stream->GetLength()); }
		template <typename Func> inline void ReadAt(FileAddr position, Func func) { auto pre = GetPosition(); Seek(position); func(*this); Seek(pre); }
		template <typename Func> inline void ReadAtOffsetAware(FileAddr position, Func func) { ReadAt(position + BaseOffset, func); }
		template <typename T, typename Func> inline T ReadValueAt(FileAddr position, Func func) { auto pre = GetPosition(); Seek(position); T v = func(*this); Seek(pre); return v; }
		template <typename T, typename Func> inline T ReadValueAtOffsetAware(FileAddr position, Func func) { return ReadValueAt<T>(position + BaseOffset, func); }

		inline std::string ReadStr()
		{
			size_t length = sizeof('\0');
			ReadAt(GetPosition(), [&length](StreamReader& reader) { while (reader.ReadChar() != '\0' && !reader.IsEOF()) length++; });
			if (length == sizeof(char))
				return "";
			auto v = std::string(length - sizeof(char), '\0');
			ReadBuffer(v.data(), length * sizeof(char) - sizeof('\0'));
			Skip(static_cast<FileAddr>(sizeof('\0')));
			return v;
		}
		inline std::string ReadStrAtOffsetAware(FileAddr position)
		{
			if (Settings.EmptyNullStringPointers && position == FileAddr::NullPtr)
				return "";
			return ReadValueAtOffsetAware<std::string>(position, [](StreamReader& reader) { return reader.ReadStr(); });
		}
		inline std::string ReadStr(size_t size) { auto v = std::string(size, '\0'); ReadBuffer(v.data(), size * sizeof(char)); return v; }
		inline std::string ReadStrPtrOffsetAware() { return ReadStrAtOffsetAware(ReadPtr()); }
	};

	struct StreamWriter final : StreamReadWriteBase
	{
		struct Settings
		{
			b8 EmptyNullStringPointers = false;
			b8 PoolStrings = true;
		} Settings;

		struct FunctionPointerEntry { FileAddr ReturnAddress, BaseAddress; std::function<void(StreamWriter&)> Func; };
		struct StringPointerEntry { FileAddr ReturnAddress; std::string_view String; i32 Alignment; };
		struct DelayedWriteEntry { FileAddr ReturnAddress; std::function<void(StreamWriter&)> Func; };

		// NOTE: Using std::list here to avoid invalidating previous entries while executing recursive pointer writes
		std::list<FunctionPointerEntry> PointerPool;
		std::vector<StringPointerEntry> StringPointerPool;
		std::vector<DelayedWriteEntry> DelayedWritePool;
		std::unordered_map<std::string, FileAddr> WrittenStringPool;

		explicit StreamWriter(IStream& stream) : StreamReadWriteBase(stream) { assert(stream.CanWrite()); }
		template <typename T> inline void WriteT_Native(T value) { WriteBuffer(&value, sizeof(T)); }
		inline size_t WriteBuffer(const void* buffer, size_t size) { return Stream->WriteBuffer(buffer, size); }
		inline void WritePtr_32(FileAddr value) { WriteI32(static_cast<i32>(value)); }
		inline void WritePtr_64(FileAddr value) { WriteI64(static_cast<i64>(value)); }
		inline void WriteSize_32(size_t value) { WriteU32(static_cast<u32>(value)); }
		inline void WriteSize_64(size_t value) { WriteU64(static_cast<u64>(value)); }
		inline void WriteI16_LE(i16 value) { WriteT_Native<i16>(value); }
		inline void WriteU16_LE(u16 value) { WriteT_Native<u16>(value); }
		inline void WriteI32_LE(i32 value) { WriteT_Native<i32>(value); }
		inline void WriteU32_LE(u32 value) { WriteT_Native<u32>(value); }
		inline void WriteI64_LE(i64 value) { WriteT_Native<i64>(value); }
		inline void WriteU64_LE(u64 value) { WriteT_Native<u64>(value); }
		inline void WriteF32_LE(f32 value) { WriteT_Native<f32>(value); }
		inline void WriteF64_LE(f64 value) { WriteT_Native<f64>(value); }
		inline void WriteI16_BE(i16 value) { WriteI16_LE(ByteSwapI16(value)); }
		inline void WriteU16_BE(u16 value) { WriteU16_LE(ByteSwapU16(value)); }
		inline void WriteI32_BE(i32 value) { WriteI32_LE(ByteSwapI32(value)); }
		inline void WriteU32_BE(u32 value) { WriteU32_LE(ByteSwapU32(value)); }
		inline void WriteI64_BE(i64 value) { WriteI64_LE(ByteSwapI64(value)); }
		inline void WriteU64_BE(u64 value) { WriteU64_LE(ByteSwapU64(value)); }
		inline void WriteF32_BE(f32 value) { WriteF32_LE(ByteSwapF32(value)); }
		inline void WriteF64_BE(f64 value) { WriteF64_LE(ByteSwapF64(value)); }

		inline void WritePtr(FileAddr value) { Is64 ? WritePtr_64(value) : WritePtr_32(value); }
		inline void WriteSize(size_t value) { Is64 ? WriteSize_64(value) : WriteSize_32(value); }
		inline void WriteBool(b8 value) { WriteT_Native<b8>(value); }
		inline void WriteChar(char value) { WriteT_Native<char>(value); }
		inline void WriteI8(i8 value) { WriteT_Native<i8>(value); }
		inline void WriteU8(u8 value) { WriteT_Native<u8>(value); }
		inline void WriteI16(i16 value) { IsBE ? WriteI16_BE(value) : WriteI16_LE(value); }
		inline void WriteU16(u16 value) { IsBE ? WriteU16_BE(value) : WriteU16_LE(value); }
		inline void WriteI32(i32 value) { IsBE ? WriteI32_BE(value) : WriteI32_LE(value); }
		inline void WriteU32(u32 value) { IsBE ? WriteU32_BE(value) : WriteU32_LE(value); }
		inline void WriteI64(i64 value) { IsBE ? WriteI64_BE(value) : WriteI64_LE(value); }
		inline void WriteU64(u64 value) { IsBE ? WriteU64_BE(value) : WriteU64_LE(value); }
		inline void WriteF32(f32 value) { IsBE ? WriteF32_BE(value) : WriteF32_LE(value); }
		inline void WriteF64(f64 value) { IsBE ? WriteF64_BE(value) : WriteF64_LE(value); }

		inline void WriteStr(std::string_view value) { WriteBuffer(value.data(), value.size()); WriteChar('\0'); }
		inline void WriteStrPtr(std::string_view value, i32 alignment = 0)
		{
			if (Settings.EmptyNullStringPointers && value.empty()) { WritePtr(FileAddr::NullPtr); }
			else { StringPointerPool.push_back({ GetPosition(), value, alignment }); WritePtr(FileAddr::NullPtr); }
		}
		template <typename Func>
		inline void WriteFuncPtr(Func func, FileAddr baseAddress = FileAddr::NullPtr) { PointerPool.push_back({ GetPosition(), baseAddress, std::function<void(StreamWriter&)> { func } }); WritePtr(FileAddr::NullPtr); }
		template <typename Func>
		inline void WriteDelayedPtr(Func func) { DelayedWritePool.push_back({ GetPosition(), std::function<void(StreamWriter&)> { func } }); WritePtr(FileAddr::NullPtr); }

		void WritePadding(size_t size, u32 paddingValue = 0xCCCCCCCC);
		void WriteAlignmentPadding(i32 alignment, u32 paddingValue = 0xCCCCCCCC);

		void FlushStringPointerPool();
		void FlushPointerPool();
		void FlushDelayedWritePool();
	};

	enum class StreamResult : u8
	{
		Success,
		BadFormat,
		BadCount,
		BadPointer,
		InsufficientSpace,
		UnknownError,
		Count
	};

	struct IStreamReadable
	{
		virtual ~IStreamReadable() = default;
		virtual StreamResult Read(StreamReader& reader) = 0;
	};

	struct IStreamWritable
	{
		virtual ~IStreamWritable() = default;
		virtual StreamResult Write(StreamWriter& writer) = 0;
	};

	template <typename Readable>
	std::unique_ptr<Readable> LoadFile(std::string_view filePath)
	{
		static_assert(std::is_base_of_v<IStreamReadable, Readable>);
		MemoryStream stream;
		stream.OpenReadMemory(filePath);
		if (!stream.IsOpen() || !stream.CanRead())
			return nullptr;

		auto out = std::make_unique<Readable>();
		if (out == nullptr)
			return nullptr;

		StreamReader reader { stream };
		if (StreamResult streamResult = out->Read(reader); streamResult != StreamResult::Success)
			return nullptr;

		return out;
	}

	template <typename Writable>
	b8 SaveFile(std::string_view filePath, Writable& writable)
	{
		static_assert(std::is_base_of_v<IStreamWritable, Writable>);
		FileStream stream;
		stream.CreateWrite(filePath);
		if (!stream.IsOpen() || !stream.CanWrite())
			return false;

		StreamWriter writer { stream };
		StreamResult streamResult = writable.Write(writer);
		return (streamResult == StreamResult::Success);
	}

	template <typename Loadable>
	std::future<std::unique_ptr<Loadable>> LoadFileAsync(std::string_view filePath)
	{
		return std::async(std::launch::async, [pathCopy = std::string(filePath)] { return LoadFile<Loadable>(pathCopy); });
	}

	// NOTE: The writable input parameter must outlive the duration of the returned future!
	template <typename Writable>
	std::future<b8> SaveFileAsync(std::string_view filePath, Writable* writable)
	{
		return std::async(std::launch::async, [pathCopy = std::string(filePath), writable] { return (writable != nullptr) ? SaveFile<Writable>(pathCopy, *writable) : false; });
	}

	enum class SectionEndianness : u32
	{
		Little = 0x10000000,
		Big = 0x18000000,
	};

	// NOTE: Defined in little endian byte order as all section header data should be
	enum class SectionSignature : u32
	{
		EOFC = 0x43464F45, // NOTE: End of File
		AEDB = 0x42444541, // NOTE: AetDB
		SPDB = 0x42445053, // NOTE: SprDB
		MOSI = 0x49534F4D, // NOTE: ObjDB
		MTXI = 0x4958544D, // NOTE: TexDB
		AETC = 0x43544541, // NOTE: AetSet
		SPRC = 0x43525053, // NOTE: SprSet
		TXPC = 0x43505854, // NOTE: SprSet.TexSet
		MTXD = 0x4458544D, // NOTE: TexSet
		MOSD = 0x44534F4D, // NOTE: ObjSet
		OMDL = 0x4C444D4F, // NOTE: ObjSet.Obj
		OIDX = 0x5844494F, // NOTE: ObjSet.IndexBuffer
		OVTX = 0x5854564F, // NOTE: ObjSet.VertexBuffer
		OSKN = 0x4E4B534F, // NOTE: ObjSet.Skin
		ENRS = 0x53524E45, // NOTE: Endianness Reverse Table
		POF0 = 0x30464F50, // NOTE: Relocation Table 32-bit
		POF1 = 0x31464F50, // NOTE: Relocation Table 64-bit
	};

	// NOTE: Represents the data extracted from the header not the in-file header layout
	struct SectionHeader
	{
		// NOTE: Absolute address of the header itself within the file
		FileAddr HeaderAddress;
		// NOTE: Relative offset to the header address to the start of the section data
		FileAddr DataOffset;
		// NOTE: Number of bytes until the end of all sub sections including POF / ENRS
		size_t SectionSize;
		// NOTE: Number of bytes until the next sub section or EOFC
		size_t DataSize;
		// NOTE: Little endian signature
		SectionSignature Signature;
		// NOTE: Depth level or 0 for top level sections
		u32 Depth;
		// NOTE: Endianness of the contained section data
		Endianness Endianness;

		inline FileAddr StartOfSubSectionAddress() const { return HeaderAddress + DataOffset; }
		inline FileAddr EndOfSectionAddress() const { return HeaderAddress + DataOffset + static_cast<FileAddr>(SectionSize); }
		inline FileAddr EndOfSubSectionAddress() const { return HeaderAddress + DataOffset + static_cast<FileAddr>(DataSize); }

		static SectionHeader Read(StreamReader& reader);
		static std::optional<SectionHeader> TryRead(StreamReader& reader, SectionSignature expectedSignature);
		static void ScanPOFSectionsForPtrSize(StreamReader& reader);
	};
}
