#pragma once
#include "Types.h"
#include "CoreTypes.h"
#include "Graphics/TexSet.h"
#include "Graphics/Auth2D/SprSet.h"
#include <functional>
#include <optional>

namespace Comfy::Graphics::Utilities
{
	enum class MergeType
	{
		NoMerge, Merge,
	};

	enum class CompressionType
	{
		None, BC1, BC2, BC3, BC4, BC5, Unknown,
	};

	using SprMarkupFlags = uint32_t;
	enum SprMarkupFlagsEnum : SprMarkupFlags
	{
		SprMarkupFlags_None = 0,
		SprMarkupFlags_NoMerge = (1 << 0),
	};

	struct SprMarkup
	{
		std::string Name;
		ivec2 Size;
		const void* RGBAPixels;
		ScreenMode ScreenMode;
		SprMarkupFlags Flags;
	};

	struct SprMarkupBox
	{
		const SprMarkup* Markup;
		ivec4 Box;
	};

	struct SprTexMarkup
	{
		std::string Name;
		ivec2 Size;
		TextureFormat Format;
		MergeType Merge;
		std::vector<SprMarkupBox> SpriteBoxes;
		int RemainingFreePixels;
	};

	class SpritePacker : NonCopyable
	{
	public:
		struct ProgressData
		{
			uint32_t Sprites, SpritesTotal;
		};

		using ProgressCallback = std::function<void(SpritePacker&, ProgressData)>;

	public:
		SpritePacker() = default;
		SpritePacker(ProgressCallback callback);
		~SpritePacker() = default;

	public:
		struct SettingsData
		{
			ivec2 MaxTextureSize = ivec2(2048, 1024);

			std::optional<uint32_t> BackgroundColor = 0x00000000; // 0xFFFF00FF;

			// NOTE: Number of pixels at each side
			ivec2 SpritePadding = ivec2(2, 2);

			bool PowerOfTwoTextures = true;
			bool FlipTexturesY = true;
		} Settings;

		UniquePtr<SprSet> Create(const std::vector<SprMarkup>& sprMarkups);

	protected:
		ProgressData currentProgress = {};
		ProgressCallback progressCallback;

		void ReportCurrentProgress();

	protected:
		std::vector<SprTexMarkup> MergeTextures(const std::vector<SprMarkup>& sprMarkups);
		std::vector<const SprMarkup*> SortByArea(const std::vector<SprMarkup>& sprMarkups) const;

		std::pair<SprTexMarkup*, ivec4> FindFittingTexMarkupToPlaceSprIn(const SprMarkup& sprToPlace, std::vector<SprTexMarkup>& existingTexMarkups);
		void AdjustTexMarkupSizes(std::vector<SprTexMarkup>& texMarkups);

		RefPtr<Tex> CreateTexFromMarkup(const SprTexMarkup& texMarkup);
		UniquePtr<uint8_t[]> CreateMergedTexMarkupRGBAPixels(const SprTexMarkup& texMarkup);

		const char* GetCompressionName(TextureFormat format) const;
		std::string FormatTextureName(MergeType merge, TextureFormat format, size_t index) const;
	};
}
