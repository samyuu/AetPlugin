#pragma once
#include "Types.h"

namespace Comfy
{
	constexpr uint32_t InvalidResourceID = 0xFFFFFFFF;

#define DECLARE_ID_TYPE(typeName) enum class typeName : uint32_t { Invalid = InvalidResourceID };
#include "Detail/IDTypeDeclarations.h"
#undef DECLARE_ID_TYPE

	template <typename IDType>
	struct CachedResourceID
	{
		CachedResourceID() = default;
		CachedResourceID(IDType id) : ID(id) {};

		IDType ID;
		mutable uint32_t CachedIndex;

		operator IDType() const { return ID; };
	};

#define DECLARE_ID_TYPE(typeName) using Cached_##typeName = CachedResourceID<typeName>;
#include "Detail/IDTypeDeclarations.h"
#undef DECLARE_ID_TYPE
}
