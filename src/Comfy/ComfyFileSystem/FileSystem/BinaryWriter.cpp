#include "BinaryWriter.h"
#include <assert.h>

namespace Comfy::FileSystem
{
	void BinaryWriter::SetPointerMode(PtrMode mode)
	{
		pointerMode = mode;

		switch (pointerMode)
		{
		case PtrMode::Mode32Bit:
			writePtrFunc = &WritePtr32;
			return;

		case PtrMode::Mode64Bit:
			writePtrFunc = &WritePtr64;
			return;

		default:
			assert(false);
			return;
		}
	}

	void BinaryWriter::WriteStr(std::string_view value)
	{
		// NOTE: Manually write null terminator
		WriteBuffer(value.data(), value.size());
		WriteChar('\0');
	}

	void BinaryWriter::WriteStrPtr(std::string_view value, int32_t alignment)
	{
		stringPointerPool.push_back({ GetPosition(), value, alignment });
		WritePtr(FileAddr::NullPtr);
	}

	void BinaryWriter::WritePtr(const std::function<void(BinaryWriter&)>& func)
	{
		pointerPool.push_back({ GetPosition(), func });
		WritePtr(FileAddr::NullPtr);
	}

	void BinaryWriter::WriteDelayedPtr(const std::function<void(BinaryWriter&)>& func)
	{
		delayedWritePool.push_back({ GetPosition(), func });
		WritePtr(FileAddr::NullPtr);
	}

	void BinaryWriter::WritePadding(size_t size, uint32_t paddingValue)
	{
		if (size < 0)
			return;

		constexpr size_t maxSize = 32;
		assert(size <= maxSize);
		std::array<uint8_t, maxSize> paddingValues;

		std::memset(paddingValues.data(), paddingValue, size);
		WriteBuffer(paddingValues.data(), size);
	}

	void BinaryWriter::WriteAlignmentPadding(int32_t alignment, uint32_t paddingValue)
	{
		constexpr bool forceExtraPadding = false;
		constexpr int32_t maxAlignment = 32;

		assert(alignment <= maxAlignment);
		std::array<uint8_t, maxAlignment> paddingValues;

		const int32_t value = static_cast<int32_t>(GetPosition());
		int32_t paddingSize = ((value + (alignment - 1)) & ~(alignment - 1)) - value;

		if (forceExtraPadding && paddingSize <= 0)
			paddingSize = alignment;

		if (paddingSize > 0)
		{
			std::memset(paddingValues.data(), paddingValue, paddingSize);
			WriteBuffer(paddingValues.data(), paddingSize);
		}
	}

	void BinaryWriter::FlushStringPointerPool()
	{
		for (auto& value : stringPointerPool)
		{
			const auto originalStringOffset = GetPosition();
			auto stringOffset = originalStringOffset;

			bool pooledStringFound = false;
			if (poolStrings)
			{
				if (auto pooledString = writtenStringPool.find(std::string(value.String)); pooledString != writtenStringPool.end())
				{
					stringOffset = pooledString->second;
					pooledStringFound = true;
				}
			}

			SetPosition(value.ReturnAddress);
			WritePtr(stringOffset);

			if (!pooledStringFound)
			{
				SetPosition(stringOffset);
				WriteStr(value.String);

				if (value.Alignment > 0)
					WriteAlignmentPadding(value.Alignment);
			}
			else
			{
				SetPosition(originalStringOffset);
			}

			if (poolStrings && !pooledStringFound)
				writtenStringPool.insert(std::make_pair(value.String, stringOffset));
		}

		if (poolStrings)
			writtenStringPool.clear();

		stringPointerPool.clear();
	}

	void BinaryWriter::FlushPointerPool()
	{
		for (auto& value : pointerPool)
		{
			const auto offset = GetPosition();

			SetPosition(value.ReturnAddress);
			WritePtr(offset);

			SetPosition(offset);
			value.Function(*this);
		}

		pointerPool.clear();
	}

	void BinaryWriter::FlushDelayedWritePool()
	{
		for (auto& value : delayedWritePool)
		{
			const auto offset = GetPosition();

			SetPosition(value.ReturnAddress);
			value.Function(*this);

			SetPosition(offset);
		}

		delayedWritePool.clear();
	}
}
