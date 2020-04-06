#pragma once
#include "Types.h"
#include "CoreTypes.h"

namespace Comfy::FileSystem
{
	class BinaryReader;
	class BinaryWriter;

	class IReadable
	{
	public:
		virtual void Load(std::string_view filePath) = 0;
		virtual void Load(std::wstring_view filePath) = 0;
	};

	class IWritable
	{
	public:
		virtual void Save(std::string_view filePath) = 0;
		virtual void Save(std::wstring_view filePath) = 0;
	};

	class IBinaryReadable : public IReadable
	{
	public:
		virtual void Read(BinaryReader& reader) = 0;

		void Load(std::string_view filePath) override;
		void Load(std::wstring_view filePath) override;
	};

	class IBinaryWritable : public IWritable
	{
	public:
		virtual void Write(BinaryWriter& writer) = 0;

		virtual void Save(std::string_view filePath) override;
		virtual void Save(std::wstring_view filePath) override;
	};

	class IBinaryReadWritable : public IBinaryReadable, public IBinaryWritable
	{
	public:
	};

	class IBufferParsable
	{
	public:
		virtual void Parse(const uint8_t* buffer, size_t bufferSize) = 0;
	};
}
