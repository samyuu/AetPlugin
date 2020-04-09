#include "FileHelper.h"
#include "FileReader.h"
#include "Misc/StringHelper.h"
#include <filesystem>
#include <fstream>
#include <shlwapi.h>
#include <shobjidl.h>
#include <assert.h>

#include "FileHelperInternal.h"

namespace Comfy::FileSystem
{
	namespace
	{
		template <typename T>
		auto FileAttributesFromStringViewInternal(std::basic_string_view<T> path)
		{
			auto pathBuffer = NullTerminatedPathBufferInternal(path);

			if constexpr (std::is_same<T, char>::value)
				return ::GetFileAttributesA(pathBuffer.data());
			else
				return ::GetFileAttributesW(pathBuffer.data());
		}

		template <typename T>
		auto IsFilePathInternal(std::basic_string_view<T> filePath)
		{
			for (int i = static_cast<int>(filePath.size()) - 1; i >= 0; i--)
			{
				auto character = filePath[i];

				if (character == '/' || character == '\\')
					break;
				else if (character == '.')
					return true;
			}
			return false;
		}

		std::wstring FilterVectorToStringInternal(const std::vector<std::string>& filterVector)
		{
			assert(filterVector.size() % 2 == 0);

			std::wstring filterString;
			for (size_t i = 0; i + 1 < filterVector.size(); i += 2)
			{
				filterString += Utf8ToUtf16(filterVector[i]);
				filterString += L'\0';

				filterString += Utf8ToUtf16(filterVector[i + 1]);
				filterString += L'\0';
			}
			filterString += L'\0';

			return filterString;
		}
	}

	bool CreateDirectoryFile(const std::wstring& filePath)
	{
		return ::CreateDirectoryW(filePath.c_str(), NULL);
	}

	bool IsFilePath(std::string_view filePath)
	{
		return IsFilePathInternal(filePath);
	}

	bool IsFilePath(std::wstring_view filePath)
	{
		return IsFilePathInternal(filePath);
	}

	bool IsDirectoryPath(std::string_view directory)
	{
		return !IsFilePath(directory);
	}

	bool IsDirectoryPath(std::wstring_view directory)
	{
		return !IsFilePath(directory);
	}

	bool IsPathRelative(const std::string& path)
	{
		return ::PathIsRelativeA(path.c_str());
	}

	bool IsPathRelative(const std::wstring& path)
	{
		return ::PathIsRelativeW(path.c_str());
	}

	bool FileExists(std::string_view filePath)
	{
		auto attributes = FileAttributesFromStringViewInternal(filePath);
		return (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY));
	}

	bool FileExists(std::wstring_view filePath)
	{
		auto attributes = FileAttributesFromStringViewInternal(filePath);
		return (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY));
	}

	bool DirectoryExists(std::string_view direction)
	{
		auto attributes = FileAttributesFromStringViewInternal(direction);
		return (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY));
	}

	bool DirectoryExists(std::wstring_view direction)
	{
		auto attributes = FileAttributesFromStringViewInternal(direction);
		return (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY));
	}

	bool CreateOpenFileDialog(std::wstring& outFilePath, const char* title, const char* directory, const std::vector<std::string>& filter)
	{
		std::wstring currentDirectory = GetWorkingDirectoryW();
		wchar_t filePathBuffer[MAX_PATH]; filePathBuffer[0] = '\0';

		assert(!filter.empty());
		std::wstring filterString = FilterVectorToStringInternal(filter);
		std::wstring directoryString = (directory != nullptr) ? Utf8ToUtf16(directory) : L"";
		std::wstring titleString = (title != nullptr) ? Utf8ToUtf16(title) : L"";

		OPENFILENAMEW openFileName = {};
		openFileName.lStructSize = sizeof(OPENFILENAMEW);
		openFileName.hwndOwner = NULL;
		openFileName.lpstrFilter = filterString.c_str();
		openFileName.lpstrFile = filePathBuffer;
		openFileName.nMaxFile = MAX_PATH;
		openFileName.lpstrInitialDir = (directory != nullptr) ? directoryString.c_str() : nullptr;
		openFileName.lpstrTitle = (title != nullptr) ? titleString.c_str() : nullptr;
		openFileName.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
		openFileName.lpstrDefExt = L"";

		bool fileSelected = ::GetOpenFileNameW(&openFileName);
		if (fileSelected)
			outFilePath = std::wstring(filePathBuffer);

		SetWorkingDirectoryW(currentDirectory);
		return fileSelected;
	}

	bool CreateSaveFileDialog(std::wstring& outFilePath, const char* title, const char* directory, const std::vector<std::string>& filter)
	{
		std::wstring currentDirectory = GetWorkingDirectoryW();
		wchar_t filePathBuffer[MAX_PATH] = {};

		assert(!filter.empty());
		std::wstring filterString = FilterVectorToStringInternal(filter);
		std::wstring directoryString = (directory != nullptr) ? Utf8ToUtf16(directory) : L"";
		std::wstring titleString = (title != nullptr) ? Utf8ToUtf16(title) : L"";

		OPENFILENAMEW openFileName = {};
		openFileName.lStructSize = sizeof(OPENFILENAMEW);
		openFileName.hwndOwner = NULL;
		openFileName.lpstrFilter = filterString.c_str();
		openFileName.lpstrFile = filePathBuffer;
		openFileName.nMaxFile = MAX_PATH;
		openFileName.lpstrInitialDir = (directory != nullptr) ? directoryString.c_str() : nullptr;
		openFileName.lpstrTitle = (title != nullptr) ? titleString.c_str() : nullptr;
		openFileName.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;

		bool fileSelected = ::GetSaveFileNameW(&openFileName);
		if (fileSelected)
			outFilePath = std::wstring(filePathBuffer);

		SetWorkingDirectoryW(currentDirectory);
		return fileSelected;
	}

	void OpenWithDefaultProgram(const std::wstring& filePath)
	{
		::ShellExecuteW(NULL, L"open", filePath.c_str(), NULL, NULL, SW_SHOW);
	}

	void OpenInExplorer(const std::wstring& filePath)
	{
		if (IsPathRelative(filePath))
		{
			std::wstring currentDirectory = GetWorkingDirectoryW();
			currentDirectory.reserve(currentDirectory.size() + filePath.size() + 2);
			currentDirectory += L"/";
			currentDirectory += filePath;

			::ShellExecuteW(NULL, L"open", currentDirectory.c_str(), NULL, NULL, SW_SHOWDEFAULT);
		}
		else
		{
			::ShellExecuteW(NULL, L"open", filePath.c_str(), NULL, NULL, SW_SHOWDEFAULT);
		}
	}

	void OpenExplorerProperties(const std::wstring& filePath)
	{
		SHELLEXECUTEINFOW info = { };

		info.cbSize = sizeof info;
		info.lpFile = filePath.c_str();
		info.nShow = SW_SHOW;
		info.fMask = SEE_MASK_INVOKEIDLIST;
		info.lpVerb = L"properties";

		::ShellExecuteExW(&info);
	}

	std::wstring ResolveFileLink(const std::wstring& filePath)
	{
#if 0
		const struct RAII_CoInitialize
		{
			RAII_CoInitialize() { ::CoInitialize(NULL); };
			~RAII_CoInitialize() { ::CoUninitialize(); };
		} coInitialize;
#endif

		std::wstring resolvedPath;

		IShellLinkW* shellLink;
		if (SUCCEEDED(::CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID*)&shellLink)))
		{
			IPersistFile* persistFile;
			if (SUCCEEDED(shellLink->QueryInterface(IID_IPersistFile, (void**)&persistFile)))
			{
				if (SUCCEEDED(persistFile->Load(filePath.c_str(), STGM_READ)))
				{
					if (SUCCEEDED(shellLink->Resolve(NULL, 0)))
					{
						WCHAR pathBuffer[MAX_PATH];
						WIN32_FIND_DATAW findData;

						if (SUCCEEDED(shellLink->GetPath(pathBuffer, MAX_PATH, &findData, SLGP_SHORTPATH)))
							resolvedPath = pathBuffer;
					}
				}
				persistFile->Release();
			}
			shellLink->Release();
		}

		return resolvedPath;
	}

	void FuckUpWindowsPath(std::string& path)
	{
		std::replace(path.begin(), path.end(), '/', '\\');
	}

	void FuckUpWindowsPath(std::wstring& path)
	{
		std::replace(path.begin(), path.end(), L'/', L'\\');
	}

	void SanitizePath(std::string& path)
	{
		std::replace(path.begin(), path.end(), '\\', '/');
	}

	void SanitizePath(std::wstring& path)
	{
		std::replace(path.begin(), path.end(), L'\\', L'/');
	}

	std::string GetWorkingDirectory()
	{
		char buffer[MAX_PATH];
		::GetCurrentDirectoryA(MAX_PATH, buffer);

		return std::string(buffer);
	}

	std::wstring GetWorkingDirectoryW()
	{
		wchar_t buffer[MAX_PATH];
		::GetCurrentDirectoryW(MAX_PATH, buffer);

		return std::wstring(buffer);
	}

	void SetWorkingDirectory(std::string_view path)
	{
		auto pathBuffer = NullTerminatedPathBufferInternal(path);
		::SetCurrentDirectoryA(pathBuffer.data());
	}

	void SetWorkingDirectoryW(std::wstring_view path)
	{
		auto pathBuffer = NullTerminatedPathBufferInternal(path);
		::SetCurrentDirectoryW(pathBuffer.data());
	}

	std::string Combine(const std::string& pathA, const std::string& pathB)
	{
		if (pathA.size() > 0 && pathA.back() == '/')
			return pathA.substr(0, pathA.length() - 1) + '/' + pathB;

		return pathA + '/' + pathB;
	}

	std::wstring Combine(const std::wstring& pathA, const std::wstring& pathB)
	{
		if (pathA.size() > 0 && pathA.back() == L'/')
			return pathA.substr(0, pathA.length() - 1) + L'/' + pathB;

		return pathA + L'/' + pathB;
	}

	std::string_view GetFileName(std::string_view filePath, bool extension)
	{
		const auto last = filePath.find_last_of("/\\");
		return (last == std::string::npos) ? filePath.substr(0, 0) : (extension ? filePath.substr(last + 1) : filePath.substr(last + 1, filePath.length() - last - 1 - GetFileExtension(filePath).length()));
	}

	std::wstring_view GetFileName(std::wstring_view filePath, bool extension)
	{
		const auto last = filePath.find_last_of(L"/\\");
		return (last == std::string::npos) ? filePath.substr(0, 0) : (extension ? filePath.substr(last + 1) : filePath.substr(last + 1, filePath.length() - last - 1 - GetFileExtension(filePath).length()));
	}

	std::string_view GetDirectory(std::string_view filePath)
	{
		return filePath.substr(0, filePath.find_last_of("/\\"));
	}

	std::wstring_view GetDirectory(std::wstring_view filePath)
	{
		return filePath.substr(0, filePath.find_last_of(L"/\\"));
	}

	std::string_view GetFileExtension(std::string_view filePath)
	{
		const auto index = filePath.find_last_of(".");
		return (index != std::string_view::npos) ? filePath.substr(index) : "";
	}

	std::wstring_view GetFileExtension(std::wstring_view filePath)
	{
		const auto index = filePath.find_last_of(L".");
		return (index != std::string_view::npos) ? filePath.substr(index) : L"";
	}

	std::vector<std::string> GetFiles(std::string_view directory)
	{
		std::vector<std::string> files;
		for (const auto& file : std::filesystem::directory_iterator(directory))
			files.push_back(file.path().u8string());
		return files;
	}

	std::vector<std::wstring> GetFiles(std::wstring_view directory)
	{
		std::vector<std::wstring> files;
		for (const auto& file : std::filesystem::directory_iterator(directory))
			files.push_back(file.path().wstring());
		return files;
	}

	bool ReadAllBytes(std::string_view filePath, void* buffer, size_t bufferSize)
	{
		HANDLE fileHandle = CreateFileHandleInternal(filePath, true);
		int error = ::GetLastError();

		if (fileHandle == nullptr)
			return false;

		DWORD fileSize = ::GetFileSize(fileHandle, nullptr);
		DWORD bytesRead;

		DWORD bytesToRead = min(fileSize, static_cast<DWORD>(bufferSize));

		::ReadFile(fileHandle, buffer, bytesToRead, &bytesRead, nullptr);
		CloseFileHandleInternal(fileHandle);
		
		return (bytesRead == bytesToRead);
	}

	bool WriteAllBytes(std::string_view filePath, const void* buffer, size_t bufferSize)
	{
		HANDLE fileHandle = CreateFileHandleInternal(filePath, false);
		int error = ::GetLastError();

		bool result = WriteAllBytesInternal(fileHandle, buffer, bufferSize);
		CloseFileHandleInternal(fileHandle);

		return result;
	}

	bool WriteAllBytes(std::string_view filePath, const std::vector<uint8_t>& buffer)
	{
		HANDLE fileHandle = CreateFileHandleInternal(filePath, false);
		int error = ::GetLastError();

		bool result = WriteAllBytesInternal(fileHandle, buffer.data(), buffer.size());
		CloseFileHandleInternal(fileHandle);

		return result;
	}

	bool WriteAllBytes(std::wstring_view filePath, const std::vector<uint8_t>& buffer)
	{
		HANDLE fileHandle = CreateFileHandleInternal(filePath, false);
		int error = ::GetLastError();

		bool result = WriteAllBytesInternal(fileHandle, buffer.data(), buffer.size());
		CloseFileHandleInternal(fileHandle);

		return result;
	}

	bool ReadAllLines(std::string_view filePath, std::vector<std::string>* buffer)
	{
		std::ifstream file(filePath);
		while (true)
		{
			buffer->emplace_back();
			if (!std::getline(file, buffer->back()))
				break;
		}
		return true;
	}

	bool ReadAllLines(std::wstring_view filePath, std::vector<std::wstring>* buffer)
	{
		std::wfstream file(filePath);
		while (true)
		{
			buffer->emplace_back();
			if (!std::getline(file, buffer->back()))
				break;
		}
		return true;
	}
}
