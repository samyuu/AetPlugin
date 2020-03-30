#pragma once
#include "CoreTypes.h"

namespace Comfy::FileSystem
{
	class FileReader
	{
	private:
		FileReader() = delete;

		template<typename TPath, typename TData>
		static inline bool ReadEntireFileInternal(TPath filePath, std::vector<TData>* buffer)
		{
			auto fileHandle = CreateFileHandle(filePath, true);
			bool validHandle = reinterpret_cast<int64_t>(fileHandle) > 0;

			if (validHandle)
			{
				size_t fileSize = GetFileSize(fileHandle);
				buffer->resize((fileSize + sizeof(TData) - 1) / sizeof(TData));
				ReadFile(fileHandle, buffer->data(), fileSize);
				CloseFileHandle(fileHandle);
			}

			return validHandle;
		}

	public:
		template<typename T>
		static inline bool ReadEntireFile(std::string_view filePath, std::vector<T>* buffer)
		{
			return ReadEntireFileInternal<std::string_view, T>(filePath, buffer);
		}

		template<typename T>
		static inline bool ReadEntireFile(std::wstring_view filePath, std::vector<T>* buffer)
		{
			return ReadEntireFileInternal<std::wstring_view, T>(filePath, buffer);
		}

	private:
		static void* CreateFileHandle(std::string_view filePath, bool read);
		static void* CreateFileHandle(std::wstring_view filePath, bool read);
		static void CloseFileHandle(void* fileHandle);

		static size_t GetFileSize(void* fileHandle);
		static size_t ReadFile(void* fileHandle, void* outputData, size_t dataSize);
	};
}
