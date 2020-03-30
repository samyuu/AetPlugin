#pragma once
#include <memory>

template <class T>
using UniquePtr = _STD unique_ptr<T>;

template<class T, class... Types, _STD enable_if_t<!_STD is_array_v<T>, int> = 0>
_NODISCARD inline UniquePtr<T> MakeUnique(Types&&... args)
{
	return (UniquePtr<T>(new T(_STD forward<Types>(args)...)));
}

template<class T, _STD enable_if_t<_STD is_array_v<T> && _STD extent_v<T> == 0, int> = 0> 
_NODISCARD inline UniquePtr<T> MakeUnique(size_t size)
{
	typedef _STD remove_extent_t<T> Elem;
	return (UniquePtr<T>(new Elem[size]()));
}

template<class T, class... Types, _STD enable_if_t<_STD extent_v<T> != 0, int> = 0>
void MakeUnique(Types&&...) = delete;

template <class T>
using RefPtr = _STD shared_ptr<T>;

template<class T, class... Types> 
_NODISCARD inline RefPtr<T> MakeRef(Types&&... args)
{
	return (_STD shared_ptr<T>(new T(_STD forward<Types>(args)...)));
}

template <class T>
using WeakPtr = _STD weak_ptr<T>;
