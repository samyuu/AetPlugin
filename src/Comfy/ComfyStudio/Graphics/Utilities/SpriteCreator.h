#pragma once
#include "Types.h"
#include "CoreTypes.h"
#include "Graphics/TexSet.h"
#include "Graphics/Auth2D/SprSet.h"
#include <functional>

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

	class SpriteCreator : NonCopyable
	{
	public:
		struct ProgressData
		{
			uint32_t Sprites, SpritesTotal;
		};

		using ProgressCallback = std::function<void(SpriteCreator&, ProgressData)>;

	public:
		SpriteCreator() = default;
		SpriteCreator(ProgressCallback callback);
		~SpriteCreator() = default;

	public:
		UniquePtr<SprSet> Create(const std::vector<SprMarkup>& sprMarkups);

	protected:
		struct SettingsData
		{
			ivec2 MaxTextureSize = ivec2(2048, 1024);

			// NOTE: Numbers of pixels at each side
			int SpritePadding = 2;

			bool EnsurePowerTwo = false;
			bool SetDummyColor = true;
			bool FlipY = true;
		} settings;

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
