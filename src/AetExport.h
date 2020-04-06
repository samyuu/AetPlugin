#pragma once
#include "AetPlugin.h"
#include "Graphics/Auth2D/Aet/AetSet.h"

namespace AetPlugin
{
	// TODO:
	class AetExporter : NonCopyable
	{
	public:
		AetExporter(std::wstring_view workingDirectory);
		~AetExporter() = default;

	protected:
		SuitesData suites;

	protected:
		struct WorkingDirectoryData
		{
			std::wstring ImportDirectory;
		} workingDirectory;
	};
}
