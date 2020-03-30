#pragma once
#include "FileReader.h"

namespace Comfy::FileSystem
{
	const std::vector<std::string> AllFilesFilter = { "All Files (*.*)", "*.*" };

	bool CreateDirectoryFile(const std::wstring& filePath);

	bool IsFilePath(std::string_view filePath);
	bool IsFilePath(std::wstring_view filePath);

	bool IsDirectoryPath(std::string_view directory);
	bool IsDirectoryPath(std::wstring_view directory);

	bool IsPathRelative(const std::string& path);
	bool IsPathRelative(const std::wstring& path);

	bool FileExists(std::string_view filePath);
	bool FileExists(std::wstring_view filePath);

	bool DirectoryExists(std::string_view direction);
	bool DirectoryExists(std::wstring_view direction);

	bool CreateOpenFileDialog(std::wstring& outFilePath, const char* title = nullptr, const char* directory = nullptr, const std::vector<std::string>& filter = AllFilesFilter);
	bool CreateSaveFileDialog(std::wstring& outFilePath, const char* title = nullptr, const char* directory = nullptr, const std::vector<std::string>& filter = AllFilesFilter);

	void OpenWithDefaultProgram(const std::wstring& filePath);
	void OpenInExplorer(const std::wstring& filePath);
	void OpenExplorerProperties(const std::wstring& filePath);

	std::wstring ResolveFileLink(const std::wstring& filePath);

	void FuckUpWindowsPath(std::string& path);
	void FuckUpWindowsPath(std::wstring& path);

	void SanitizePath(std::string& path);
	void SanitizePath(std::wstring& path);

	std::string GetWorkingDirectory();
	std::wstring GetWorkingDirectoryW();

	void SetWorkingDirectory(std::string_view path);
	void SetWorkingDirectoryW(std::wstring_view path);

	std::string Combine(const std::string& pathA, const std::string& pathB);
	std::wstring Combine(const std::wstring& pathA, const std::wstring& pathB);

	std::string_view GetFileName(std::string_view filePath, bool extension = true);
	std::wstring_view GetFileName(std::wstring_view filePath, bool extension = true);

	std::string_view GetDirectory(std::string_view filePath);
	std::wstring_view GetDirectory(std::wstring_view filePath);

	std::string_view GetFileExtension(std::string_view filePath);
	std::wstring_view GetFileExtension(std::wstring_view filePath);

	std::vector<std::string> GetFiles(std::string_view directory);
	std::vector<std::wstring> GetFiles(std::wstring_view directory);

	bool ReadAllBytes(std::string_view filePath, void* buffer, size_t bufferSize);

	template <typename T>
	inline bool ReadAllBytes(std::string_view filePath, T* buffer) { return ReadAllBytes(filePath, buffer, sizeof(T)); };

	bool WriteAllBytes(std::string_view filePath, const void* buffer, size_t bufferSize);

	template <typename T>
	inline bool WriteAllBytes(std::string_view filePath, const T& buffer) { return WriteAllBytes(filePath, &buffer, sizeof(T)); };

	bool WriteAllBytes(std::string_view filePath, const std::vector<uint8_t>& buffer);
	bool WriteAllBytes(std::wstring_view filePath, const std::vector<uint8_t>& buffer);
	
	bool ReadAllLines(std::string_view filePath, std::vector<std::string>* buffer);
	bool ReadAllLines(std::wstring_view filePath, std::vector<std::wstring>* buffer);
}
