#include "SprSet.h"
#include "FileSystem/BinaryReader.h"

using namespace Comfy::FileSystem;

namespace Comfy::Graphics
{
	vec2 Spr::GetSize() const
	{
		return vec2(PixelRegion.z, PixelRegion.w);
	}

	void SprSet::Parse(const uint8_t* buffer, size_t bufferSize)
	{
		SprSet* sprSet = this;

		sprSet->Signature = *(uint32_t*)(buffer + 0);
		uint32_t txpSetOffset = *(uint32_t*)(buffer + 4);
		uint32_t textureCount = *(uint32_t*)(buffer + 8);
		uint32_t spritesCount = *(uint32_t*)(buffer + 12);
		uint32_t spritesOffset = *(uint32_t*)(buffer + 16);
		uint32_t textureNamesOffset = *(uint32_t*)(buffer + 20);
		uint32_t spriteNamesOffset = *(uint32_t*)(buffer + 24);
		uint32_t spriteExtraDataOffset = *(uint32_t*)(buffer + 28);

		if (txpSetOffset != 0)
		{
			sprSet->TxpSet = MakeUnique<Graphics::TxpSet>();
			sprSet->TxpSet->Parse(buffer + txpSetOffset, bufferSize - txpSetOffset);
		}

		if (spritesOffset != 0)
		{
			const uint8_t* spritesBuffer = buffer + spritesOffset;

			sprSet->Sprites.resize(spritesCount);
			for (uint32_t i = 0; i < spritesCount; i++)
			{
				Spr* sprite = &sprSet->Sprites[i];

				sprite->TextureIndex = *(uint32_t*)(spritesBuffer + 0);
				sprite->Unknown = *(float*)(spritesBuffer + 4);
				sprite->TexelRegion = *(vec4*)(spritesBuffer + 8);
				sprite->PixelRegion = *(vec4*)(spritesBuffer + 24);
				spritesBuffer += 40;
			}
		}

		if (textureNamesOffset != 0)
		{
			const uint8_t* textureNamesOffsetBuffer = buffer + textureNamesOffset;

			for (uint32_t i = 0; i < textureCount; i++)
			{
				uint32_t nameOffset = ((uint32_t*)textureNamesOffsetBuffer)[i];
				char* name = (char*)(buffer + nameOffset);
				sprSet->TxpSet->Txps[i]->Name = std::string(name);
			}
		}

		if (spriteNamesOffset != 0)
		{
			const uint8_t* spriteNamesOffsetBuffer = buffer + spriteNamesOffset;

			for (uint32_t i = 0; i < spritesCount; i++)
			{
				uint32_t nameOffset = ((uint32_t*)spriteNamesOffsetBuffer)[i];
				char* name = (char*)(buffer + nameOffset);
				sprSet->Sprites[i].Name = std::string(name);
			}
		}

		if (spriteExtraDataOffset != 0)
		{
			const uint8_t* extraDataBuffer = buffer + spriteExtraDataOffset;

			for (Spr &sprite : sprSet->Sprites)
			{
				sprite.GraphicsReserved = *((uint32_t*)extraDataBuffer + 0);
				sprite.DisplayMode = *((DisplayMode*)extraDataBuffer + 4);
				extraDataBuffer += 8;
			}
		}
	}
}
