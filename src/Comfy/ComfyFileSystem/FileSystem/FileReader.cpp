#include "FileReader.h"
#include "FileHelperInternal.h"
#include "FileHelper.h"
#include <assert.h>

namespace Comfy::FileSystem
{
	void* FileReader::CreateFileHandle(std::string_view filePath, bool read)
	{
		assert(FileSystem::FileExists(filePath));
		return CreateFileHandleInternal(filePath, read);
	}

	void* FileReader::CreateFileHandle(std::wstring_view filePath, bool read)
	{
		assert(FileSystem::FileExists(filePath));
		return CreateFileHandleInternal(filePath, read);
	}

	void FileReader::CloseFileHandle(void* fileHandle)
	{
		CloseFileHandleInternal(fileHandle);
	}

	size_t FileReader::GetFileSize(void* fileHandle)
	{
		LARGE_INTEGER fileSizeLarge = {};
		bool success = ::GetFileSizeEx(fileHandle, &fileSizeLarge);

		return static_cast<size_t>(fileSizeLarge.QuadPart);
	}

	size_t FileReader::ReadFile(void* fileHandle, void* outputData, size_t dataSize)
	{
		DWORD bytesRead;
		bool success = ::ReadFile(fileHandle, outputData, static_cast<DWORD>(dataSize), &bytesRead, nullptr);

		return static_cast<size_t>(bytesRead);
	}
}
