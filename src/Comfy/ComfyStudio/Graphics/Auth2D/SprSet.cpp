#include "SprSet.h"
#include "FileSystem/BinaryReader.h"
#include "FileSystem/BinaryWriter.h"

using namespace Comfy::FileSystem;

namespace Comfy::Graphics
{
	vec2 Spr::GetSize() const
	{
		return vec2(PixelRegion.z, PixelRegion.w);
	}

	void SprSet::Write(BinaryWriter& writer)
	{
		writer.WriteU32(Flags);

		const FileAddr texSetPtrAddress = writer.GetPosition();
		writer.WriteU32(0x00000000);
		writer.WriteU32((TexSet != nullptr) ? static_cast<uint32_t>(TexSet->Textures.size()) : 0);

		writer.WriteU32(static_cast<uint32_t>(Sprites.size()));
		writer.WritePtr([&](BinaryWriter& writer)
		{
			for (const auto& sprite : Sprites)
			{
				writer.WriteI32(sprite.TextureIndex);
				writer.WriteI32(sprite.Rotate);
				writer.WriteF32(sprite.TexelRegion.x);
				writer.WriteF32(sprite.TexelRegion.y);
				writer.WriteF32(sprite.TexelRegion.z);
				writer.WriteF32(sprite.TexelRegion.w);
				writer.WriteF32(sprite.PixelRegion.x);
				writer.WriteF32(sprite.PixelRegion.y);
				writer.WriteF32(sprite.PixelRegion.z);
				writer.WriteF32(sprite.PixelRegion.w);
			}
		});

		writer.WritePtr([&](BinaryWriter& writer)
		{
			if (this->TexSet == nullptr)
				return;

			for (const auto& texture : this->TexSet->Textures)
			{
				if (texture->Name.has_value())
					writer.WriteStrPtr(texture->Name.value());
				else
					writer.WritePtr(FileAddr::NullPtr);
			}
		});

		writer.WritePtr([&](BinaryWriter& writer)
		{
			for (const auto& sprite : Sprites)
				writer.WriteStrPtr(sprite.Name);
		});

		writer.WritePtr([&](BinaryWriter& writer)
		{
			for (const auto& sprite : Sprites)
			{
				writer.WriteU32(sprite.Extra.Flags);
				writer.WriteU32(static_cast<uint32_t>(sprite.Extra.ScreenMode));
			}
		});

		writer.FlushPointerPool();
		writer.WriteAlignmentPadding(16);

		writer.FlushStringPointerPool();
		writer.WriteAlignmentPadding(16);

		if (TexSet != nullptr)
		{
			const FileAddr texSetPtr = writer.GetPosition();
			TexSet->Write(writer);

			writer.SetPosition(texSetPtrAddress);
			writer.WritePtr(texSetPtr);
		}
	}

	void SprSet::Parse(const uint8_t* buffer, size_t bufferSize)
	{
		SprSet* sprSet = this;

		sprSet->Flags = *(uint32_t*)(buffer + 0);
		uint32_t texSetOffset = *(uint32_t*)(buffer + 4);
		uint32_t textureCount = *(uint32_t*)(buffer + 8);
		uint32_t spritesCount = *(uint32_t*)(buffer + 12);
		uint32_t spritesOffset = *(uint32_t*)(buffer + 16);
		uint32_t textureNamesOffset = *(uint32_t*)(buffer + 20);
		uint32_t spriteNamesOffset = *(uint32_t*)(buffer + 24);
		uint32_t spriteExtraDataOffset = *(uint32_t*)(buffer + 28);

		if (texSetOffset != 0)
		{
			sprSet->TexSet = MakeUnique<Graphics::TexSet>();
			sprSet->TexSet->Parse(buffer + texSetOffset, bufferSize - texSetOffset);
		}

		if (spritesOffset != 0)
		{
			const uint8_t* spritesBuffer = buffer + spritesOffset;

			sprSet->Sprites.resize(spritesCount);
			for (uint32_t i = 0; i < spritesCount; i++)
			{
				Spr* sprite = &sprSet->Sprites[i];

				sprite->TextureIndex = *(int32_t*)(spritesBuffer + 0);
				sprite->Rotate = *(int32_t*)(spritesBuffer + 4);
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
				sprSet->TexSet->Textures[i]->Name = (const char*)(buffer + nameOffset);
			}
		}

		if (spriteNamesOffset != 0)
		{
			const uint8_t* spriteNamesOffsetBuffer = buffer + spriteNamesOffset;

			for (uint32_t i = 0; i < spritesCount; i++)
			{
				uint32_t nameOffset = ((uint32_t*)spriteNamesOffsetBuffer)[i];
				sprSet->Sprites[i].Name = (const char*)(buffer + nameOffset);
			}
		}

		if (spriteExtraDataOffset != 0)
		{
			const uint8_t* extraDataBuffer = buffer + spriteExtraDataOffset;

			for (auto& sprite : sprSet->Sprites)
			{
				sprite.Extra.Flags = *((uint32_t*)extraDataBuffer + 0);
				sprite.Extra.ScreenMode = *((ScreenMode*)extraDataBuffer + 4);
				extraDataBuffer += 8;
			}
		}
	}
}
