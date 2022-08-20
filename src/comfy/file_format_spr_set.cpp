#include "file_format_spr_set.h"

namespace Comfy
{
	StreamResult Tex::Read(StreamReader& reader)
	{
		reader.PushBaseOffset();
		const auto texSignature = static_cast<TxpSig>(reader.ReadU32());
		const auto mipMapCount = reader.ReadU32();

		const auto mipLevels = reader.ReadU8();
		const auto arraySize = reader.ReadU8();
		const auto depth = reader.ReadU8();
		const auto dimensions = reader.ReadU8();

		if (texSignature != TxpSig::Texture2D && texSignature != TxpSig::CubeMap)
			return StreamResult::BadFormat;

		const auto adjustedMipLevels = (texSignature == TxpSig::CubeMap) ? (mipMapCount / arraySize) : mipMapCount;

		MipMapsArray.reserve(arraySize);
		for (size_t i = 0; i < arraySize; i++)
		{
			auto& mipMaps = MipMapsArray.emplace_back();
			mipMaps.reserve(adjustedMipLevels);

			for (size_t j = 0; j < adjustedMipLevels; j++)
			{
				const auto mipMapOffset = reader.ReadPtr_32();
				if (!reader.IsValidPointer(mipMapOffset))
					return StreamResult::BadPointer;

				auto streamResult = StreamResult::Success;
				reader.ReadAtOffsetAware(mipMapOffset, [&](StreamReader& reader)
				{
					const auto mipSignature = static_cast<TxpSig>(reader.ReadU32());
					if (mipSignature != TxpSig::MipMap)
					{
						streamResult = StreamResult::BadFormat;
						return;
					}

					auto& mipMap = mipMaps.emplace_back();
					mipMap.Size.x = reader.ReadI32();
					mipMap.Size.y = reader.ReadI32();
					mipMap.Format = static_cast<TextureFormat>(reader.ReadU32());

					const auto mipIndex = reader.ReadU8();
					const auto arrayIndex = reader.ReadU8();
					const auto padding = reader.ReadU16();

					mipMap.DataSize = reader.ReadU32();
					if (mipMap.DataSize > static_cast<size_t>(reader.GetRemaining()))
					{
						streamResult = StreamResult::BadCount;
						return;
					}

					mipMap.Data = std::make_unique<u8[]>(mipMap.DataSize);
					reader.ReadBuffer(mipMap.Data.get(), mipMap.DataSize);
				});
				if (streamResult != StreamResult::Success)
					return streamResult;
			}
		}

		reader.PopBaseOffset();
		return StreamResult::Success;
	}

	StreamResult TexSet::Read(StreamReader& reader)
	{
		auto baseHeader = SectionHeader::TryRead(reader, SectionSignature::MTXD);
		if (!baseHeader.has_value())
			baseHeader = SectionHeader::TryRead(reader, SectionSignature::TXPC);

		SectionHeader::ScanPOFSectionsForPtrSize(reader);

		if (baseHeader.has_value())
		{
			reader.SetEndianness(baseHeader->Endianness);
			reader.Seek(baseHeader->StartOfSubSectionAddress());

			if (reader.GetPtrSize() == PtrSize::Mode64Bit)
				reader.PushBaseOffset();
		}

		reader.PushBaseOffset();

		const auto setSignature = static_cast<TxpSig>(reader.ReadU32());
		const auto textureCount = reader.ReadU32();
		const auto packedInfo = reader.ReadU32();

		if (setSignature != TxpSig::TexSet)
			return StreamResult::BadFormat;

		Textures.reserve(textureCount);
		for (size_t i = 0; i < textureCount; i++)
		{
			const auto textureOffset = reader.ReadPtr_32();
			if (!reader.IsValidPointer(textureOffset))
				return StreamResult::BadPointer;

			auto streamResult = StreamResult::Success;
			reader.ReadAtOffsetAware(textureOffset, [&](StreamReader& reader)
			{
				streamResult = Textures.emplace_back(std::make_shared<Tex>())->Read(reader);
			});

			if (streamResult != StreamResult::Success)
				return streamResult;
		}

		reader.PopBaseOffset();

		if (baseHeader.has_value() && reader.GetPtrSize() == PtrSize::Mode64Bit)
			reader.PopBaseOffset();

		return StreamResult::Success;
	}

	StreamResult TexSet::Write(StreamWriter& writer)
	{
		const u32 textureCount = static_cast<u32>(Textures.size());
		constexpr u32 packedMask = 0x01010100;

		const FileAddr setBaseAddress = writer.GetPosition();
		writer.WriteU32(static_cast<u32>(TxpSig::TexSet));
		writer.WriteU32(textureCount);
		writer.WriteU32(textureCount | packedMask);

		for (const auto& texture : Textures)
		{
			writer.WriteFuncPtr([&](StreamWriter& writer)
			{
				const FileAddr texBaseAddress = writer.GetPosition();
				const u8 arraySize = static_cast<u8>(texture->MipMapsArray.size());
				const u8 mipLevels = (arraySize > 0) ? static_cast<u8>(texture->MipMapsArray.front().size()) : 0;

				writer.WriteU32(static_cast<u32>(texture->GetSignature()));
				writer.WriteU32(arraySize * mipLevels);
				writer.WriteU8(mipLevels);
				writer.WriteU8(arraySize);
				writer.WriteU8(0x01);
				writer.WriteU8(0x01);

				for (u8 arrayIndex = 0; arrayIndex < arraySize; arrayIndex++)
				{
					for (u8 mipIndex = 0; mipIndex < mipLevels; mipIndex++)
					{
						writer.WriteFuncPtr([arrayIndex, mipIndex, &texture](StreamWriter& writer)
						{
							const auto& mipMap = texture->MipMapsArray[arrayIndex][mipIndex];
							writer.WriteU32(static_cast<u32>(TxpSig::MipMap));
							writer.WriteI32(mipMap.Size.x);
							writer.WriteI32(mipMap.Size.y);
							writer.WriteU32(static_cast<u32>(mipMap.Format));
							writer.WriteU8(mipIndex);
							writer.WriteU8(arrayIndex);
							writer.WriteU8(0x00);
							writer.WriteU8(0x00);
							writer.WriteU32(mipMap.DataSize);
							writer.WriteBuffer(mipMap.Data.get(), mipMap.DataSize);
						}, texBaseAddress);
					}
				}
			}, setBaseAddress);
		}

		writer.FlushPointerPool();
		writer.WriteAlignmentPadding(16);

		return StreamResult::Success;
	}

	void SprSet::ApplyDBNames(const SprSetEntry& sprSetEntry)
	{
		if (sprSetEntry.SprEntries.size() != Sprites.size())
			return;

		if (Name.empty())
			Name = sprSetEntry.Name;

		// NOTE: Transform "SPR_{SET_NAME}" -> "SPR_{SET_NAME}_" to be stripped from "SPR_{SET_NAME}_{SPRITE_NAME}"
		const auto sprPrefixToStrip = (sprSetEntry.Name + "_");

		for (auto& sprEntry : sprSetEntry.SprEntries)
		{
			if (!InBounds(sprEntry.Index, Sprites))
				continue;

			// NOTE: Following the same convention that existing legacy SprSets name their sprites after
			Sprites[sprEntry.Index].Name = ASCII::TrimPrefix(sprEntry.Name, sprPrefixToStrip);
		}
	}

	StreamResult SprSet::Read(StreamReader& reader)
	{
		const auto baseHeader = SectionHeader::TryRead(reader, SectionSignature::SPRC);
		SectionHeader::ScanPOFSectionsForPtrSize(reader);

		if (baseHeader.has_value())
		{
			reader.SetEndianness(baseHeader->Endianness);
			reader.Seek(baseHeader->StartOfSubSectionAddress());

			if (reader.GetPtrSize() == PtrSize::Mode64Bit)
				reader.PushBaseOffset();
		}

		Flags = reader.ReadU32();
		const auto texSetOffset = reader.ReadPtr_32();
		const auto textureCount = reader.ReadSize_32();
		const auto spriteCount = reader.ReadU32();
		const auto spritesOffset = reader.ReadPtr();
		const auto textureNamesOffset = reader.ReadPtr();
		const auto spriteNamesOffset = reader.ReadPtr();
		const auto spriteExtraDataOffset = reader.ReadPtr();

		if (textureCount > 0)
		{
			auto streamResult = StreamResult::Success;
			if (reader.HasSections)
			{
				const auto texSetStartOffset = baseHeader->EndOfSectionAddress();
				reader.ReadAt(texSetStartOffset, [&](StreamReader& reader) { streamResult = TexSet.Read(reader); });
			}
			else
			{
				if (!reader.IsValidPointer(texSetOffset))
					return StreamResult::BadPointer;

				reader.ReadAtOffsetAware(texSetOffset, [&](StreamReader& reader) { streamResult = TexSet.Read(reader); });
			}
			if (streamResult != StreamResult::Success)
				return streamResult;

			if (reader.IsValidPointer(textureNamesOffset) && TexSet.Textures.size() == textureCount)
			{
				reader.ReadAtOffsetAware(textureNamesOffset, [&](StreamReader& reader)
				{
					for (auto& texture : this->TexSet.Textures)
						texture->Name = reader.ReadStrPtrOffsetAware();
				});
			}
		}

		if (spriteCount > 0)
		{
			if (!reader.IsValidPointer(spritesOffset) || !reader.IsValidPointer(spriteExtraDataOffset))
				return StreamResult::BadPointer;

			auto streamResult = StreamResult::Success;
			reader.ReadAtOffsetAware(spritesOffset, [&](StreamReader& reader)
			{
				Sprites.reserve(spriteCount);
				for (size_t i = 0; i < spriteCount; i++)
				{
					auto& sprite = Sprites.emplace_back();
					sprite.TextureIndex = reader.ReadI32();
					sprite.Rotate = reader.ReadI32();
					sprite.TexelRegion = reader.ReadVec4();
					sprite.PixelRegion = reader.ReadVec4();
				}
			});
			if (streamResult != StreamResult::Success)
				return streamResult;

			if (reader.IsValidPointer(spriteNamesOffset) && Sprites.size() == spriteCount)
			{
				reader.ReadAtOffsetAware(spriteNamesOffset, [&](StreamReader& reader)
				{
					for (auto& sprite : Sprites)
						sprite.Name = reader.ReadStrPtrOffsetAware();
				});
			}

			reader.ReadAtOffsetAware(spriteExtraDataOffset, [&](StreamReader& reader)
			{
				for (auto& sprite : Sprites)
				{
					sprite.Extra.Flags = reader.ReadU32();
					sprite.Extra.ScreenMode = static_cast<ScreenMode>(reader.ReadU32());
				}
			});
		}

		if (baseHeader.has_value() && reader.GetPtrSize() == PtrSize::Mode64Bit)
			reader.PopBaseOffset();

		return StreamResult::Success;
	}

	StreamResult SprSet::Write(StreamWriter& writer)
	{
		writer.WriteU32(Flags);

		const auto texSetPtrAddress = writer.GetPosition();
		writer.WriteU32(0x00000000);
		writer.WriteU32(static_cast<u32>(TexSet.Textures.size()));

		writer.WriteU32(static_cast<u32>(Sprites.size()));
		writer.WriteFuncPtr([&](StreamWriter& writer)
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

		writer.WriteFuncPtr([&](StreamWriter& writer)
		{
			for (const auto& texture : this->TexSet.Textures)
			{
				if (texture->Name.has_value())
					writer.WriteStrPtr(texture->Name.value());
				else
					writer.WritePtr(FileAddr::NullPtr);
			}
		});

		writer.WriteFuncPtr([&](StreamWriter& writer)
		{
			for (const auto& sprite : Sprites)
				writer.WriteStrPtr(sprite.Name);
		});

		writer.WriteFuncPtr([&](StreamWriter& writer)
		{
			for (const auto& sprite : Sprites)
			{
				writer.WriteU32(sprite.Extra.Flags);
				writer.WriteU32(static_cast<u32>(sprite.Extra.ScreenMode));
			}
		});

		writer.FlushPointerPool();
		writer.WriteAlignmentPadding(16);

		writer.FlushStringPointerPool();
		writer.WriteAlignmentPadding(16);

		const auto texSetPtr = writer.GetPosition();
		TexSet.Write(writer);

		writer.Seek(texSetPtrAddress);
		writer.WritePtr(texSetPtr);

		return StreamResult::Success;
	}
}
