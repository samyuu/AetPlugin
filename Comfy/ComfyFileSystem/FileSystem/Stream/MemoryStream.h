#pragma once
#include "Stream.h"
#include "CoreTypes.h"

namespace Comfy::FileSystem
{
	class MemoryStream : public StreamBase
	{
	public:
		MemoryStream();
		MemoryStream(std::string_view filePath);
		MemoryStream(std::wstring_view filePath);
		MemoryStream(IStream& stream);
		MemoryStream(std::vector<uint8_t>& source);
		~MemoryStream();

		void Seek(FileAddr position) override;
		FileAddr GetPosition() const override;
		FileAddr GetLength() const override;

		bool IsOpen() const override;
		bool CanRead() const override;
		bool CanWrite() const override;

		size_t ReadBuffer(void* buffer, size_t size) override;
		size_t WriteBuffer(const void* buffer, size_t size) override;

		void FromStreamSource(std::vector<uint8_t>& source);
		void FromFile(std::string_view filePath);
		void FromFile(std::wstring_view filePath);
		void FromStream(IStream& stream);
		void Close() override;

	protected:
		bool canRead = false;

		FileAddr position = {};
		FileAddr dataSize = {};

		std::vector<uint8_t>* dataSource;
		std::vector<uint8_t> dataVector;
	};
}
