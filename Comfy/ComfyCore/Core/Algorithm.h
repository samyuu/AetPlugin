#pragma once
#include <type_traits>

template <typename IndexType, typename ArrayType>
constexpr inline bool InBounds(const IndexType& index, const ArrayType& array)
{
	static_assert(std::is_integral<IndexType>::value);

	if constexpr (std::is_signed<IndexType>::value)
		return (index >= 0 && index < array.size());
	else
		return (index < array.size());
}
