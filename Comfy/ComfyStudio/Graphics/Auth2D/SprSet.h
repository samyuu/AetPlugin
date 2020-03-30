#pragma once
#include "Types.h"
#include "Graphics/TxpSet.h"
#include "Graphics/GraphicTypes.h"

namespace Comfy::Graphics
{
	struct Spr
	{
		int32_t TextureIndex;
		float Unknown;
		vec4 TexelRegion;
		vec4 PixelRegion;
		std::string Name;
		unk32_t GraphicsReserved;
		DisplayMode DisplayMode;

		vec2 GetSize() const;
	};

	class SprSet : public FileSystem::IBufferParsable
	{
	public:
		std::string Name;
		uint32_t Signature;
		UniquePtr<TxpSet> TxpSet;
		std::vector<Spr> Sprites;

		void Parse(const uint8_t* buffer, size_t bufferSize) override;

	private:
	};
}
