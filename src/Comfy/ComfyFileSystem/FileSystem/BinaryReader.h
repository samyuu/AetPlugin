#pragma once
#include "CoreTypes.h"
#include "Stream/Stream.h"
#include "BinaryMode.h"
#include "Misc/EndianHelper.h"

namespace Comfy::FileSystem
{
	class BinaryReader : NonCopyable
	{
	public:
		BinaryReader()
		{
			SetPointerMode(PtrMode::Mode32Bit);
			SetEndianness(Endianness::Little);
		}

		BinaryReader(IStream& stream) : BinaryReader()
		{
			OpenStream(stream);
		}

		~BinaryReader()
		{
			Close();
		}

		inline void OpenStream(IStream& stream)
		{
			assert(stream.CanRead() && underlyingStream == nullptr);
			underlyingStream = &stream;
		}

		inline void Close()
		{
			if (underlyingStream != nullptr && !leaveStreamOpen)
				underlyingStream->Close();
			underlyingStream = nullptr;
		}

		inline bool IsOpen() const { return underlyingStream != nullptr; }

		inline void SetLeaveStreamOpen(bool value) { leaveStreamOpen = value; }
		inline bool GetLeaveStreamOpen() const { return leaveStreamOpen; }

		inline FileAddr GetPosition() const { return underlyingStream->GetPosition() - streamSeekOffset; }
		inline void SetPosition(FileAddr position) { return underlyingStream->Seek(position + streamSeekOffset); }

		inline FileAddr GetLength() const { return underlyingStream->GetLength(); }
		inline bool EndOfFile() const { return underlyingStream->EndOfFile(); }

		inline FileAddr GetStreamSeekOffset() const { return streamSeekOffset; }
		inline void SetStreamSeekOffset(FileAddr value) { streamSeekOffset = value; }

		inline PtrMode GetPointerMode() const { return pointerMode; }
		void SetPointerMode(PtrMode value);

		inline Endianness GetEndianness() const { return endianness; }
		void SetEndianness(Endianness value);

		inline size_t ReadBuffer(void* buffer, size_t size) { return underlyingStream->ReadBuffer(buffer, size); }

		template <typename Func>
		void ReadAt(FileAddr position, const Func func)
		{
			const auto prePos = GetPosition();
			SetPosition(position);
			func(*this);
			SetPosition(prePos);
		}

		template <typename Func>
		void ReadAt(FileAddr position, FileAddr baseAddress, Func func)
		{
			ReadAt((position + baseAddress), func);
		}

		template <typename T, typename Func>
		T ReadAt(FileAddr position, Func func)
		{
			const auto prePos = GetPosition();
			SetPosition(position);
			const T value = func(*this);
			SetPosition(prePos);
			return value;
		}

		std::string ReadStr();
		std::string ReadStrAt(FileAddr position);
		std::string ReadStr(size_t size);

		inline std::string ReadStrPtr() { return ReadStrAt(ReadPtr()); }

		inline FileAddr ReadPtr() { return readPtrFunc(*this); }
		inline size_t ReadSize() { return readSizeFunc(*this); }
		inline bool ReadBool() { return ReadType<bool>(); }
		inline char ReadChar() { return ReadType<char>(); }
		inline uint8_t ReadI8() { return ReadType<int8_t>(); }
		inline uint8_t ReadU8() { return ReadType<uint8_t>(); }
		inline int16_t ReadI16() { return readI16Func(*this); }
		inline uint16_t ReadU16() { return readU16Func(*this); }
		inline int32_t ReadI32() { return readI32Func(*this); }
		inline uint32_t ReadU32() { return readU32Func(*this); }
		inline int64_t ReadI64() { return readI64Func(*this); }
		inline uint64_t ReadU64() { return readU64Func(*this); }
		inline float ReadF32() { return readF32Func(*this); }
		inline double ReadF64() { return readF64Func(*this); }

		inline vec2 ReadV2() { vec2 result; result.x = ReadF32(); result.y = ReadF32(); return result; }
		inline vec3 ReadV3() { vec3 result; result.x = ReadF32(); result.y = ReadF32(); result.z = ReadF32(); return result; }
		inline vec4 ReadV4() { vec4 result; result.x = ReadF32(); result.y = ReadF32(); result.z = ReadF32(); result.w = ReadF32(); return result; }
		inline mat3 ReadMat3() { assert(GetEndianness() == Endianness::Native); return ReadType<mat3>(); }
		inline mat4 ReadMat4() { assert(GetEndianness() == Endianness::Native); return ReadType<mat4>(); }
		inline ivec2 ReadIV2() { ivec2 result; result.x = ReadI32(); result.y = ReadI32(); return result; }
		inline ivec3 ReadIV3() { ivec3 result; result.x = ReadI32(); result.y = ReadI32(); result.z = ReadI32(); return result; }
		inline ivec4 ReadIV4() { ivec4 result; result.x = ReadI32(); result.y = ReadI32(); result.z = ReadI32(); result.w = ReadI32(); return result; }

	protected:
		using ReadPtrFunc_t = FileAddr(BinaryReader&);
		using ReadSizeFunc_t = size_t(BinaryReader&);
		using ReadI16Func_t = int16_t(BinaryReader&);
		using ReadU16Func_t = uint16_t(BinaryReader&);
		using ReadI32Func_t = int32_t(BinaryReader&);
		using ReadU32Func_t = uint32_t(BinaryReader&);
		using ReadI64Func_t = int64_t(BinaryReader&);
		using ReadU64Func_t = uint64_t(BinaryReader&);
		using ReadF32Func_t = float(BinaryReader&);
		using ReadF64Func_t = double(BinaryReader&);

		ReadPtrFunc_t* readPtrFunc = nullptr;
		ReadSizeFunc_t* readSizeFunc = nullptr;
		ReadI16Func_t* readI16Func = nullptr;
		ReadU16Func_t* readU16Func = nullptr;
		ReadI32Func_t* readI32Func = nullptr;
		ReadU32Func_t* readU32Func = nullptr;
		ReadI64Func_t* readI64Func = nullptr;
		ReadU64Func_t* readU64Func = nullptr;
		ReadF32Func_t* readF32Func = nullptr;
		ReadF64Func_t* readF64Func = nullptr;

		PtrMode pointerMode = {};
		Endianness endianness = {};

		bool leaveStreamOpen = false;
		FileAddr streamSeekOffset = {};
		IStream* underlyingStream = nullptr;

	private:
		template <typename T>
		T ReadType()
		{
			T value;
			ReadBuffer(&value, sizeof(value));
			return value;
		}

		static FileAddr ReadPtr32(BinaryReader& reader) { return static_cast<FileAddr>(reader.ReadI32()); }
		static FileAddr ReadPtr64(BinaryReader& reader) { return static_cast<FileAddr>(reader.ReadI64()); }

		static size_t ReadSize32(BinaryReader& reader) { return static_cast<size_t>(reader.ReadU32()); }
		static size_t ReadSize64(BinaryReader& reader) { return static_cast<size_t>(reader.ReadU64()); }

		static int16_t LE_ReadI16(BinaryReader& reader) { return reader.ReadType<int16_t>(); }
		static uint16_t LE_ReadU16(BinaryReader& reader) { return reader.ReadType<uint16_t>(); }
		static int32_t LE_ReadI32(BinaryReader& reader) { return reader.ReadType<int32_t>(); }
		static uint32_t LE_ReadU32(BinaryReader& reader) { return reader.ReadType<uint32_t>(); }
		static int64_t LE_ReadI64(BinaryReader& reader) { return reader.ReadType<int64_t>(); }
		static uint64_t LE_ReadU64(BinaryReader& reader) { return reader.ReadType<uint64_t>(); }
		static float LE_ReadF32(BinaryReader& reader) { return reader.ReadType<float>(); }
		static double LE_ReadF64(BinaryReader& reader) { return reader.ReadType<double>(); }

		static int16_t BE_ReadI16(BinaryReader& reader) { return Utilities::ByteSwapI16(reader.ReadType<int16_t>()); }
		static uint16_t BE_ReadU16(BinaryReader& reader) { return Utilities::ByteSwapU16(reader.ReadType<uint16_t>()); }
		static int32_t BE_ReadI32(BinaryReader& reader) { return Utilities::ByteSwapI32(reader.ReadType<int32_t>()); }
		static uint32_t BE_ReadU32(BinaryReader& reader) { return Utilities::ByteSwapU32(reader.ReadType<uint32_t>()); }
		static int64_t BE_ReadI64(BinaryReader& reader) { return Utilities::ByteSwapI64(reader.ReadType<int64_t>()); }
		static uint64_t BE_ReadU64(BinaryReader& reader) { return Utilities::ByteSwapU64(reader.ReadType<uint64_t>()); }
		static float BE_ReadF32(BinaryReader& reader) { return Utilities::ByteSwapF32(reader.ReadType<float>()); }
		static double BE_ReadF64(BinaryReader& reader) { return Utilities::ByteSwapF64(reader.ReadType<double>()); }
	};
}
