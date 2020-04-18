#pragma once
#include "Types.h"
#include "Graphics/TexSet.h"
#include "Graphics/GraphicTypes.h"

namespace Comfy::Graphics
{
	struct Spr
	{
		int32_t TextureIndex;
		int32_t Rotate;
		vec4 TexelRegion;
		vec4 PixelRegion;
		std::string Name;
		struct ExtraData
		{
			uint32_t Flags;
			ScreenMode ScreenMode;
		} Extra;

		vec2 GetSize() const;
	};

	class SprSet : public FileSystem::IBinaryWritable, public FileSystem::IBufferParsable, NonCopyable
	{
	public:
		std::string Name;
		uint32_t Flags;
		UniquePtr<TexSet> TexSet;
		std::vector<Spr> Sprites;

		void Write(FileSystem::BinaryWriter& writer) override;

		void Parse(const uint8_t* buffer, size_t bufferSize) override;

	private:
	};
}
