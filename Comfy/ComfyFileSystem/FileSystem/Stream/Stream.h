#pragma once
#include "Types.h"

namespace Comfy::FileSystem
{
	class IStream
	{
	public:
		virtual ~IStream() = default;

		virtual bool EndOfFile() const = 0;
		virtual void Skip(FileAddr amount) = 0;
		virtual void Rewind() = 0;

		virtual void Seek(FileAddr position) = 0;
		virtual FileAddr RemainingBytes() const = 0;
		virtual FileAddr GetPosition() const = 0;
		virtual FileAddr GetLength() const = 0;

		virtual bool IsOpen() const = 0;
		virtual bool CanRead() const = 0;
		virtual bool CanWrite() const = 0;

		virtual size_t ReadBuffer(void* buffer, size_t size) = 0;
		virtual size_t WriteBuffer(const void* buffer, size_t size) = 0;

		virtual void Close() = 0;
	};

	class StreamBase : public IStream, NonCopyable
	{
	public:
		bool EndOfFile() const override;
		void Skip(FileAddr amount) override;
		void Rewind() override;

		FileAddr RemainingBytes() const;
	};
}
