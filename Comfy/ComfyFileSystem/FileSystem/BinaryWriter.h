#pragma once
#include "CoreTypes.h"
#include "Stream/Stream.h"
#include "BinaryMode.h"
#include <list>
#include <map>
#include <functional>

namespace Comfy::FileSystem
{
	class BinaryWriter : NonCopyable
	{
	public:
		static constexpr uint32_t PaddingValue = 0xCCCCCCCC;

	public:
		BinaryWriter()
		{
			SetPointerMode(PtrMode::Mode32Bit);
		}

		BinaryWriter(IStream& stream) : BinaryWriter()
		{
			OpenStream(stream);
		}

		~BinaryWriter()
		{
			Close();
		}

		inline void OpenStream(IStream& stream)
		{
			assert(stream.CanWrite() && underlyingStream == nullptr);
			underlyingStream = &stream;
		}

		inline void Close()
		{
			if (underlyingStream != nullptr && !leaveStreamOpen)
				underlyingStream->Close();
			underlyingStream = nullptr;
		}

		inline bool IsOpen() const { return underlyingStream != nullptr; }
		inline bool GetLeaveStreamOpen() const { return leaveStreamOpen; }

		inline FileAddr GetPosition() const { return underlyingStream->GetPosition(); }
		inline void SetPosition(FileAddr position) { return underlyingStream->Seek(position); }

		inline void SkipPosition(FileAddr increment) { return underlyingStream->Seek(GetPosition() + increment); }

		inline FileAddr GetLength() const { return underlyingStream->GetLength(); }
		inline bool EndOfFile() const { return underlyingStream->EndOfFile(); }

		inline PtrMode GetPointerMode() const { return pointerMode; }
		void SetPointerMode(PtrMode mode);

		inline size_t WriteBuffer(const void* buffer, size_t size) { return underlyingStream->WriteBuffer(buffer, size); }

		template <typename T> 
		void WriteType(T value) { WriteBuffer(&value, sizeof(T)); }

		void WriteStr(std::string_view value);
		void WriteStrPtr(std::string_view value, int32_t alignment = 0);

		inline void WritePtr(FileAddr value) { writePtrFunc(*this, value); };
		inline void WritePtr(nullptr_t) = delete;

		void WritePtr(const std::function<void(BinaryWriter&)>& func);
		void WriteDelayedPtr(const std::function<void(BinaryWriter&)>& func);

		void WritePadding(size_t size, uint32_t paddingValue = PaddingValue);
		void WriteAlignmentPadding(int32_t alignment, uint32_t paddingValue = PaddingValue);

		void FlushStringPointerPool();
		void FlushPointerPool();
		void FlushDelayedWritePool();

		inline void WriteBool(bool value) { return WriteType<bool>(value); }
		inline void WriteChar(char value) { return WriteType<char>(value); }
		inline void WriteI8(uint8_t value) { return WriteType<int8_t>(value); }
		inline void WriteU8(uint8_t value) { return WriteType<uint8_t>(value); }
		inline void WriteI16(int16_t value) { return WriteType<int16_t>(value); }
		inline void WriteU16(uint16_t value) { return WriteType<uint16_t>(value); }
		inline void WriteI32(int32_t value) { return WriteType<int32_t>(value); }
		inline void WriteU32(uint32_t value) { return WriteType<uint32_t>(value); }
		inline void WriteI64(int64_t value) { return WriteType<int64_t>(value); }
		inline void WriteU64(uint64_t value) { return WriteType<uint64_t>(value); }
		inline void WriteF32(float value) { return WriteType<float>(value); }
		inline void WriteF64(double value) { return WriteType<double>(value); }

	protected:
		using WritePtrFunc_t = void(BinaryWriter&, FileAddr);
		WritePtrFunc_t* writePtrFunc = nullptr;

		PtrMode pointerMode = {};
		bool leaveStreamOpen = false;
		bool poolStrings = true;
		IStream* underlyingStream = nullptr;

		struct StringPointerEntry
		{
			FileAddr ReturnAddress;
			std::string_view String;
			int32_t Alignment;
		};

		struct FunctionPointerEntry
		{
			FileAddr ReturnAddress;
			const std::function<void(BinaryWriter&)> Function;
		};

		struct DelayedWriteEntry
		{
			FileAddr ReturnAddress;
			const std::function<void(BinaryWriter&)> Function;
		};

		std::unordered_map<std::string, FileAddr> writtenStringPool;
		std::vector<StringPointerEntry> stringPointerPool;
		std::list<FunctionPointerEntry> pointerPool;
		std::vector<DelayedWriteEntry> delayedWritePool;

	private:
		static void WritePtr32(BinaryWriter& writer, FileAddr value) { writer.WriteI32(static_cast<int32_t>(value)); }
		static void WritePtr64(BinaryWriter& writer, FileAddr value) { writer.WriteI64(static_cast<int64_t>(value)); }
	};
}
