#pragma once
#include "IntegralTypes.h"
#include <type_traits>

// NOTE: Any address, offset or pointer in file space
enum class FileAddr : int64_t { NullPtr = 0 };

inline FileAddr operator+(FileAddr left, FileAddr right)
{ 
	using UnderlyingType = std::underlying_type<FileAddr>::type;
	return static_cast<FileAddr>(static_cast<UnderlyingType>(left) + static_cast<UnderlyingType>(right));
}

inline FileAddr operator-(FileAddr left, FileAddr right)
{
	using UnderlyingType = std::underlying_type<FileAddr>::type;
	return static_cast<FileAddr>(static_cast<UnderlyingType>(left) - static_cast<UnderlyingType>(right));
}

inline FileAddr& operator+=(FileAddr& left, FileAddr right)
{
	using UnderlyingType = std::underlying_type<FileAddr>::type;
	return (left = static_cast<FileAddr>(static_cast<UnderlyingType>(left) + static_cast<UnderlyingType>(right)));
}

inline FileAddr& operator-=(FileAddr& left, FileAddr right)
{
	using UnderlyingType = std::underlying_type<FileAddr>::type;
	return (left = static_cast<FileAddr>(static_cast<UnderlyingType>(left) - static_cast<UnderlyingType>(right)));
}
