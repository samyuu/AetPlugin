#pragma once
#include "Types.h"
#include "CoreTypes.h"
#include "FileInterface.h"
#include <atomic>
#include <thread>

namespace Comfy::FileSystem
{
	class ISynchronousFileLoader
	{
	public:
		virtual void LoadSync() = 0;
	};

	class IAsynchronousFileLoader
	{
	public:
		virtual void LoadAsync() = 0;
		virtual void CheckStartLoadAsync() = 0;
	};

	class IFileLoader : public ISynchronousFileLoader, public IAsynchronousFileLoader
	{
	public:
		virtual bool GetIsLoaded() const = 0;
		virtual bool GetFileFound() const = 0;
		virtual bool GetIsLoading() const = 0;

		virtual const std::vector<uint8_t>& GetFileContent() const = 0;
		virtual void Read(IBinaryReadable* readable) const = 0;
		virtual void Parse(IBufferParsable* parsable) const = 0;

		virtual void FreeData() = 0;

	protected:
		virtual void SetIsLoaded(bool value) = 0;
		virtual void SetFileFound(bool value) = 0;
	};

	class FileLoader final : public IFileLoader
	{
	public:
		FileLoader();
		FileLoader(const std::string& filePath);
		~FileLoader();

		const std::string& GetFilePath() const;
		void SetFilePath(const std::string& value);

		void LoadSync() override;
		void LoadAsync() override;
		void CheckStartLoadAsync() override;

		bool GetIsLoaded() const override;
		bool GetFileFound() const override;
		bool GetIsLoading() const override;

		const std::vector<uint8_t>& GetFileContent() const override;
		void Read(IBinaryReadable* readable) const override;
		void Parse(IBufferParsable* parsable) const override;

		void FreeData() override;

	protected:
		std::string filePath;

		bool isLoaded = false;
		bool fileFound = false;
		std::vector<uint8_t> fileContent;

		std::atomic_bool threadRunning = false;
		UniquePtr<std::thread> loaderThread = nullptr;

		void CheckFileLocation();

		void SetIsLoaded(bool value) override;
		void SetFileFound(bool value) override;
	};
}
