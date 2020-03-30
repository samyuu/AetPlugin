#include "FileLoader.h"
#include "FileHelper.h"
#include "BinaryReader.h"
#include "Stream/MemoryStream.h"
#include "Misc/StringHelper.h"
#include "Core/Logger.h"
#include <assert.h>

namespace Comfy::FileSystem
{
	FileLoader::FileLoader()
	{
	}

	FileLoader::FileLoader(const std::string& filePath)
	{
		SetFilePath(filePath);
	}

	FileLoader::~FileLoader()
	{
		if (loaderThread != nullptr && loaderThread->joinable())
			loaderThread->join();
	}

	const std::string& FileLoader::GetFilePath() const
	{
		return filePath;
	}

	void FileLoader::SetFilePath(const std::string& value)
	{
		assert(!isLoaded);
		assert(!threadRunning && loaderThread == nullptr);
	
		filePath = value;
	}

	void FileLoader::LoadSync()
	{
		assert(!isLoaded);
		assert(filePath.size() > 0);

		CheckFileLocation();
		if (!GetFileFound())
			return;

		if (FileReader::ReadEntireFile(Utf8ToUtf16(filePath), &fileContent))
		{
			SetIsLoaded(true);
		}
	}

	void FileLoader::LoadAsync()
	{
		assert(!isLoaded);
		assert(!threadRunning);
		assert(filePath.size() > 0);

		CheckFileLocation();
		if (!GetFileFound())
			return;

		loaderThread = MakeUnique<std::thread>([this]()
		{
			threadRunning = true;
			if (FileReader::ReadEntireFile(Utf8ToUtf16(filePath), &fileContent))
			{
				SetIsLoaded(true);
			}
			threadRunning = false;
		});
	}

	void FileLoader::CheckStartLoadAsync()
	{
		if (!isLoaded && loaderThread == nullptr && !threadRunning)
			LoadAsync();
	}

	bool FileLoader::GetIsLoaded() const
	{
		return isLoaded;
	}

	bool FileLoader::GetFileFound() const
	{
		return fileFound;
	}

	bool FileLoader::GetIsLoading() const
	{
		return fileFound && threadRunning && !isLoaded;
	}

	const std::vector<uint8_t>& FileLoader::GetFileContent() const
	{
		assert(fileFound && isLoaded);
		return fileContent;
	}

	void FileLoader::Read(IBinaryReadable* readable) const
	{
		assert(readable != nullptr);
		assert(fileFound && isLoaded);

		MemoryStream stream(const_cast<FileLoader*>(this)->fileContent);
		BinaryReader reader(stream);
		
		readable->Read(reader);
	}

	void FileLoader::Parse(IBufferParsable* parsable) const
	{
		assert(parsable != nullptr);
		assert(fileFound && isLoaded);

		parsable->Parse(fileContent.data(), fileContent.size());
	}

	void FileLoader::FreeData()
	{
		assert(fileFound && isLoaded);
		fileContent.clear();
		fileContent.shrink_to_fit();
	}

	void FileLoader::CheckFileLocation()
	{
		SetFileFound(FileExists(Utf8ToUtf16(filePath)));

		if (!GetFileFound())
		{
			Logger::LogErrorLine(__FUNCTION__"(): %s not found", filePath.c_str());
		}
	}

	void FileLoader::SetIsLoaded(bool value)
	{
		isLoaded = true;
	}

	void FileLoader::SetFileFound(bool value)
	{
		fileFound = value;
	}
}
