#pragma once
#include <stdio.h>

namespace Comfy
{
	class Logger
	{
	public:
		static void NewLine();
		static void Log(_Printf_format_string_ char const* const format, ...);
		static void LogLine(_Printf_format_string_ char const* const format, ...);
		static void LogError(_Printf_format_string_ char const* const format, ...);
		static void LogErrorLine(_Printf_format_string_ char const* const format, ...);

	private:
		static FILE* GetStream();
		static FILE* GetErrorStream();
	};
}
