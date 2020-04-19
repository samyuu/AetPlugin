#include "SpritePacker.h"
#include <numeric>
#include <intrin.h>

namespace Comfy::Graphics::Utilities
{
	namespace
	{
		constexpr size_t RGBABytesPerPixel = 4;

		constexpr int Area(const ivec2& size)
		{
			return (size.x * size.y);
		}

		constexpr bool Intersects(const ivec4& boxA, const ivec4& boxB)
		{
			return
				(boxB.x < boxA.x + boxA.z) && (boxA.x < (boxB.x + boxB.z)) &&
				(boxB.y < boxA.y + boxA.w) && (boxA.y < boxB.y + boxB.w);
		}

		constexpr bool Contains(const ivec4& boxA, const ivec4& boxB)
		{
			return
				(boxA.x <= boxB.x) && ((boxB.x + boxB.z) <= (boxA.x + boxA.z)) &&
				(boxA.y <= boxB.y) && ((boxB.y + boxB.w) <= (boxA.y + boxA.w));
		}

		constexpr int GetBoxRight(const ivec4& box)
		{
			return box.x + box.z;
		}

		constexpr int GetBoxBottom(const ivec4& box)
		{
			return box.y + box.w;
		}

		uint32_t RoundToNearestPowerOfTwo(uint32_t x)
		{
			return (x <= 1 || x > ((std::numeric_limits<uint32_t>::max() / 2) + 1)) ? x : (1 << (32 - __lzcnt(x - 1)));
		}

		ivec2 RoundToNearestPowerOfTwo(ivec2 input)
		{
			return
			{
				static_cast<int>(RoundToNearestPowerOfTwo(static_cast<uint32_t>(input.x))),
				static_cast<int>(RoundToNearestPowerOfTwo(static_cast<uint32_t>(input.y))),
			};
		}

		bool FitsInsideTexture(const ivec4& textureBox, const std::vector<SprMarkupBox>& existingSprites, const ivec4& spriteBox)
		{
			if (!Contains(textureBox, spriteBox))
				return false;

			for (const auto& existingSprite : existingSprites)
			{
				if (Intersects(existingSprite.Box, spriteBox))
					return false;
			}

			return true;
		}

		constexpr vec4 GetTexelRegionFromPixelRegion(const vec4& pixelRegion, vec2 textureSize)
		{
			vec4 texelRegion =
			{
				(pixelRegion.x / textureSize.x),
				(pixelRegion.y / textureSize.y),
				(pixelRegion.z / textureSize.x),
				(pixelRegion.w / textureSize.y),
			};
			return texelRegion;
		}

		uint32_t& GetPixel(int width, void* rgbaPixels, int x, int y)
		{
			return reinterpret_cast<uint32_t*>(rgbaPixels)[(width * y) + x];
		}

		const uint32_t& GetPixel(int width, const void* rgbaPixels, int x, int y)
		{
			return reinterpret_cast<const uint32_t*>(rgbaPixels)[(width * y) + x];
		}

		void CopySprIntoTex(const SprTexMarkup& texMarkup, void* texData, const SprMarkupBox& sprBox)
		{
			const auto texSize = texMarkup.Size;

			const auto sprSize = sprBox.Markup->Size;
			const auto sprBoxSize = ivec2(sprBox.Box.z, sprBox.Box.w);

			const auto sprPadding = (sprBoxSize - sprSize) / 2;
			const auto sprOffset = ivec2(sprBox.Box.x, sprBox.Box.y) + sprPadding;

			const void* sprData = sprBox.Markup->RGBAPixels;

			if (sprPadding.x > 0 && sprPadding.y > 0 && sprSize.x > 0 && sprSize.y > 0)
			{
				const auto cornerTopLeft = ivec2(sprBox.Box.x, sprBox.Box.y);
				const auto cornerBottomRight = cornerTopLeft + ivec2(sprBox.Box.z, sprBox.Box.w) - sprPadding;
				for (int x = 0; x < sprPadding.x; x++)
				{
					for (int y = 0; y < sprPadding.y; y++)
					{
						const auto topLeft = cornerTopLeft + ivec2(x, y);
						const auto bottomRight = cornerBottomRight + ivec2(x, y);

						// NOTE: Top left / bottom left / top right / bottom right
						GetPixel(texSize.x, texData, topLeft.x, topLeft.y) = GetPixel(sprSize.x, sprData, 0, 0);
						GetPixel(texSize.x, texData, topLeft.x, bottomRight.y) = GetPixel(sprSize.x, sprData, 0, sprSize.y - 1);
						GetPixel(texSize.x, texData, bottomRight.x, topLeft.y) = GetPixel(sprSize.x, sprData, sprSize.x - 1, 0);
						GetPixel(texSize.x, texData, bottomRight.x, bottomRight.y) = GetPixel(sprSize.x, sprData, sprSize.x - 1, sprSize.y - 1);
					}
				}

				// NOTE: Top / bottom / left / right
				for (int x = sprPadding.x; x < sprBoxSize.x - sprPadding.x; x++)
				{
					for (int y = 0; y < sprPadding.y; y++)
						GetPixel(texSize.x, texData, x + sprBox.Box.x, y + sprBox.Box.y) = GetPixel(sprSize.x, sprData, x - sprPadding.x, 0);
					for (int y = sprBoxSize.y - sprPadding.y; y < sprBoxSize.y; y++)
						GetPixel(texSize.x, texData, x + sprBox.Box.x, y + sprBox.Box.y) = GetPixel(sprSize.x, sprData, x - sprPadding.x, sprSize.y - 1);
				}
				for (int y = sprPadding.y; y < sprBoxSize.y - sprPadding.y; y++)
				{
					for (int x = 0; x < sprPadding.x; x++)
						GetPixel(texSize.x, texData, x + sprBox.Box.x, y + sprBox.Box.y) = GetPixel(sprSize.x, sprData, 0, y - sprPadding.y);
					for (int x = sprBoxSize.x - sprPadding.x; x < sprBoxSize.x; x++)
						GetPixel(texSize.x, texData, x + sprBox.Box.x, y + sprBox.Box.y) = GetPixel(sprSize.x, sprData, sprSize.x - 1, y - sprPadding.y);
				}
			}

			for (int x = 0; x < sprSize.x; x++)
			{
				for (int y = 0; y < sprSize.y; y++)
				{
					const uint32_t& sprPixel = GetPixel(sprSize.x, sprData, x, y);
					uint32_t& texPixel = GetPixel(texSize.x, texData, x + sprOffset.x, y + sprOffset.y);

					texPixel = sprPixel;
				}
			}
		}

		void FlipTextureY(ivec2 size, void* rgbaPixels)
		{
			for (int y = 0; y < size.y / 2; y++)
			{
				for (int x = 0; x < size.x; x++)
				{
					uint32_t& pixel = GetPixel(size.x, rgbaPixels, x, y);
					uint32_t& flippedPixel = GetPixel(size.x, rgbaPixels, x, size.y - 1 - y);

					std::swap(pixel, flippedPixel);
				}
			}
		}
	}

	SpritePacker::SpritePacker(ProgressCallback callback)
		: progressCallback(callback)
	{
	}

	UniquePtr<SprSet> SpritePacker::Create(const std::vector<SprMarkup>& sprMarkups)
	{
		currentProgress = {};

		auto result = MakeUnique<SprSet>();
		result->TexSet = MakeUnique<TexSet>();

		SprSet& sprSet = *result;
		TexSet& texSet = *sprSet.TexSet;

		sprSet.Flags = 0;
		sprSet.Sprites.reserve(sprMarkups.size());

		auto texMarkups = MergeTextures(sprMarkups);
		texSet.Textures.reserve(texMarkups.size());

		for (int texIndex = 0; texIndex < static_cast<int>(texMarkups.size()); texIndex++)
		{
			const auto& texMarkup = texMarkups[texIndex];
			for (const auto& spriteBox : texMarkup.SpriteBoxes)
			{
				const auto& sprMarkup = *spriteBox.Markup;
				auto& spr = sprSet.Sprites.emplace_back();
				spr.TextureIndex = texIndex;
				spr.Rotate = 0;
				spr.PixelRegion = ivec4(ivec2(spriteBox.Box) + Settings.SpritePadding, sprMarkup.Size);
				spr.TexelRegion = GetTexelRegionFromPixelRegion(spr.PixelRegion, texMarkup.Size);
				spr.Name = sprMarkup.Name;
				spr.Extra.Flags = 0;
				spr.Extra.ScreenMode = sprMarkup.ScreenMode;
			}

			texSet.Textures.push_back(CreateTexFromMarkup(texMarkup));
		}

		return result;
	}

	void SpritePacker::ReportCurrentProgress()
	{
		if (progressCallback)
			progressCallback(*this, currentProgress);
	}

	std::vector<SprTexMarkup> SpritePacker::MergeTextures(const std::vector<SprMarkup>& sprMarkups)
	{
		currentProgress.Sprites = 0;
		currentProgress.SpritesTotal = static_cast<uint32_t>(sprMarkups.size());

		const auto sizeSortedSprMarkups = SortByArea(sprMarkups);
		std::vector<SprTexMarkup> texMarkups;

		size_t noMergeIndex = 0, mergeIndex = 0;

		for (const auto* sprMarkupPtr : sizeSortedSprMarkups)
		{
			auto addNewTexMarkup = [&](ivec2 texSize, const auto& sprMarkup, ivec2 sprSize, TextureFormat format, MergeType merge, size_t& inOutIndex)
			{
				auto& texMarkup = texMarkups.emplace_back();
				texMarkup.Size = (Settings.PowerOfTwoTextures) ? RoundToNearestPowerOfTwo(texSize) : texSize;
				texMarkup.Format = format;
				texMarkup.Merge = merge;
				texMarkup.SpriteBoxes.push_back({ &sprMarkup, ivec4(ivec2(0, 0), sprSize) });
				texMarkup.Name = FormatTextureName(texMarkup.Merge, texMarkup.Format, inOutIndex++);
				texMarkup.RemainingFreePixels = Area(texMarkup.Size) - Area(sprSize);
			};

			const auto& sprMarkup = *sprMarkupPtr;
			if (sprMarkup.Flags & SprMarkupFlags_NoMerge)
			{
				addNewTexMarkup(sprMarkup.Size, sprMarkup, sprMarkup.Size, TextureFormat::RGBA8, MergeType::NoMerge, noMergeIndex);
			}
			else if (sprMarkup.Size.x > Settings.MaxTextureSize.x || sprMarkup.Size.y > Settings.MaxTextureSize.y)
			{
				addNewTexMarkup(sprMarkup.Size, sprMarkup, sprMarkup.Size + (Settings.SpritePadding * 2), TextureFormat::RGBA8, MergeType::Merge, mergeIndex);
			}
			else
			{
				if (const auto[fittingTex, fittingSprBox] = FindFittingTexMarkupToPlaceSprIn(sprMarkup, texMarkups); fittingTex != nullptr)
				{
					fittingTex->SpriteBoxes.push_back({ &sprMarkup, fittingSprBox });
					fittingTex->RemainingFreePixels -= Area(ivec2(fittingSprBox.z, fittingSprBox.w));
				}
				else
				{
					const auto newTextureSize = Settings.MaxTextureSize;
					addNewTexMarkup(newTextureSize, sprMarkup, sprMarkup.Size + (Settings.SpritePadding * 2), TextureFormat::RGBA8, MergeType::Merge, mergeIndex);
				}
			}

			currentProgress.Sprites++;
			ReportCurrentProgress();
		}

		AdjustTexMarkupSizes(texMarkups);
		return texMarkups;
	}

	std::vector<const SprMarkup*> SpritePacker::SortByArea(const std::vector<SprMarkup>& sprMarkups) const
	{
		std::vector<const SprMarkup*> result;
		result.reserve(sprMarkups.size());

		for (auto& sprMarkup : sprMarkups)
			result.push_back(&sprMarkup);

		std::sort(result.begin(), result.end(), [](auto& sprA, auto& sprB)
		{
			return Area(sprA->Size) > Area(sprB->Size);
		});

		return result;
	}

	std::pair<SprTexMarkup*, ivec4> SpritePacker::FindFittingTexMarkupToPlaceSprIn(const SprMarkup& sprToPlace, std::vector<SprTexMarkup>& existingTexMarkups)
	{
		// TODO: "Largest failed SprMarkupBox" to then compare with all future boxes (?)
		constexpr int stepSize = 1;
		constexpr int roughStepSize = 8;

#if 1 // NOTE: Forward
		for (auto& existingTexMarkup : existingTexMarkups)
		{
#else // NOTE: Backwards
		for (int i = static_cast<int>(existingTexMarkups.size()) - 1; i >= 0; i--)
		{
			auto& existingTexMarkup = existingTexMarkups[i];
#endif

			if (existingTexMarkup.Merge == MergeType::NoMerge || existingTexMarkup.RemainingFreePixels < Area(sprToPlace.Size))
				continue;

			const ivec2 texBoxSize = existingTexMarkup.Size;
			const ivec4 texBox = ivec4(ivec2(0, 0), texBoxSize);

			const ivec2 sprBoxSize = sprToPlace.Size + (Settings.SpritePadding * 2);
			ivec4 sprBox = ivec4(ivec2(0, 0), sprBoxSize);

#if 0 // NOTE: Precise step only
			for (sprBox.y = 0; sprBox.y < texBoxSize.y - sprBoxSize.y; sprBox.y += stepSize)
			{
				for (sprBox.x = 0; sprBox.x < texBoxSize.x - sprBoxSize.x; sprBox.x += stepSize)
				{
					if (FitsInsideTexture(texBox, existingTexMarkup.SpriteBoxes, sprBox))
						return std::make_pair(&existingTexMarkup, sprBox);
				}
			}
#else // NOTE: Rough step first then precise adjust
			for (sprBox.y = 0; sprBox.y < texBoxSize.y - sprBoxSize.y; sprBox.y += roughStepSize)
			{
				for (sprBox.x = 0; sprBox.x < texBoxSize.x - sprBoxSize.x; sprBox.x += roughStepSize)
				{
					if (!FitsInsideTexture(texBox, existingTexMarkup.SpriteBoxes, sprBox))
						continue;

					const auto roughSprBox = sprBox;

					for (int preciseY = roughStepSize - 1; preciseY >= 0; preciseY--)
					{
						for (int preciseX = roughStepSize - 1; preciseX >= 0; preciseX--)
						{
							const auto preciseSprBox = ivec4(sprBox.x - preciseX, sprBox.y - preciseY, sprBox.z, sprBox.w);
							if (FitsInsideTexture(texBox, existingTexMarkup.SpriteBoxes, preciseSprBox))
								return std::make_pair(&existingTexMarkup, preciseSprBox);
						}
					}

					return std::make_pair(&existingTexMarkup, roughSprBox);
		}
		}
#endif
	}

		return std::make_pair(static_cast<SprTexMarkup*>(nullptr), ivec4(0, 0, 0, 0));
}

	void SpritePacker::AdjustTexMarkupSizes(std::vector<SprTexMarkup>& texMarkups)
	{
		for (auto& texMarkup : texMarkups)
		{
			const auto maxRight = std::max_element(
				texMarkup.SpriteBoxes.begin(),
				texMarkup.SpriteBoxes.end(),
				[](const auto& sprBoxA, const auto& sprBoxB) { return GetBoxRight(sprBoxA.Box) < GetBoxRight(sprBoxB.Box); });

			const auto maxBottom = std::max_element(
				texMarkup.SpriteBoxes.begin(),
				texMarkup.SpriteBoxes.end(),
				[](const auto& sprBoxA, const auto& sprBoxB) { return GetBoxBottom(sprBoxA.Box) < GetBoxBottom(sprBoxB.Box); });

			if (maxRight == texMarkup.SpriteBoxes.end() || maxBottom == texMarkup.SpriteBoxes.end())
				continue;

			const auto texNeededSize = ivec2(GetBoxRight(maxRight->Box), GetBoxBottom(maxBottom->Box));
			texMarkup.Size = (Settings.PowerOfTwoTextures) ? RoundToNearestPowerOfTwo(texNeededSize) : texNeededSize;
		}
	}

	RefPtr<Tex> SpritePacker::CreateTexFromMarkup(const SprTexMarkup& texMarkup)
	{
		auto margedRGBAPixels = CreateMergedTexMarkupRGBAPixels(texMarkup);

		auto tex = MakeRef<Tex>();
		tex->Name = texMarkup.Name;
		auto& mipMaps = tex->MipMapsArray.emplace_back();
		auto& baseMipMap = mipMaps.emplace_back();
		baseMipMap.Format = TextureFormat::RGBA8;
		baseMipMap.Size = texMarkup.Size;
		baseMipMap.DataSize = Area(texMarkup.Size) * RGBABytesPerPixel;
		// TODO: In the future only move once all mipmaps have been generated and the target format is RGBA8
		baseMipMap.Data = std::move(margedRGBAPixels);

		return tex;
	}

	UniquePtr<uint8_t[]> SpritePacker::CreateMergedTexMarkupRGBAPixels(const SprTexMarkup& texMarkup)
	{
		const size_t texDataSize = Area(texMarkup.Size) * RGBABytesPerPixel;
		auto texData = MakeUnique<uint8_t[]>(texDataSize);

		if (texMarkup.SpriteBoxes.size() == 1 && texMarkup.SpriteBoxes.front().Markup->Size == texMarkup.Size)
		{
			std::memcpy(texData.get(), texMarkup.SpriteBoxes.front().Markup->RGBAPixels, texDataSize);
		}
		else
		{
			if (Settings.BackgroundColor.has_value())
			{
				const auto backgroundColor = Settings.BackgroundColor.value();
				for (size_t i = 0; i < texDataSize / RGBABytesPerPixel; i++)
					reinterpret_cast<uint32_t*>(texData.get())[i] = backgroundColor;
			}

			for (const auto& sprBox : texMarkup.SpriteBoxes)
				CopySprIntoTex(texMarkup, texData.get(), sprBox);
		}

		if (Settings.FlipTexturesY)
			FlipTextureY(texMarkup.Size, texData.get());

		return texData;
	}

	const char* SpritePacker::GetCompressionName(TextureFormat format) const
	{
		switch (format)
		{
		case TextureFormat::A8:
		case TextureFormat::RGB8:
		case TextureFormat::RGBA8:
		case TextureFormat::RGB5:
		case TextureFormat::RGB5_A1:
		case TextureFormat::RGBA4:
		case TextureFormat::L8:
		case TextureFormat::L8A8:
			return "NOCOMP";
		case TextureFormat::DXT1:
		case TextureFormat::DXT1a:
			return "BC1COMP";
		case TextureFormat::DXT3:
			return "BC2COMP";
		case TextureFormat::DXT5:
			return "BC3COMP";
		case TextureFormat::RGTC1:
			return "BC4COMP";
		case TextureFormat::RGTC2:
			return "BC5COMP";
		default:
			return "UNKCOMP";
		}

		return nullptr;
	}

	std::string SpritePacker::FormatTextureName(MergeType merge, TextureFormat format, size_t index) const
	{
		const char* mergeString = (merge == MergeType::Merge) ? "MERGE" : "NOMERGE";
		const char* compressionString = GetCompressionName(format);

		char nameBuffer[32];
		sprintf_s(nameBuffer, "%s_%s_%03zu", mergeString, compressionString, index);
		return nameBuffer;
	}
}
