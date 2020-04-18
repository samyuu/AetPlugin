#pragma once
#include "Types.h"
#include "CoreTypes.h"
#include "FileSystem/FileInterface.h"

namespace FarcUtil
{
	// TODO: Make a static Comfy FarcArchive or Utilities helper function: void Write(BinaryWriter& writer, const std::vector<FileMarkup>& files, FarcFlags flags)
	void WriteUncompressedFarc(std::string_view filePath, Comfy::FileSystem::IBinaryWritable& writable, std::string_view fileName);
}
