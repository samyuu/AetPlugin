#include "core_types.h"
#include <Windows.h>

static_assert(BitsPerByte == 8);
static_assert((sizeof(u8) * BitsPerByte) == 8 && (sizeof(i8) * BitsPerByte) == 8);
static_assert((sizeof(u16) * BitsPerByte) == 16 && (sizeof(i16) * BitsPerByte) == 16);
static_assert((sizeof(u32) * BitsPerByte) == 32 && (sizeof(i32) * BitsPerByte) == 32);
static_assert((sizeof(u64) * BitsPerByte) == 64 && (sizeof(i64) * BitsPerByte) == 64);
static_assert((sizeof(f32) * BitsPerByte) == 32 && (sizeof(f64) * BitsPerByte) == 64);
static_assert((sizeof(b8) * BitsPerByte) == 8);

static i64 Win32GetPerformanceCounterTicksPerSecond()
{
	::LARGE_INTEGER frequency = {};
	::QueryPerformanceFrequency(&frequency);
	return frequency.QuadPart;
}

static i64 Win32GetPerformanceCounterTicksNow()
{
	::LARGE_INTEGER timeNow = {};
	::QueryPerformanceCounter(&timeNow);
	return timeNow.QuadPart;
}

static struct Win32PerformanceCounterData
{
	i64 TicksPerSecond = Win32GetPerformanceCounterTicksPerSecond();
	i64 TicksOnProgramStartup = Win32GetPerformanceCounterTicksNow();
} Win32GlobalPerformanceCounter = {};

CPUTime CPUTime::GetNow()
{
	return CPUTime { Win32GetPerformanceCounterTicksNow() - Win32GlobalPerformanceCounter.TicksOnProgramStartup };
}

CPUTime CPUTime::GetNowAbsolute()
{
	return CPUTime { Win32GetPerformanceCounterTicksNow() };
}

Time CPUTime::DeltaTime(const CPUTime& startTime, const CPUTime& endTime)
{
	const i64 deltaTicks = (endTime.Ticks - startTime.Ticks);
	return Time::FromSeconds(static_cast<f64>(deltaTicks) / static_cast<f64>(Win32GlobalPerformanceCounter.TicksPerSecond));
}
