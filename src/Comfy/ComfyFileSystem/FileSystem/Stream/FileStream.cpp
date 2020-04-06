#include "FileStream.h"
#include "Core/Win32/ComfyWindows.h"
#include "FileSystem/FileHelperInternal.h"
#include <assert.h>

namespace Comfy::FileSystem
{
	namespace
	{
		constexpr unsigned long GetWin32FileShareMode()
		{
			return (FILE_SHARE_READ | FILE_SHARE_WRITE);
		}

		unsigned long GetWin32FileAttributes()
		{
			return (FILE_ATTRIBUTE_NORMAL);
		}
	}

	FileStream::FileStream()
	{
	}

	FileStream::FileStream(std::wstring_view filePath)
	{
		OpenReadWrite(filePath);
	}

	FileStream::~FileStream()
	{
		Close();
	}

	void FileStream::Seek(FileAddr position)
	{
		::LARGE_INTEGER distanceToMove;
		distanceToMove.QuadPart = static_cast<LONGLONG>(position);
		::SetFilePointerEx(fileHandle, distanceToMove, NULL, FILE_BEGIN);

		this->position = position;
	}

	FileAddr FileStream::GetPosition() const
	{
		return position;
	}

	FileAddr FileStream::GetLength() const
	{
		return fileSize;
	}

	bool FileStream::IsOpen() const
	{
		return fileHandle != nullptr;
	}

	bool FileStream::CanRead() const
	{
		return canRead;
	}

	bool FileStream::CanWrite() const
	{
		return canWrite;
	}

	size_t FileStream::ReadBuffer(void* buffer, size_t size)
	{
		assert(canRead);

		DWORD bytesRead = 0;
		::ReadFile(fileHandle, buffer, static_cast<DWORD>(size), &bytesRead, nullptr);

		position += static_cast<FileAddr>(bytesRead);
		return bytesRead;
	}

	size_t FileStream::WriteBuffer(const void* buffer, size_t size)
	{
		assert(canWrite);

		DWORD bytesWritten = 0;
		::WriteFile(fileHandle, buffer, static_cast<DWORD>(size), &bytesWritten, nullptr);

		if (position > (GetLength() - static_cast<FileAddr>(bytesWritten)))
			fileSize += static_cast<FileAddr>(bytesWritten);
		position += static_cast<FileAddr>(bytesWritten);

		return bytesWritten;
	}

	void FileStream::OpenRead(std::wstring_view filePath)
	{
		auto pathBuffer = NullTerminatedPathBufferInternal(filePath);

		assert(fileHandle == nullptr);
		fileHandle = ::CreateFileW(pathBuffer.data(), (GENERIC_READ), GetWin32FileShareMode(), NULL, OPEN_EXISTING, GetWin32FileAttributes(), NULL);

		if (fileHandle != nullptr)
			canRead = true;
		UpdateFileSize();
	}

	void FileStream::OpenWrite(std::wstring_view filePath)
	{
		auto pathBuffer = NullTerminatedPathBufferInternal(filePath);

		assert(fileHandle == nullptr);
		fileHandle = ::CreateFileW(pathBuffer.data(), (GENERIC_WRITE), GetWin32FileShareMode(), NULL, OPEN_EXISTING, GetWin32FileAttributes(), NULL);

		if (fileHandle != nullptr)
			canWrite = true;
		UpdateFileSize();
	}

	void FileStream::OpenReadWrite(std::wstring_view filePath)
	{
		auto pathBuffer = NullTerminatedPathBufferInternal(filePath);

		assert(fileHandle == nullptr);
		fileHandle = ::CreateFileW(pathBuffer.data(), (GENERIC_READ | GENERIC_WRITE), GetWin32FileShareMode(), NULL, OPEN_EXISTING, GetWin32FileAttributes(), NULL);

		if (fileHandle != nullptr)
		{
			canRead = true;
			canWrite = true;
		}
		UpdateFileSize();
	}

	void FileStream::CreateWrite(std::wstring_view filePath)
	{
		auto pathBuffer = NullTerminatedPathBufferInternal(filePath);

		assert(fileHandle == nullptr);
		fileHandle = ::CreateFileW(pathBuffer.data(), (GENERIC_WRITE), GetWin32FileShareMode(), NULL, CREATE_ALWAYS, GetWin32FileAttributes(), NULL);

		if (fileHandle != nullptr)
			canWrite = true;
		UpdateFileSize();
	}

	void FileStream::CreateReadWrite(std::wstring_view filePath)
	{
		auto pathBuffer = NullTerminatedPathBufferInternal(filePath);

		assert(fileHandle == nullptr);
		fileHandle = ::CreateFileW(pathBuffer.data(), (GENERIC_READ | GENERIC_WRITE), GetWin32FileShareMode(), NULL, CREATE_ALWAYS, GetWin32FileAttributes(), NULL);

		if (fileHandle != nullptr)
		{
			canRead = true;
			canWrite = true;
		}
		UpdateFileSize();
	}

	void FileStream::Close()
	{
		if (fileHandle != nullptr)
			::CloseHandle(fileHandle);
		fileHandle = nullptr;
	}

	void FileStream::UpdateFileSize()
	{
		if (fileHandle != nullptr)
		{
			::LARGE_INTEGER largeIntegerFileSize = {};
			::GetFileSizeEx(fileHandle, &largeIntegerFileSize);

			fileSize = static_cast<FileAddr>(largeIntegerFileSize.QuadPart);
		}
		else
		{
			assert(false);
			fileSize = {};
			canRead = false;
			canWrite = false;
		}
	}
}
