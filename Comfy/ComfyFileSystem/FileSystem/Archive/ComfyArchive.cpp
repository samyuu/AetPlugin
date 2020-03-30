#include "ComfyArchive.h"
#include "FileSystem/Stream/FileStream.h"
#include "Misc/StringHelper.h"

namespace Comfy::FileSystem
{
	ComfyArchive::ComfyArchive()
	{
	}

	ComfyArchive::~ComfyArchive()
	{
		UnMount();
	}

	void ComfyArchive::Mount(const std::string_view filePath)
	{
		if (isMounted)
			return;

		isMounted = true;

		const auto widePath = Utilities::Utf8ToUtf16(filePath);
		dataStream = MakeUnique<FileStream>(widePath);

		if (!dataStream->IsOpen())
			return;

		ParseEntries();
		LinkRemapPointers();

		if (header.Flags.EncryptedStrings)
			DecryptStrings();
	}

	void ComfyArchive::UnMount()
	{
		if (!isMounted)
			return;

		if (dataStream != nullptr)
		{
			dataStream->Close();
			dataStream = nullptr;
		}

		dataBuffer = nullptr;
	}

	const ComfyArchiveHeader& ComfyArchive::GetHeader() const
	{
		assert(isMounted);
		
		return header;
	}

	const ComfyDirectory& ComfyArchive::GetRootDirectory() const
	{
		assert(isMounted && rootDirectory != nullptr);
		
		return *rootDirectory;
	}

	const ComfyEntry* ComfyArchive::FindFile(std::string_view filePath) const
	{
		if (filePath.size() < 1)
			return nullptr;

		size_t lastSeparatorIndex = filePath.size();
		for (int64_t i = static_cast<int64_t>(filePath.size()) - 1; i >= 0; i--)
		{
			if (filePath[i] == DirectorySeparator)
			{
				lastSeparatorIndex = i;
				break;
			}
		}

		if (lastSeparatorIndex == filePath.size())
			return FindFileInDirectory(rootDirectory, filePath);

		std::string_view directory = filePath.substr(0, lastSeparatorIndex);
		const ComfyDirectory* parentDirectory = FindNestedDirectory(rootDirectory, directory);

		if (parentDirectory == nullptr)
			return nullptr;

		lastSeparatorIndex++;
		std::string_view fileName = filePath.substr(lastSeparatorIndex, filePath.size() - lastSeparatorIndex);

		for (size_t i = 0; i < parentDirectory->EntryCount; i++)
		{
			auto& entry = parentDirectory->Entries[i];

			if (entry.Name == fileName)
				return &entry;
		}

		return nullptr;
	}

	const ComfyEntry* ComfyArchive::FindFileInDirectory(const ComfyDirectory* directory, std::string_view fileName) const
	{
		for (size_t i = 0; i < directory->EntryCount; i++)
		{
			auto& entry = directory->Entries[i];

			if (entry.Name == fileName)
				return &entry;
		}

		return nullptr;
	}

	const ComfyDirectory* ComfyArchive::FindDirectory(std::string_view directoryPath) const
	{
		return FindNestedDirectory(rootDirectory, directoryPath);
	}

	bool ComfyArchive::ReadFileIntoBuffer(std::string_view filePath, std::vector<uint8_t>& buffer)
	{
		auto fileEntry = FindFile(filePath);
		if (fileEntry == nullptr)
			return false;

		buffer.resize(fileEntry->Size);
		ReadEntryIntoBuffer(fileEntry, buffer.data());

		return true;
	}

	bool ComfyArchive::ReadEntryIntoBuffer(const ComfyEntry* entry, void* outputBuffer)
	{
		assert(entry != nullptr && isMounted && dataStream != nullptr && dataStream->CanRead());
		if (entry == nullptr || !isMounted || dataStream == nullptr || !dataStream->CanRead())
			return false;
		
		dataStream->Seek(static_cast<FileAddr>(entry->Offset));
		dataStream->ReadBuffer(outputBuffer, entry->Size);

		return true;
	}

	const ComfyDirectory* ComfyArchive::FindNestedDirectory(const ComfyDirectory* parent, std::string_view directory) const
	{
		for (size_t i = 0; i < directory.size(); i++)
		{
			if (directory[i] == DirectorySeparator)
			{
				std::string_view parentDirectory = directory.substr(0, i);
				std::string_view subDirectory = directory.substr(i + 1);

				auto result = FindDirectory(parent, parentDirectory);
				if (result == nullptr)
					return nullptr;

				return FindNestedDirectory(result, subDirectory);
			}
		}

		return FindDirectory(parent, directory);
	}

	const ComfyDirectory* ComfyArchive::FindDirectory(const ComfyDirectory* parent, std::string_view directoryName) const
	{
		for (size_t i = 0; i < parent->SubDirectoryCount; i++)
		{
			auto& subDirectory = parent->SubDirectories[i];

			if (subDirectory.Name == directoryName)
				return &subDirectory;
		}

		return nullptr;
	}

	void ComfyArchive::ParseEntries()
	{
		dataStream->ReadBuffer(&header, sizeof(header));

		// TODO: Extensive validation checking
		assert(header.Magic == ComfyArchive::Magic);

		dataBuffer = MakeUnique<uint8_t[]>(header.DataSize);

		dataStream->Seek(static_cast<FileAddr>(header.DataOffset));
		dataStream->ReadBuffer(dataBuffer.get(), header.DataSize);
	}

	namespace
	{
		template <typename T>
		inline void RemapFileSpacePointer(uint8_t* dataBuffer, T*& fileSpacePointer)
		{
			fileSpacePointer = reinterpret_cast<T*>(&dataBuffer[reinterpret_cast<uintptr_t>(fileSpacePointer)]);
		}

		template <typename T>
		inline void RemapFileSpacePointerIfNotNull(uint8_t* dataBuffer, T*& fileSpacePointer)
		{
			if (fileSpacePointer != nullptr)
				RemapFileSpacePointer(dataBuffer, fileSpacePointer);
		}

		void LinkDirectoryEntry(ComfyDirectory* directory, uint8_t* dataBuffer)
		{
			RemapFileSpacePointerIfNotNull(dataBuffer, directory->Name);
			RemapFileSpacePointerIfNotNull(dataBuffer, directory->Entries);
			RemapFileSpacePointerIfNotNull(dataBuffer, directory->SubDirectories);

			for (size_t i = 0; i < directory->EntryCount; i++)
				RemapFileSpacePointerIfNotNull(dataBuffer, directory->Entries[i].Name);

			for (size_t i = 0; i < directory->SubDirectoryCount; i++)
				LinkDirectoryEntry(&directory->SubDirectories[i], dataBuffer);
		}
	}

	void ComfyArchive::LinkRemapPointers()
	{
		uint8_t* offsetDataBuffer = dataBuffer.get() - header.DataOffset;

		rootDirectory = reinterpret_cast<ComfyDirectory*>(dataBuffer.get());
		LinkDirectoryEntry(rootDirectory, offsetDataBuffer);
	}

	void ComfyArchive::DecryptStrings()
	{
		// TODO:
	}
}
