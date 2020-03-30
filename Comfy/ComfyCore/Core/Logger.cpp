#include "Logger.h"
#include <stdarg.h>

namespace Comfy
{
	void Logger::NewLine()
	{
		putc('\n', GetStream());
	}

	void Logger::Log(_Printf_format_string_ char const* const format, ...)
	{
		va_list arguments;

		va_start(arguments, format);
		vfprintf(GetStream(), format, arguments);
		va_end(arguments);
	}

	void Logger::LogLine(_Printf_format_string_ char const* const format, ...)
	{
		va_list arguments;

		va_start(arguments, format);
		vfprintf(GetStream(), format, arguments);
		va_end(arguments);
		putchar('\n');
	}

	void Logger::LogError(_Printf_format_string_ char const* const format, ...)
	{
		va_list arguments;

		va_start(arguments, format);
		vfprintf(GetErrorStream(), format, arguments);
		va_end(arguments);
	}

	void Logger::LogErrorLine(_Printf_format_string_ char const* const format, ...)
	{
		va_list arguments;

		va_start(arguments, format);
		vfprintf(GetErrorStream(), format, arguments);
		va_end(arguments);
		putchar('\n');
	}

	inline FILE* Logger::GetStream()
	{
		return stdout;
	}

	inline FILE* Logger::GetErrorStream()
	{
		return stderr;
	}
}
