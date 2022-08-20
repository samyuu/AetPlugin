#pragma once
#include "core_types.h"
#include "file_format_spr_set.h"
#include <optional>

namespace Comfy
{
	b8 ReadImageFile(std::string_view filePath, ivec2& outSize, std::unique_ptr<u8[]>& outRGBAPixels);
	b8 WriteImageFile(std::string_view filePath, ivec2 size, const void* rgbaPixels);
}

namespace Comfy
{
	constexpr u32 RoundToNearestPowerOfTwo(u32 v) { return ::RoundUpToPowerOfTwo(v); }
	constexpr ivec2 RoundToNearestPowerOfTwo(ivec2 input)
	{
		return ivec2(
			static_cast<i32>(RoundToNearestPowerOfTwo(static_cast<u32>(Max(input.x, 1)))),
			static_cast<i32>(RoundToNearestPowerOfTwo(static_cast<u32>(Max(input.y, 1)))));
	}

	enum class TextureFilterMode { Point, Linear, Cubic, Box, Triangle, };

	size_t TextureFormatBlockSize(TextureFormat format);
	size_t TextureFormatChannelCount(TextureFormat format);
	size_t TextureFormatByteSize(ivec2 size, TextureFormat format);

	// NOTE: Raw decompression routine, the output format must not be compressed
	b8 DecompressTextureData(ivec2 size, const u8* inData, TextureFormat inFormat, size_t inByteSize, u8* outData, TextureFormat outFormat, size_t outByteSize);

	// NOTE: Raw compression routine, the input format must not be compressed
	//		 While reasonably fast to compute, the output is not quite as high quallity as that of the slow NVTT compression
	b8 CompressTextureData(ivec2 size, const u8* inData, TextureFormat inFormat, size_t inByteSize, u8* outData, TextureFormat outFormat, size_t outByteSize);

	// NOTE: For internal use, usually shouldn't be called on its own.
	b8 ConvertYACbCrToRGBABuffer(const TexMipMap& mipMapYA, const TexMipMap& mipMapCbCr, u8* outData, size_t outByteSize);

	// NOTE: For internal use, usually shouldn't be called on its own. Both the YA and CbCr buffers are expected to be the same size
	b8 ConvertRGBAToYACbCrBuffer(ivec2 size, const u8* inData, TextureFormat inFormat, size_t inByteSize, u8* outYAData, u8* outCbCrData);

	b8 CreateYACbCrTexture(ivec2 size, const u8* inData, TextureFormat inFormat, size_t inByteSize, Tex& outTexture);

	// NOTE: Includes automatic checking and decoding of YACbCr RGTC2 textures
	b8 ConvertTextureToRGBABuffer(const Tex& inTexture, u8* outData, size_t outByteSize, i32 cubeFace = 0);
	std::unique_ptr<u8[]> ConvertTextureToRGBA(const Tex& inTexture, i32 cubeFace = 0);

	// NOTE: In place texture flip, in most cases it's probably more optimal to flip during reading or writing of the pixel data instead
	b8 FlipTextureBufferY(ivec2 size, u8* inOutData, TextureFormat inFormat, size_t inByteSize);

	b8 ResizeTextureBuffer(ivec2 inSize, const u8* inData, TextureFormat inFormat, size_t inByteSize, ivec2 outSize, u8* outData, size_t outByteSize, TextureFilterMode filterMode = TextureFilterMode::Linear);

	b8 ConvertRGBToRGBA(ivec2 size, const u8* inData, size_t inByteSize, u8* outData, size_t outByteSize);

	b8 LoadDDSToTexture(std::string_view filePath, Tex& outTexture);
	b8 SaveTextureToDDS(std::string_view filePath, const Tex& inTexture);
}

namespace Comfy
{
	void ExtractAllSprPNGs(std::string_view outputDirectory, const SprSet& sprSet);

	// NOTE: Defined in sort order
	enum class SprMergeType : u8 { Merge, NoMerge, Count };

	// NOTE: Defined in sort order
	enum class SprCompressionType : u8 { BC4Comp, BC5Comp, D5Comp, NoComp, UnkComp, Count };

	using SprMarkupFlags = u32;
	enum SprMarkupFlagsEnum : SprMarkupFlags
	{
		SprMarkupFlags_None = 0,
		SprMarkupFlags_NoMerge = (1 << 0),
		SprMarkupFlags_Compress = (1 << 1),
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
		TextureFormat OutputFormat;
		SprMergeType Merge;
		SprCompressionType CompressionType;
		u16 FormatTypeIndex;
		std::vector<SprMarkupBox> SpriteBoxes;
		i32 RemainingFreePixels;
	};

	struct SprPacker
	{
		struct ProgressData { u32 Sprites, SpritesTotal; };
		using ProgressCallback = std::function<void(SprPacker&, ProgressData)>;

		SprPacker() = default;
		SprPacker(ProgressCallback callback) : progressCallback(std::move(callback)) {}
		~SprPacker() = default;

		std::unique_ptr<SprSet> Create(const std::vector<SprMarkup>& sprMarkups);

		struct SettingsData
		{
			// NOTE: Set to 0xFFFF00FF for debugging but fully transparent by default to avoid cross sprite boundary block compression artifacts
			std::optional<u32> BackgroundColor = 0x00000000;

			// NOTE: Might affect texture filtering and different blend modes negatively (?)
			std::optional<u32> TransparencyColor = {};

			// NOTE: No-merge sprites with an area smaller or equal to this threshold are considered not worth compressing due to the minor space savings
			i32 NoMergeUncompressedAreaThreshold = (32 * 32);

			// NOTE: If any of the input sprites is larger than this threshold the size of the texture will be set to match that of the sprite
			ivec2 MaxTextureSize = ivec2(2048, 1024);

			// NOTE: Number of pixels at each side
			ivec2 SpritePadding = ivec2(2, 2);

			// NOTE: Generally higher quallity than block compression on its own at the cost of additional encoding and decoding time
			b8 AllowYCbCrTextures = true;

			// NOTE: Conventionally required for texture block compression as well as older graphics APIs / hardware
			b8 PowerOfTwoTextures = true;

			// NOTE: Flip to follow the OpenGL texture convention
			b8 FlipTexturesY = true;

			// NOTE: Unless the host application wants to run multiple object instances on separate threads itself...?
			b8 Multithreaded = true;
		} Settings;

	private:
		ProgressData currentProgress = {};
		ProgressCallback progressCallback;

		void ReportCurrentProgress();

		TextureFormat DetermineSprOutputFormat(const SprMarkup& sprMarkup) const;

		std::vector<SprTexMarkup> MergeTextures(const std::vector<SprMarkup>& sprMarkups);
		std::vector<const SprMarkup*> SortByArea(const std::vector<SprMarkup>& sprMarkups) const;

		std::pair<SprTexMarkup*, ivec4> FindFittingTexMarkupToPlaceSprIn(const SprMarkup& sprToPlace, TextureFormat sprOutputFormat, std::vector<SprTexMarkup>& existingTexMarkups);
		void AdjustTexMarkupSizes(std::vector<SprTexMarkup>& texMarkups) const;

		// NOTE: Theses serve no functional purpose other than to make the final output look consistent and cleaner
		void FinalTexMarkupSort(std::vector<SprTexMarkup>& texMarkups) const;
		void FinalSpriteSort(std::vector<Spr>& sprites) const;

		std::shared_ptr<Tex> CreateCompressTexFromMarkup(const SprTexMarkup& texMarkup);
		std::unique_ptr<u8[]> CreateMergedTexMarkupRGBAPixels(const SprTexMarkup& texMarkup);

		SprCompressionType GetCompressionType(TextureFormat format) const;
		cstr GetMergeName(SprMergeType merge) const;
		cstr GetCompressionName(SprCompressionType compression) const;
		std::string FormatTextureName(SprMergeType merge, SprCompressionType compression, size_t index) const;
	};
}
