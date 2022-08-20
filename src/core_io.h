#pragma once
#include "core_types.h"
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <functional>

namespace Path
{
	constexpr char ExtensionSeparator = '.';
	constexpr char DirectorySeparator = '/', DirectorySeparatorWin32 = '\\';
	constexpr cstr DirectorySeparators = "/\\";

	constexpr char InvalidPathCharacters[] = { '\"', '<', '>', '|', '\0', };
	constexpr char InvalidFileNameCharacters[] = { '\"', '<', '>', '|', ':', '*', '?', '\\', '/', '\0', };

	constexpr b8 IsValidPathChar(char charToCheck) { for (char invalid : InvalidFileNameCharacters) { if (charToCheck == invalid) return false; } return true; }

	std::string_view GetExtension(std::string_view filePath);
	std::string_view TrimExtension(std::string_view filePath);

	// NOTE: Case insensitive
	b8 HasExtension(std::string_view filePath, std::string_view extension);
	// NOTE: Case insensitive, packed extensions to be separated by a semicolon (Example: ".wav;.flac;.ogg;.mp3")
	b8 HasAnyExtension(std::string_view filePath, std::string_view packedExtensions);

	std::string_view GetFileName(std::string_view filePath, b8 includeExtension = true);
	std::string_view GetDirectoryName(std::string_view filePath);

	b8 IsRelative(std::string_view filePath);
	b8 IsDirectory(std::string_view filePath);

	std::string TryMakeAbsolute(std::string_view relativePath, std::string_view baseFileOrDirectory);
	std::string TryMakeRelative(std::string_view absolutePath, std::string_view baseFileOrDirectory);

	// NOTE: Replace '\\' -> '/' etc.
	std::string CopyAndNormalize(std::string_view filePath);
	std::string& NormalizeInPlace(std::string& inOutFilePath);
	// NOTE: Replace '/' -> '\\' etc.
	std::string CopyAndNormalizeWin32(std::string_view filePath);
	std::string& NormalizeInPlaceWin32(std::string& inOutFilePath);

	std::string Combine(std::string pathA, std::string_view pathB);
	std::string Combine(std::string_view pathA, std::string_view pathB);
}

namespace File
{
	struct UniqueFileContent
	{
		std::unique_ptr<u8[]> Content;
		size_t Size;
	};

	UniqueFileContent ReadAllBytes(std::string_view filePath);
	b8 WriteAllBytes(std::string_view filePath, const void* fileContent, size_t fileSize);
	b8 WriteAllBytes(std::string_view filePath, const UniqueFileContent& uniqueFileContent);
	b8 WriteAllBytes(std::string_view filePath, const std::string_view textFileContent);

	b8 Exists(std::string_view filePath);
	b8 Copy(std::string_view source, std::string_view destination, b8 overwriteExisting = false);
}

namespace Directory
{
	b8 Create(std::string_view directoryPath);
	b8 Exists(std::string_view directoryPath);

	std::string GetExecutablePath();
	std::string GetExecutableDirectory();
	std::string GetWorkingDirectory();
	void SetWorkingDirectory(std::string_view directoryPath);

	b8 ForEachFile(std::string_view directoryPath, const std::function<ControlFlow(std::string_view)>& forEachFileFunc);
}

namespace Shell
{
	enum class MessageBoxButtons : u8 { AbortRetryIgnore, CancelTryContinue, OK, OKCancel, RetryCancel, YesNo, YesNoCancel };
	enum class MessageBoxIcon : u8 { Asterisk, Error, Exclamation, Hand, Information, None, Question, Stop, Warning };
	enum class MessageBoxResult : u8 { Abort, Cancel, Continue, Ignore, No, None, OK, Retry, TryAgain, Yes };
	MessageBoxResult ShowMessageBox(std::string_view message, std::string_view title, MessageBoxButtons buttons, MessageBoxIcon icon, void* parentWindowHandle);

	enum class FileDialogItemType { VisualGroupStart, VisualGroupEnd, Checkbox };

	struct FileDialogItem
	{
		FileDialogItemType Type;
		std::string_view Label;
		struct ItemData
		{
			b8* CheckboxChecked;
		} InOut;
	};

	constexpr std::string_view AllFilesFilterName = "All Files";
	constexpr std::string_view AllFilesFilterSpec = "*.*";

	struct FileFilter
	{
		// NOTE: In the format "File Type Name" without spec prefix
		std::string_view Name;
		// NOTE: In the format "*.ext" for a single format and "*.ext;*.ext;*.ext" for a list
		std::string_view Spec;
	};

	enum class FileDialogResult { OK, Cancel, Error };

	struct FileDialog
	{
		std::string_view InTitle;
		std::string_view InFileName;
		std::string_view InDefaultExtension;
		std::vector<FileFilter> InFilters;
		u32 InOutFilterIndex = 0;
		void* InParentWindowHandle = nullptr;
		std::vector<FileDialogItem> InOutCustomizeItems;
		std::string OutFilePath;

		FileDialogResult OpenRead();
		FileDialogResult OpenSave();
		FileDialogResult OpenSelectFolder();
	};
}
