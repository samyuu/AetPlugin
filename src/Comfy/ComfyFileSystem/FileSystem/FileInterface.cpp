#include "FileInterface.h"
#include "Stream/MemoryStream.h"
#include "Stream/FileStream.h"
#include "BinaryReader.h"
#include "BinaryWriter.h"
#include "Misc/StringHelper.h"

namespace Comfy::FileSystem
{
	namespace
	{
		void IBinaryReadableLoadBase(IBinaryReadable& readable, IStream& stream)
		{
			BinaryReader reader(stream);
			readable.Read(reader);
		}

		void IBinaryWritableSaveBase(IBinaryWritable& writable, IStream& stream)
		{
			BinaryWriter writer(stream);
			writable.Write(writer);
		}
	}

	void IBinaryReadable::Load(std::string_view filePath)
	{
		MemoryStream stream(filePath);
		IBinaryReadableLoadBase(*this, stream);
	}

	void IBinaryReadable::Load(std::wstring_view filePath)
	{
		MemoryStream stream(filePath);
		IBinaryReadableLoadBase(*this, stream);
	}

	void IBinaryWritable::Save(std::string_view filePath)
	{
		FileStream stream;

		std::wstring widePath = Utf8ToUtf16(filePath);
		stream.CreateReadWrite(widePath);
		IBinaryWritableSaveBase(*this, stream);
	}

	void IBinaryWritable::Save(std::wstring_view filePath)
	{
		FileStream stream;
		stream.CreateReadWrite(filePath);
		IBinaryWritableSaveBase(*this, stream);
	}
}
