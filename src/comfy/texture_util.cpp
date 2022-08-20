#include "texture_util.h"
#include "core_io.h"
#include <future>
#include <array>

#include <zlib.h>
#define STBIW_MALLOC(sz)        malloc(sz)
#define STBIW_REALLOC(p,newsz)  realloc(p,newsz)
#define STBIW_FREE(p)           free(p)
static unsigned char* CustomStbImageZLibCompress2(const unsigned char* inData, int inDataSize, int* outDataSize, int inQuality)
{
	// NOTE: If successful this buffer will be freed by stb image
	unsigned char* outBuffer = static_cast<unsigned char*>(STBIW_MALLOC(inDataSize));
	uLongf compressedSize = inDataSize;
	const int compressResult = compress2(outBuffer, &compressedSize, inData, inDataSize, inQuality);
	*outDataSize = static_cast<int>(compressedSize);
	if (compressResult != Z_OK) { STBIW_FREE(outBuffer); return nullptr; }
	return outBuffer;
}
#define STBIW_ZLIB_COMPRESS CustomStbImageZLibCompress2
#define STBIW_WINDOWS_UTF8
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_ONLY_PSD
#define STBI_ONLY_GIF
#define STBI_WINDOWS_UTF8
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace Comfy
{
	static constexpr i32 ChannelsRGBA = 4;

	b8 ReadImageFile(std::string_view filePath, ivec2& outSize, std::unique_ptr<u8[]>& outRGBAPixels)
	{
		int components;
		stbi_uc* pixels = stbi_load(filePath.data(), &outSize.x, &outSize.y, &components, ChannelsRGBA);

		// HACK: Copy could be avoided by overwriting malloc with new and then moving the pointer directly
		if (pixels != nullptr)
		{
			const size_t dataSize = (outSize.x * outSize.y * ChannelsRGBA);
			outRGBAPixels = std::make_unique<u8[]>(dataSize);
			std::memcpy(outRGBAPixels.get(), pixels, dataSize);
			stbi_image_free(pixels);
		}

		return (outRGBAPixels != nullptr);
	}

	b8 WriteImageFile(std::string_view filePath, ivec2 size, const void* rgbaPixels)
	{
		if (rgbaPixels == nullptr || size.x <= 0 || size.y <= 0)
			return false;

		const std::string_view extension = Path::GetExtension(filePath);
		const std::string nullTerminatedFilePath = std::string(filePath);

		if (ASCII::MatchesInsensitive(extension, ".bmp"))
		{
			return stbi_write_bmp(nullTerminatedFilePath.c_str(), size.x, size.y, ChannelsRGBA, rgbaPixels);
		}
		else if (ASCII::MatchesInsensitive(extension, ".png"))
		{
			// static constexpr i32 bitsPerPixel = ChannelsRGBA * BitsPerByte;
			// const i32 pixelsPerLine = (size.x % bitsPerPixel != 0) ? (size.x + (bitsPerPixel - (size.x % bitsPerPixel))) : (size.x);

			// NOTE: For simplicity sake proper 16 byte stride alignment will be ignored for now
			const i32 pixelsPerLine = size.x;

			return stbi_write_png(nullTerminatedFilePath.c_str(), size.x, size.y, ChannelsRGBA, rgbaPixels, pixelsPerLine * ChannelsRGBA);
		}
		else
		{
			assert(false);
			return false;
		}
	}
}

#include <Windows.h>
#include <DirectXTex.h>

namespace Comfy
{
	static constexpr DXGI_FORMAT TextureFormatToDXGI(TextureFormat format)
	{
		switch (format)
		{
		case TextureFormat::A8: return DXGI_FORMAT_R8_UINT;
		case TextureFormat::RGB8: return DXGI_FORMAT_UNKNOWN;
		case TextureFormat::RGBA8: return DXGI_FORMAT_R8G8B8A8_UNORM;
		case TextureFormat::RGB5: return DXGI_FORMAT_UNKNOWN;
		case TextureFormat::RGB5_A1: return DXGI_FORMAT_UNKNOWN;
		case TextureFormat::RGBA4: return DXGI_FORMAT_UNKNOWN;
		case TextureFormat::DXT1: return DXGI_FORMAT_BC1_UNORM;
		case TextureFormat::DXT1a: return DXGI_FORMAT_BC1_UNORM_SRGB;
		case TextureFormat::DXT3: return DXGI_FORMAT_BC2_UNORM;
		case TextureFormat::DXT5: return DXGI_FORMAT_BC3_UNORM;
		case TextureFormat::RGTC1: return DXGI_FORMAT_BC4_UNORM;
		case TextureFormat::RGTC2: return DXGI_FORMAT_BC5_UNORM;
		case TextureFormat::L8: return DXGI_FORMAT_A8_UNORM;
		case TextureFormat::L8A8: return DXGI_FORMAT_A8P8;
		default: return DXGI_FORMAT_UNKNOWN;
		}
	}

	static constexpr TextureFormat DXGIFormatToTextureFormat(DXGI_FORMAT format)
	{
		switch (format)
		{
		case DXGI_FORMAT_R8_UINT: return TextureFormat::A8;
		case DXGI_FORMAT_R8G8B8A8_UNORM: return TextureFormat::RGBA8;
		case DXGI_FORMAT_BC1_UNORM: return TextureFormat::DXT1;
		case DXGI_FORMAT_BC1_UNORM_SRGB: return TextureFormat::DXT1a;
		case DXGI_FORMAT_BC2_UNORM: return TextureFormat::DXT3;
		case DXGI_FORMAT_BC3_UNORM: return TextureFormat::DXT5;
		case DXGI_FORMAT_BC4_UNORM: return TextureFormat::RGTC1;
		case DXGI_FORMAT_BC5_UNORM: return TextureFormat::RGTC2;
		case DXGI_FORMAT_A8_UNORM: return TextureFormat::L8;
		case DXGI_FORMAT_A8P8: return TextureFormat::L8A8;
		default: return TextureFormat::Unknown;
		}
	}

	static constexpr ::DirectX::TEX_FILTER_FLAGS FilterModeToTexFilterFlags(TextureFilterMode filterMode)
	{
		switch (filterMode)
		{
		case TextureFilterMode::Point: return ::DirectX::TEX_FILTER_POINT;
		case TextureFilterMode::Linear: return ::DirectX::TEX_FILTER_LINEAR;
		case TextureFilterMode::Cubic: return ::DirectX::TEX_FILTER_CUBIC;
		case TextureFilterMode::Box: return ::DirectX::TEX_FILTER_BOX;
		case TextureFilterMode::Triangle: return ::DirectX::TEX_FILTER_TRIANGLE;
		default: return ::DirectX::TEX_FILTER_DEFAULT;
		}
	}

	size_t TextureFormatBlockSize(TextureFormat format)
	{
		switch (format)
		{
		case TextureFormat::DXT1:
		case TextureFormat::DXT1a:
		case TextureFormat::RGTC1:
			return 8;

		case TextureFormat::DXT3:
		case TextureFormat::DXT5:
		case TextureFormat::RGTC2:
			return 16;

		default:
			return 0;
		}
	}

	size_t TextureFormatChannelCount(TextureFormat format)
	{
		switch (format)
		{
		case TextureFormat::A8:
		case TextureFormat::L8:
		case TextureFormat::RGTC1:
			return 1;

		case TextureFormat::L8A8:
		case TextureFormat::RGTC2:
			return 2;

		case TextureFormat::RGB8:
		case TextureFormat::RGB5:
		case TextureFormat::DXT1:
			return 3;

		case TextureFormat::RGBA8:
		case TextureFormat::RGB5_A1:
		case TextureFormat::RGBA4:
		case TextureFormat::DXT1a:
		case TextureFormat::DXT3:
		case TextureFormat::DXT5:
			return 4;

		default:
			assert(false);
			return 0;
		}
	}

	size_t TextureFormatByteSize(ivec2 size, TextureFormat format)
	{
		switch (format)
		{
		case TextureFormat::A8:
		case TextureFormat::L8:
		case TextureFormat::RGB8:
		case TextureFormat::RGBA8:
		case TextureFormat::L8A8:
			return size.x * size.y * TextureFormatChannelCount(format);

			// BUG: Not entirely accurate but not like these are used anyway
		case TextureFormat::RGB5:
			assert(false);
			return (size.x * size.y * 15) / CHAR_BIT;
		case TextureFormat::RGB5_A1:
			assert(false);
			return (size.x * size.y * 16) / CHAR_BIT;

		case TextureFormat::RGBA4:
			return (size.x * size.y * TextureFormatChannelCount(format)) / 2;

		case TextureFormat::DXT1:
		case TextureFormat::DXT1a:
		case TextureFormat::DXT3:
		case TextureFormat::DXT5:
		case TextureFormat::RGTC1:
		case TextureFormat::RGTC2:
			return Max(1, (size.x + 3) / 4) * Max(1, (size.y + 3) / 4) * TextureFormatBlockSize(format);

		default:
			assert(false);
			return 0;
		}
	}

	b8 DecompressTextureData(ivec2 size, const u8* inData, TextureFormat inFormat, size_t inByteSize, u8* outData, TextureFormat outFormat, size_t outByteSize)
	{
		if (size.x <= 0 || size.y <= 0)
			return false;

		if (inData == nullptr || outData == nullptr)
			return false;

		const auto expectedInputByteSize = TextureFormatByteSize(size, inFormat);
		if (inByteSize < expectedInputByteSize)
			return false;

		if (inFormat == TextureFormat::RGB8 && outFormat == TextureFormat::RGBA8)
			return ConvertRGBToRGBA(size, inData, inByteSize, outData, outByteSize);

		const auto inFormatDXGI = TextureFormatToDXGI(inFormat);
		const auto outFormatDXGI = TextureFormatToDXGI(outFormat);

		if (inFormatDXGI == DXGI_FORMAT_UNKNOWN || outFormatDXGI == DXGI_FORMAT_UNKNOWN)
			return false;

		if (inFormat == outFormat)
		{
			std::memcpy(outData, inData, expectedInputByteSize);
			return true;
		}

		auto inputImage = ::DirectX::Image {};
		inputImage.width = size.x;
		inputImage.height = size.y;
		inputImage.format = inFormatDXGI;
		inputImage.rowPitch = TextureFormatByteSize(ivec2(size.x, 1), inFormat);
		inputImage.slicePitch = expectedInputByteSize;
		inputImage.pixels = const_cast<u8*>(inData);

		auto outputImage = ::DirectX::ScratchImage {};
		if (FAILED(::DirectX::Decompress(inputImage, outFormatDXGI, outputImage)))
			return false;

		if (outByteSize < outputImage.GetPixelsSize())
			return false;

		std::memcpy(outData, outputImage.GetPixels(), outputImage.GetPixelsSize());
		return true;
	}

	b8 CompressTextureData(ivec2 size, const u8* inData, TextureFormat inFormat, size_t inByteSize, u8* outData, TextureFormat outFormat, size_t outByteSize)
	{
		if (size.x <= 0 || size.y <= 0)
			return false;

		if (inData == nullptr || outData == nullptr)
			return false;

		const auto expectedInputByteSize = TextureFormatByteSize(size, inFormat);
		if (inByteSize < expectedInputByteSize)
			return false;

		const auto inFormatDXGI = TextureFormatToDXGI(inFormat);
		const auto outFormatDXGI = TextureFormatToDXGI(outFormat);

		if (inFormatDXGI == DXGI_FORMAT_UNKNOWN || outFormatDXGI == DXGI_FORMAT_UNKNOWN)
			return false;

		auto inputImage = ::DirectX::Image {};
		inputImage.width = size.x;
		inputImage.height = size.y;
		inputImage.format = inFormatDXGI;
		inputImage.rowPitch = TextureFormatByteSize(ivec2(size.x, 1), inFormat);
		inputImage.slicePitch = expectedInputByteSize;
		inputImage.pixels = const_cast<u8*>(inData);

		auto compressionFlags = ::DirectX::TEX_COMPRESS_DEFAULT;

		if (true) // NOTE: It seems this yields the best results in most cases
			compressionFlags |= ::DirectX::TEX_COMPRESS_DITHER;

		auto outputImage = ::DirectX::ScratchImage {};
		if (FAILED(::DirectX::Compress(inputImage, outFormatDXGI, compressionFlags, ::DirectX::TEX_THRESHOLD_DEFAULT, outputImage)))
			return false;

		if (outByteSize < outputImage.GetPixelsSize())
			return false;

		std::memcpy(outData, outputImage.GetPixels(), outputImage.GetPixelsSize());
		return true;
	}

	static constexpr f32 CbCrOffset = 0.503929f;
	static constexpr f32 CbCrFactor = 1.003922f;

	static constexpr mat3 YCbCrToRGBTransform =
	{
		vec3(+1.5748f, +1.0f, +0.0000f),
		vec3(-0.4681f, +1.0f, -0.1873f),
		vec3(+0.0000f, +1.0f, +1.8556f),
	};

	static constexpr mat3 RGBToYCbCrTransform =
	{
		vec3(+0.500004232f, -0.454162151f, -0.0458420813f),
		vec3(+0.212593317f, +0.715214610f, +0.0721921176f),
		vec3(-0.114568502f, -0.385435730f, +0.5000042320f),
	};

	static constexpr float PixelU8ToF32(u8 pixel)
	{
		constexpr f32 factor = 1.0f / static_cast<f32>(U8Max);
		return static_cast<f32>(pixel) * factor;
	}

	static constexpr u8 PixelF32ToU8(f32 pixel)
	{
		constexpr f32 factor = static_cast<f32>(U8Max);
		return static_cast<u8>(Clamp(pixel, 0.0f, 1.0f) * factor);
	}

	static constexpr u32 PackU8RGBA(u8 r, u8 g, u8 b, u8 a)
	{
		return (static_cast<u32>(a) << 24) | (static_cast<u32>(b) << 16) | (static_cast<u32>(g) << 8) | (static_cast<u32>(r) << 0);
	}

	static constexpr u32 ConvertSinglePixelYACbCrToRGBA(const u8 inYA[2], const u8 inCbCr[2])
	{
		const vec3 yCbCr = vec3(
			(PixelU8ToF32(inCbCr[1]) * CbCrFactor) - CbCrOffset,
			(PixelU8ToF32(inYA[0])),
			(PixelU8ToF32(inCbCr[0]) * CbCrFactor) - CbCrOffset);

		return PackU8RGBA(
			PixelF32ToU8(Dot(yCbCr, YCbCrToRGBTransform[0])),
			PixelF32ToU8(Dot(yCbCr, YCbCrToRGBTransform[1])),
			PixelF32ToU8(Dot(yCbCr, YCbCrToRGBTransform[2])),
			inYA[1]);
	}

	static constexpr void ConvertSinglePixelRGBAToYACbCr(const u32 inRGBA, u8 outYA[2], u8 outCbCr[2])
	{
		constexpr f32 cbCrFactor = 1.0f / CbCrFactor;

		const vec3 rgb = vec3(
			PixelU8ToF32((inRGBA & 0x0000FF)),
			PixelU8ToF32((inRGBA & 0x00FF00) >> 8),
			PixelU8ToF32((inRGBA & 0xFF0000) >> 16));

		outCbCr[0] = PixelF32ToU8((Dot(rgb, RGBToYCbCrTransform[2]) + CbCrOffset) * cbCrFactor);
		outCbCr[1] = PixelF32ToU8((Dot(rgb, RGBToYCbCrTransform[0]) + CbCrOffset) * cbCrFactor);

		outYA[0] = PixelF32ToU8(Dot(rgb, RGBToYCbCrTransform[1]));
		outYA[1] = static_cast<u8>(inRGBA >> 24);
	}

	b8 ConvertYACbCrToRGBABuffer(const TexMipMap& mipMapYA, const TexMipMap& mipMapCbCr, u8* outData, size_t outByteSize)
	{
		if (mipMapYA.Format != TextureFormat::RGTC2 || mipMapCbCr.Format != TextureFormat::RGTC2 || (mipMapYA.Size / 2) != mipMapCbCr.Size)
			return false;

		if (outByteSize < TextureFormatByteSize(mipMapYA.Size, TextureFormat::RGBA8))
			return false;

		auto imageFromMip = [](const auto& mip)
		{
			auto image = ::DirectX::Image {};
			image.width = mip.Size.x;
			image.height = mip.Size.y;
			image.format = DXGI_FORMAT_BC5_UNORM;
			image.rowPitch = TextureFormatByteSize(ivec2(mip.Size.x, 1), TextureFormat::RGTC2);
			image.slicePitch = mip.DataSize;
			image.pixels = const_cast<u8*>(mip.Data.get());
			return image;
		};

		auto outputImageYA = ::DirectX::ScratchImage {};
		if (FAILED(::DirectX::Decompress(imageFromMip(mipMapYA), DXGI_FORMAT_R8G8_UNORM, outputImageYA)))
			return false;

		auto outputImageCbCr = ::DirectX::ScratchImage {};
		if (FAILED(::DirectX::Decompress(imageFromMip(mipMapCbCr), DXGI_FORMAT_R8G8_UNORM, outputImageCbCr)))
			return false;

		auto outputImageCbCrResized = ::DirectX::ScratchImage {};
		if (FAILED(::DirectX::Resize(*outputImageCbCr.GetImage(0, 0, 0), mipMapYA.Size.x, mipMapYA.Size.y, ::DirectX::TEX_FILTER_LINEAR | ::DirectX::TEX_FILTER_FORCE_NON_WIC, outputImageCbCrResized)))
			return false;

		const u8* pixelBufferYA = (outputImageYA.GetPixels());
		const u8* pixelBufferCbCrResized = (outputImageCbCrResized.GetPixels());
		u32* outRGBA = reinterpret_cast<u32*>(outData);

		for (size_t y = 0; y < mipMapYA.Size.y; y++)
		{
			for (size_t x = 0; x < mipMapYA.Size.x; x++)
			{
				const auto pixelIndex = (mipMapYA.Size.x * y + x);

				const u8* inYA = &pixelBufferYA[pixelIndex * 2];
				const u8* inCbCr = &pixelBufferCbCrResized[pixelIndex * 2];

				outRGBA[pixelIndex] = ConvertSinglePixelYACbCrToRGBA(inYA, inCbCr);
			}
		}

		return true;
	}

	b8 ConvertRGBAToYACbCrBuffer(ivec2 size, const u8* inData, TextureFormat inFormat, size_t inByteSize, u8* outYAData, u8* outCbCrData)
	{
		if (inFormat != TextureFormat::RGBA8)
			return false;

		const auto inRGBAData = reinterpret_cast<const u32*>(inData);

		for (size_t y = 0; y < size.y; y++)
		{
			for (size_t x = 0; x < size.x; x++)
			{
				const auto pixelIndex = (size.x * y + x);

				u8* outYA = &outYAData[pixelIndex * 2];
				u8* outCbCr = &outCbCrData[pixelIndex * 2];

				ConvertSinglePixelRGBAToYACbCr(inRGBAData[pixelIndex], outYA, outCbCr);
			}
		}

		return true;
	}

	b8 CreateYACbCrTexture(ivec2 size, const u8* inData, TextureFormat inFormat, size_t inByteSize, Tex& outTexture)
	{
		if (size.x <= 0 || size.y <= 0)
			return false;

		if (inFormat != TextureFormat::RGBA8)
			return false;

		const auto fullSize = size;
		const auto halfSize = size / 2;

		auto yaBuffer = std::make_unique<u8[]>(fullSize.x * fullSize.y * 2);
		auto fullCbCrBuffer = std::make_unique<u8[]>(fullSize.x * fullSize.y * 2);

		if (!ConvertRGBAToYACbCrBuffer(fullSize, inData, inFormat, inByteSize, yaBuffer.get(), fullCbCrBuffer.get()))
			return false;

		auto fullCbCr = ::DirectX::Image {};
		fullCbCr.width = fullSize.x;
		fullCbCr.height = fullSize.y;
		fullCbCr.format = DXGI_FORMAT_R8G8_UNORM;
		fullCbCr.rowPitch = fullSize.x * 2;
		fullCbCr.slicePitch = fullSize.x * fullSize.y * 2;
		fullCbCr.pixels = fullCbCrBuffer.get();

		auto halfCbCr = ::DirectX::ScratchImage {};
		if (FAILED(::DirectX::Resize(fullCbCr, halfSize.x, halfSize.y, ::DirectX::TEX_FILTER_LINEAR | ::DirectX::TEX_FILTER_FORCE_NON_WIC, halfCbCr)))
			return false;

		auto halfCbCrSource = ::DirectX::Image {};
		halfCbCrSource.width = halfSize.x;
		halfCbCrSource.height = halfSize.y;
		halfCbCrSource.format = DXGI_FORMAT_R8G8_UNORM;
		halfCbCrSource.rowPitch = halfSize.x * 2;
		halfCbCrSource.slicePitch = halfSize.x * halfSize.y * 2;
		halfCbCrSource.pixels = halfCbCr.GetPixels();

		auto compressedHalfCbCr = ::DirectX::ScratchImage {};
		if (FAILED(::DirectX::Compress(halfCbCrSource, DXGI_FORMAT_BC5_UNORM, ::DirectX::TEX_COMPRESS_UNIFORM, ::DirectX::TEX_THRESHOLD_DEFAULT, compressedHalfCbCr)))
			return false;

		auto yaSource = ::DirectX::Image {};
		yaSource.width = fullSize.x;
		yaSource.height = fullSize.y;
		yaSource.format = DXGI_FORMAT_R8G8_UNORM;
		yaSource.rowPitch = fullSize.x * 2;
		yaSource.slicePitch = fullSize.x * fullSize.y * 2;
		yaSource.pixels = yaBuffer.get();

		auto compressedYA = ::DirectX::ScratchImage {};
		if (FAILED(::DirectX::Compress(yaSource, DXGI_FORMAT_BC5_UNORM, ::DirectX::TEX_COMPRESS_UNIFORM, ::DirectX::TEX_THRESHOLD_DEFAULT, compressedYA)))
			return false;

		outTexture.MipMapsArray.resize(1);
		auto& mipMaps = outTexture.MipMapsArray.front();

		mipMaps.resize(2);
		auto& mipMapYA = mipMaps[0];
		auto& mipMapCbCr = mipMaps[1];

		mipMapYA.Size = fullSize;
		mipMapCbCr.Size = halfSize;

		for (auto& mip : mipMaps)
		{
			mip.Format = TextureFormat::RGTC2;
			mip.DataSize = static_cast<u32>(TextureFormatByteSize(mip.Size, TextureFormat::RGTC2));
			mip.Data = std::make_unique<u8[]>(mip.DataSize);
		}

		std::memcpy(mipMapYA.Data.get(), compressedYA.GetPixels(), mipMapYA.DataSize);
		std::memcpy(mipMapCbCr.Data.get(), compressedHalfCbCr.GetPixels(), mipMapCbCr.DataSize);

		return true;
	}

	b8 ConvertTextureToRGBABuffer(const Tex& inTexture, u8* outData, size_t outByteSize, i32 cubeFace)
	{
		if (inTexture.MipMapsArray.empty() || cubeFace >= inTexture.MipMapsArray.size())
			return false;

		const auto& mips = inTexture.MipMapsArray[cubeFace];
		if (mips.empty())
			return false;

		const auto& frontMip = mips.front();
		const b8 decodeYACbCr = (inTexture.GetSignature() == TxpSig::Texture2D && mips.size() == 2 && frontMip.Format == TextureFormat::RGTC2);

		if (decodeYACbCr)
			return ConvertYACbCrToRGBABuffer(mips[0], mips[1], outData, outByteSize);

		return DecompressTextureData(frontMip.Size, frontMip.Data.get(), frontMip.Format, frontMip.DataSize, outData, TextureFormat::RGBA8, outByteSize);
	}

	std::unique_ptr<u8[]> ConvertTextureToRGBA(const Tex& inTexture, i32 cubeFace)
	{
		const auto outByteSize = TextureFormatByteSize(inTexture.GetSize(), TextureFormat::RGBA8);
		auto outData = std::make_unique<u8[]>(outByteSize);

		if (!ConvertTextureToRGBABuffer(inTexture, outData.get(), outByteSize, cubeFace))
			return nullptr;

		return outData;
	}

	b8 FlipTextureBufferY(ivec2 size, u8* inOutData, TextureFormat inFormat, size_t inByteSize)
	{
		if (size.x <= 0 || size.y <= 0)
			return false;

		if (inFormat != TextureFormat::RGBA8)
			return false;

		if (inByteSize < TextureFormatByteSize(size, inFormat))
			return false;

		auto inOutRGBAPixels = reinterpret_cast<u32*>(inOutData);

		for (auto y = 0; y < size.y / 2; y++)
		{
			for (auto x = 0; x < size.x; x++)
			{
				u32& pixel = inOutRGBAPixels[size.x * y + x];
				u32& flippedPixel = inOutRGBAPixels[(size.x * (size.y - 1 - y)) + x];

				std::swap(pixel, flippedPixel);
			}
		}

		return true;
	}

	b8 ResizeTextureBuffer(ivec2 inSize, const u8* inData, TextureFormat inFormat, size_t inByteSize, ivec2 outSize, u8* outData, size_t outByteSize, TextureFilterMode filterMode)
	{
		if (inSize.x <= 0 || inSize.y <= 0 || outSize.x <= 0 || outSize.y <= 0)
			return false;

		const auto expectedInputByteSize = TextureFormatByteSize(inSize, inFormat);
		if (inByteSize < expectedInputByteSize)
			return false;

		const auto expectedOutputByteSize = TextureFormatByteSize(outSize, inFormat);
		if (outByteSize < expectedOutputByteSize)
			return false;

		const auto inOutFormatDXGI = TextureFormatToDXGI(inFormat);
		if (inOutFormatDXGI == DXGI_FORMAT_UNKNOWN)
			return false;

		auto sourceImage = ::DirectX::Image {};
		sourceImage.width = inSize.x;
		sourceImage.height = inSize.y;
		sourceImage.format = inOutFormatDXGI;
		sourceImage.rowPitch = TextureFormatByteSize(ivec2(inSize.x, 1), inFormat);
		sourceImage.slicePitch = expectedInputByteSize;
		sourceImage.pixels = const_cast<u8*>(inData);

		const auto filterFlags = FilterModeToTexFilterFlags(filterMode) | ::DirectX::TEX_FILTER_FORCE_NON_WIC;

		auto resizedImage = ::DirectX::ScratchImage {};
		if (FAILED(::DirectX::Resize(sourceImage, outSize.x, outSize.y, filterFlags, resizedImage)))
			return false;

		std::memcpy(outData, resizedImage.GetPixels(), expectedOutputByteSize);
		return true;
	}

	b8 ConvertRGBToRGBA(ivec2 size, const u8* inData, size_t inByteSize, u8* outData, size_t outByteSize)
	{
		if (size.x <= 0 || size.y <= 0)
			return false;

		const auto expectedInputByteSize = TextureFormatByteSize(size, TextureFormat::RGB8);
		if (inByteSize < expectedInputByteSize)
			return false;

		const auto expectedOutputByteSize = TextureFormatByteSize(size, TextureFormat::RGBA8);
		if (outByteSize < expectedOutputByteSize)
			return false;

		const u8* currentRGBPixel = inData;
		u32* outRGBAPixel = reinterpret_cast<u32*>(outData);

		for (size_t i = 0; i < (size.x * size.y); i++)
		{
			const u8 r = currentRGBPixel[0];
			const u8 g = currentRGBPixel[1];
			const u8 b = currentRGBPixel[2];
			const u8 a = U8Max;
			currentRGBPixel += 3;

			outRGBAPixel[i] = (r << 0) | (g << 8) | (b << 16) | (a << 24);
		}

		return true;
	}

	b8 LoadDDSToTexture(std::string_view filePath, Tex& outTexture)
	{
		auto outMetadata = ::DirectX::TexMetadata {};
		auto outImage = ::DirectX::ScratchImage {};

		if (FAILED(::DirectX::LoadFromDDSFile(UTF8::WideArg(filePath).c_str(), ::DirectX::DDS_FLAGS_NONE, &outMetadata, outImage)))
			return false;

		outTexture.Name = std::string(Path::GetFileName(filePath, false));
		outTexture.MipMapsArray.resize(outMetadata.arraySize);

		for (size_t arrayIndex = 0; arrayIndex < outTexture.MipMapsArray.size(); arrayIndex++)
		{
			auto& mipMaps = outTexture.MipMapsArray[arrayIndex];
			mipMaps.resize(outMetadata.mipLevels);

			for (size_t mipIndex = 0; mipIndex < mipMaps.size(); mipIndex++)
			{
				const auto* image = outImage.GetImage(mipIndex, arrayIndex, 0);

				auto& mip = mipMaps[mipIndex];
				mip.Size = ivec2(static_cast<i32>(image->width), static_cast<i32>(image->height));
				mip.Format = DXGIFormatToTextureFormat(image->format);

				if (mip.Format == TextureFormat::Unknown)
					return false;

				mip.DataSize = static_cast<u32>(image->slicePitch);
				mip.Data = std::make_unique<u8[]>(mip.DataSize);
				std::memcpy(mip.Data.get(), image->pixels, mip.DataSize);
			}
		}

		return true;
	}

	b8 SaveTextureToDDS(std::string_view filePath, const Tex& inTexture)
	{
		if (inTexture.MipMapsArray.empty() || inTexture.MipMapsArray.front().empty())
			return false;

		const auto imageCount = inTexture.MipMapsArray.size() * inTexture.MipMapsArray.front().size();
		if (imageCount < 1)
			return false;

		auto images = std::make_unique<::DirectX::Image[]>(imageCount);
		auto imageWriteHead = images.get();

		for (const auto& mipMaps : inTexture.MipMapsArray)
		{
			for (auto& mip : mipMaps)
			{
				imageWriteHead->width = mip.Size.x;
				imageWriteHead->height = mip.Size.y;
				imageWriteHead->format = TextureFormatToDXGI(mip.Format);
				imageWriteHead->rowPitch = TextureFormatByteSize(ivec2(mip.Size.x, 1), mip.Format);
				imageWriteHead->slicePitch = TextureFormatByteSize(mip.Size, mip.Format);
				imageWriteHead->pixels = const_cast<u8*>(mip.Data.get());
				imageWriteHead++;
			}
		}

		auto metadata = ::DirectX::TexMetadata {};
		metadata.width = images[0].width;
		metadata.height = images[0].height;
		metadata.depth = 1;
		metadata.arraySize = inTexture.MipMapsArray.size();
		metadata.mipLevels = inTexture.MipMapsArray.front().size();
		metadata.format = images[0].format;
		metadata.dimension = ::DirectX::TEX_DIMENSION_TEXTURE2D;

		if (FAILED(::DirectX::SaveToDDSFile(images.get(), imageCount, metadata, ::DirectX::DDS_FLAGS_NONE, UTF8::WideArg(filePath).c_str())))
			return false;

		return true;
	}
}

namespace Comfy
{
	static constexpr size_t SpritePixelCount(const Spr& spr)
	{
		return static_cast<size_t>(spr.PixelRegion.z) * static_cast<size_t>(spr.PixelRegion.w);
	}

	static size_t SumTotalSpritePixels(const SprSet& sprSet)
	{
		size_t totalPixels = 0;
		for (const auto& spr : sprSet.Sprites)
			totalPixels += SpritePixelCount(spr);
		return totalPixels;
	}

	static constexpr b8 SpriteFitsInTexture(ivec2 sprPos, ivec2 sprSize, ivec2 texSize)
	{
		return (sprPos.x >= 0 && sprPos.x + sprSize.x <= texSize.x && sprPos.y >= 0 && sprPos.y + sprSize.y <= texSize.y);
	}

	void ExtractAllSprPNGs(std::string_view outputDirectory, const SprSet& sprSet)
	{
		if (sprSet.Sprites.empty() || sprSet.TexSet.Textures.empty())
			return;

		const auto& textures = sprSet.TexSet.Textures;
		const auto& sprites = sprSet.Sprites;

		const auto rgbaTextures = std::make_unique<std::unique_ptr<u8[]>[]>(textures.size());
		const auto textureFutures = std::make_unique<std::future<void>[]>(textures.size());

		for (size_t i = 0; i < textures.size(); i++)
		{
			textureFutures[i] = std::async(std::launch::async, [&, i]
			{
				auto& inTexture = *textures[i];
				auto& outRGBA = rgbaTextures[i];

				const auto rgbaByteSize = TextureFormatByteSize(inTexture.GetSize(), TextureFormat::RGBA8);
				outRGBA = std::make_unique<u8[]>(rgbaByteSize);

				if (!ConvertTextureToRGBABuffer(inTexture, outRGBA.get(), rgbaByteSize))
					outRGBA = nullptr;
			});
		}

		// NOTE: While the textures are still being decompressed in the background start allocating a large memory region to be and split up and used by each sprite
		const auto combinedSprPixelsBuffer = std::make_unique<u32[]>(SumTotalSpritePixels(sprSet));
		const auto spriteFutures = std::make_unique<std::future<void>[]>(sprites.size());

		for (size_t i = 0; i < textures.size(); i++)
			textureFutures[i].wait();

		for (size_t i = 0, pixelIndex = 0; i < sprites.size(); i++)
		{
			u32* sprRGBA = &combinedSprPixelsBuffer[pixelIndex];
			pixelIndex += SpritePixelCount(sprites[i]);

			spriteFutures[i] = std::async(std::launch::async, [&, sprIndex = i, sprRGBA]
			{
				const auto& spr = sprites[sprIndex];
				if (!InBounds(spr.TextureIndex, textures))
					return;

				const ivec2 sprPos = ivec2(static_cast<i32>(spr.PixelRegion.x), static_cast<i32>(spr.PixelRegion.y));
				const ivec2 sprSize = ivec2(static_cast<i32>(spr.GetSize().x), static_cast<i32>(spr.GetSize().y));
				const ivec2 texSize = textures[spr.TextureIndex]->GetSize();

				if (!SpriteFitsInTexture(sprPos, sprSize, texSize))
					return;

				const u32* texRGBA = reinterpret_cast<const u32*>(rgbaTextures[spr.TextureIndex].get());
				if (texRGBA == nullptr)
					return;

				for (size_t y = 0; y < sprSize.y; y++)
				{
					for (size_t x = 0; x < sprSize.x; x++)
					{
						// NOTE: Also perform texture Y flip
						const u32 texPixel = texRGBA[texSize.x * ((texSize.y - 1) - y - sprPos.y) + (x + sprPos.x)];

						sprRGBA[sprSize.x * y + x] = texPixel;
					}
				}

				const auto fileName = ASCII::ToLowerCopy(spr.Name) + ".png";
				const auto filePath = Path::Combine(outputDirectory, fileName);

				WriteImageFile(filePath, sprSize, sprRGBA);
			});
		}

		for (size_t i = 0; i < sprites.size(); i++)
			spriteFutures[i].wait();
	}

	static constexpr size_t RGBABytesPerPixel = 4;

	static constexpr i32 Area(const ivec2& size)
	{
		return (size.x * size.y);
	}

	static constexpr b8 Intersects(const ivec4& boxA, const ivec4& boxB)
	{
		return
			(boxB.x < boxA.x + boxA.z) && (boxA.x < (boxB.x + boxB.z)) &&
			(boxB.y < boxA.y + boxA.w) && (boxA.y < boxB.y + boxB.w);
	}

	static constexpr b8 Contains(const ivec4& boxA, const ivec4& boxB)
	{
		return
			(boxA.x <= boxB.x) && ((boxB.x + boxB.z) <= (boxA.x + boxA.z)) &&
			(boxA.y <= boxB.y) && ((boxB.y + boxB.w) <= (boxA.y + boxA.w));
	}

	static constexpr i32 GetBoxRight(const ivec4& box)
	{
		return box.x + box.z;
	}

	static constexpr i32 GetBoxBottom(const ivec4& box)
	{
		return box.y + box.w;
	}

	static constexpr ivec2 GetBoxPos(const ivec4& box)
	{
		return ivec2(box.x, box.y);
	}

	static constexpr ivec2 GetBoxSize(const ivec4& box)
	{
		return ivec2(box.z, box.w);
	}

	static b8 FitsInsideTexture(const ivec4& textureBox, const std::vector<SprMarkupBox>& existingSprites, const ivec4& spriteBox)
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

	static constexpr vec4 GetTexelRegionFromPixelRegion(const vec4& spritePixelRegion, vec2 textureAtlasSize)
	{
		const vec4 texelRegion =
		{
#if 0
				(spritePixelRegion.x / textureAtlasSize.x),
				(spritePixelRegion.y / textureAtlasSize.y),
				(spritePixelRegion.z / textureAtlasSize.x),
				(spritePixelRegion.w / textureAtlasSize.y),
#else
				(spritePixelRegion.x / textureAtlasSize.x),
				(spritePixelRegion.y / textureAtlasSize.y),
				((spritePixelRegion.x + spritePixelRegion.z) / textureAtlasSize.x),
				((spritePixelRegion.y + spritePixelRegion.w) / textureAtlasSize.y),
#endif
		};
		return texelRegion;
	}

	static inline u32& GetPixel(i32 width, void* rgbaPixels, i32 x, i32 y)
	{
		return reinterpret_cast<u32*>(rgbaPixels)[(width * y) + x];
	}

	static inline const u32& GetPixel(i32 width, const void* rgbaPixels, i32 x, i32 y)
	{
		return reinterpret_cast<const u32*>(rgbaPixels)[(width * y) + x];
	}

	static void CopySprIntoTex(const SprTexMarkup& texMarkup, void* texData, const SprMarkupBox& sprBox)
	{
		const auto texSize = texMarkup.Size;

		const auto sprSize = sprBox.Markup->Size;
		const auto sprBoxSize = GetBoxSize(sprBox.Box);

		const auto sprPadding = (sprBoxSize - sprSize) / 2;
		const auto sprOffset = GetBoxPos(sprBox.Box) + sprPadding;

		const void* sprData = sprBox.Markup->RGBAPixels;

		if (sprPadding.x > 0 && sprPadding.y > 0 && sprSize.x > 0 && sprSize.y > 0)
		{
			const auto cornerTopLeft = GetBoxPos(sprBox.Box);
			const auto cornerBottomRight = cornerTopLeft + sprBoxSize - sprPadding;
			for (i32 x = 0; x < sprPadding.x; x++)
			{
				for (i32 y = 0; y < sprPadding.y; y++)
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
			for (i32 x = sprPadding.x; x < sprBoxSize.x - sprPadding.x; x++)
			{
				for (i32 y = 0; y < sprPadding.y; y++)
					GetPixel(texSize.x, texData, x + sprBox.Box.x, y + sprBox.Box.y) = GetPixel(sprSize.x, sprData, x - sprPadding.x, 0);
				for (i32 y = sprBoxSize.y - sprPadding.y; y < sprBoxSize.y; y++)
					GetPixel(texSize.x, texData, x + sprBox.Box.x, y + sprBox.Box.y) = GetPixel(sprSize.x, sprData, x - sprPadding.x, sprSize.y - 1);
			}
			for (i32 y = sprPadding.y; y < sprBoxSize.y - sprPadding.y; y++)
			{
				for (i32 x = 0; x < sprPadding.x; x++)
					GetPixel(texSize.x, texData, x + sprBox.Box.x, y + sprBox.Box.y) = GetPixel(sprSize.x, sprData, 0, y - sprPadding.y);
				for (i32 x = sprBoxSize.x - sprPadding.x; x < sprBoxSize.x; x++)
					GetPixel(texSize.x, texData, x + sprBox.Box.x, y + sprBox.Box.y) = GetPixel(sprSize.x, sprData, sprSize.x - 1, y - sprPadding.y);
			}
		}

		for (i32 y = 0; y < sprSize.y; y++)
		{
			for (i32 x = 0; x < sprSize.x; x++)
			{
				const u32& sprPixel = GetPixel(sprSize.x, sprData, x, y);
				u32& texPixel = GetPixel(texSize.x, texData, x + sprOffset.x, y + sprOffset.y);

				texPixel = sprPixel;
			}
		}
	}

	static void SetPixelsUniformTransparency(ivec2 size, void* rgbaPixels, u32 transparencyColor)
	{
		for (i32 y = 0; y < size.y; y++)
		{
			for (i32 x = 0; x < size.x; x++)
			{
				u32& pixel = GetPixel(size.x, rgbaPixels, x, y);
				const b8 isFullyTransparent = ((pixel >> 24) & 0xFF) == 0x00;

				if (isFullyTransparent)
					pixel = transparencyColor;
			}
		}
	}

	static void FlipTextureY(ivec2 size, void* rgbaPixels)
	{
		for (i32 y = 0; y < size.y / 2; y++)
		{
			for (i32 x = 0; x < size.x; x++)
			{
				u32& pixel = GetPixel(size.x, rgbaPixels, x, y);
				u32& flippedPixel = GetPixel(size.x, rgbaPixels, x, size.y - 1 - y);

				std::swap(pixel, flippedPixel);
			}
		}
	}

	static constexpr b8 MakesUseOfAlphaChannel(const SprMarkup& sprMarkup)
	{
		for (i32 y = 0; y < sprMarkup.Size.y; y++)
		{
			for (i32 x = 0; x < sprMarkup.Size.x; x++)
			{
				constexpr u32 alphaMask = 0xFF000000;
				const u32 pixel = GetPixel(sprMarkup.Size.x, sprMarkup.RGBAPixels, x, y);

				if ((pixel & alphaMask) != alphaMask)
					return true;
			}
		}

		return false;
	}

	std::unique_ptr<SprSet> SprPacker::Create(const std::vector<SprMarkup>& sprMarkups)
	{
		currentProgress = {};

		auto result = std::make_unique<SprSet>();
		SprSet& sprSet = *result;

		sprSet.Flags = 0;
		sprSet.Sprites.reserve(sprMarkups.size());

		const auto mergedTextures = MergeTextures(sprMarkups);

		std::vector<std::future<std::shared_ptr<Tex>>> texFutures;
		texFutures.reserve(mergedTextures.size());

		for (size_t texIndex = 0; texIndex < mergedTextures.size(); texIndex++)
		{
			const auto& texMarkup = mergedTextures[texIndex];
			for (const auto& sprBox : texMarkup.SpriteBoxes)
			{
				const auto& sprMarkup = *sprBox.Markup;
				const auto sprPadding = (GetBoxSize(sprBox.Box) - sprMarkup.Size) / 2;

				auto& spr = sprSet.Sprites.emplace_back();
				spr.TextureIndex = static_cast<i32>(texIndex);
				spr.Rotate = 0;
				spr.PixelRegion = vec4(ivec4(GetBoxPos(sprBox.Box) + sprPadding, sprMarkup.Size));
				spr.TexelRegion = GetTexelRegionFromPixelRegion(spr.PixelRegion, vec2(texMarkup.Size));
				spr.Name = sprMarkup.Name;
				spr.Extra.Flags = 0;
				spr.Extra.ScreenMode = sprMarkup.ScreenMode;
			}

			texFutures.emplace_back(std::async(Settings.Multithreaded ? std::launch::async : std::launch::deferred, [&texMarkup, this]
			{
				return CreateCompressTexFromMarkup(texMarkup);
			}));
		}

		FinalSpriteSort(sprSet.Sprites);

		sprSet.TexSet.Textures.reserve(mergedTextures.size());
		for (auto& texFuture : texFutures)
			sprSet.TexSet.Textures.emplace_back(std::move(texFuture.get()));

		return result;
	}

	void SprPacker::ReportCurrentProgress()
	{
		if (progressCallback)
			progressCallback(*this, currentProgress);
	}

	TextureFormat SprPacker::DetermineSprOutputFormat(const SprMarkup& sprMarkup) const
	{
		if (!(sprMarkup.Flags & SprMarkupFlags_Compress))
			return TextureFormat::RGBA8;

		if ((sprMarkup.Flags & SprMarkupFlags_NoMerge) && Area(sprMarkup.Size) <= Settings.NoMergeUncompressedAreaThreshold)
			return TextureFormat::RGBA8;

		if (Settings.AllowYCbCrTextures)
			return TextureFormat::RGTC2;

		return MakesUseOfAlphaChannel(sprMarkup) ? TextureFormat::DXT5 : TextureFormat::DXT1;
	}

	std::vector<SprTexMarkup> SprPacker::MergeTextures(const std::vector<SprMarkup>& sprMarkups)
	{
		currentProgress.Sprites = 0;
		currentProgress.SpritesTotal = static_cast<u32>(sprMarkups.size());

		const auto sizeSortedSprMarkups = SortByArea(sprMarkups);
		std::vector<SprTexMarkup> texMarkups;

		std::array<std::array<u16, EnumCount<SprCompressionType>>, EnumCount<SprMergeType>> formatTypeIndices = {};

		for (const auto* sprMarkupPtr : sizeSortedSprMarkups)
		{
			auto addNewTexMarkup = [&](ivec2 texSize, const auto& sprMarkup, ivec2 sprSize, TextureFormat format, SprMergeType merge)
			{
				const auto compressionType = GetCompressionType(format);
				auto& formatTypeIndex = formatTypeIndices[static_cast<size_t>(merge)][static_cast<size_t>(compressionType)];

				auto& texMarkup = texMarkups.emplace_back();
				texMarkup.Size = (Settings.PowerOfTwoTextures) ? RoundToNearestPowerOfTwo(texSize) : texSize;
				texMarkup.OutputFormat = format;
				texMarkup.CompressionType = compressionType;
				texMarkup.Merge = merge;
				texMarkup.SpriteBoxes.push_back({ &sprMarkup, ivec4(ivec2(0, 0), sprSize) });
				texMarkup.FormatTypeIndex = formatTypeIndex++;
				texMarkup.Name = FormatTextureName(texMarkup.Merge, texMarkup.CompressionType, texMarkup.FormatTypeIndex);
				texMarkup.RemainingFreePixels = Area(texMarkup.Size) - Area(sprSize);
			};

			const auto& sprMarkup = *sprMarkupPtr;
			const auto sprOutputFormat = DetermineSprOutputFormat(sprMarkup);

			if (sprMarkup.Flags & SprMarkupFlags_NoMerge)
			{
				addNewTexMarkup(sprMarkup.Size, sprMarkup, sprMarkup.Size, sprOutputFormat, SprMergeType::NoMerge);
			}
			else if (sprMarkup.Size.x > Settings.MaxTextureSize.x || sprMarkup.Size.y > Settings.MaxTextureSize.y)
			{
				addNewTexMarkup(sprMarkup.Size, sprMarkup, sprMarkup.Size + (Settings.SpritePadding * 2), sprOutputFormat, SprMergeType::Merge);
			}
			else
			{
				if (const auto[fittingTex, fittingSprBox] = FindFittingTexMarkupToPlaceSprIn(sprMarkup, sprOutputFormat, texMarkups); fittingTex != nullptr)
				{
					fittingTex->SpriteBoxes.push_back({ &sprMarkup, fittingSprBox });
					fittingTex->RemainingFreePixels -= Area(GetBoxSize(fittingSprBox));
				}
				else
				{
					const auto newTextureSize = Settings.MaxTextureSize;
					addNewTexMarkup(newTextureSize, sprMarkup, sprMarkup.Size + (Settings.SpritePadding * 2), sprOutputFormat, SprMergeType::Merge);
				}
			}

			currentProgress.Sprites++;
			ReportCurrentProgress();
		}

		AdjustTexMarkupSizes(texMarkups);
		FinalTexMarkupSort(texMarkups);

		return texMarkups;
	}

	std::vector<const SprMarkup*> SprPacker::SortByArea(const std::vector<SprMarkup>& sprMarkups) const
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

	std::pair<SprTexMarkup*, ivec4> SprPacker::FindFittingTexMarkupToPlaceSprIn(const SprMarkup& sprToPlace, TextureFormat sprOutputFormat, std::vector<SprTexMarkup>& existingTexMarkups)
	{
		static constexpr i32 stepSize = 1;
		static constexpr i32 roughStepSize = 8;

		for (auto& existingTexMarkup : existingTexMarkups)
		{
			if (existingTexMarkup.OutputFormat != sprOutputFormat)
				continue;

			if (existingTexMarkup.Merge == SprMergeType::NoMerge || existingTexMarkup.RemainingFreePixels < Area(sprToPlace.Size))
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

					for (i32 preciseY = roughStepSize - 1; preciseY >= 0; preciseY--)
					{
						for (i32 preciseX = roughStepSize - 1; preciseX >= 0; preciseX--)
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

	void SprPacker::AdjustTexMarkupSizes(std::vector<SprTexMarkup>& texMarkups) const
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

	void SprPacker::FinalTexMarkupSort(std::vector<SprTexMarkup>& texMarkups) const
	{
		// NOTE: Sort by (MERGE > COMP > INDEX)
		auto getTexSortWeight = [](const SprTexMarkup& texMarkup)
		{
			static_assert(sizeof(texMarkup.Merge) == sizeof(u8));
			static_assert(sizeof(texMarkup.CompressionType) == sizeof(u8));
			static_assert(sizeof(texMarkup.FormatTypeIndex) == sizeof(u16));

			u32 totalWeight = 0;
			totalWeight |= (static_cast<u32>(texMarkup.Merge) << 24);
			totalWeight |= (static_cast<u32>(texMarkup.CompressionType) << 16);
			totalWeight |= (static_cast<u32>(texMarkup.FormatTypeIndex) << 0);
			return totalWeight;
		};

		std::sort(texMarkups.begin(), texMarkups.end(), [&](const auto& texA, const auto& texB)
		{
			return getTexSortWeight(texA) < getTexSortWeight(texB);
		});
	}

	void SprPacker::FinalSpriteSort(std::vector<Spr>& sprites) const
	{
		// NOTE: Pseudo alphabetic order
		std::sort(sprites.begin(), sprites.end(), [&](const auto& sprA, const auto& sprB)
		{
			return (sprA.Name < sprB.Name);
		});
	}

	std::shared_ptr<Tex> SprPacker::CreateCompressTexFromMarkup(const SprTexMarkup& texMarkup)
	{
		auto mergedRGBAPixels = CreateMergedTexMarkupRGBAPixels(texMarkup);
		const auto mergedByteSize = Area(texMarkup.Size) * RGBABytesPerPixel;

		auto tex = std::make_shared<Tex>();
		tex->Name = texMarkup.Name;

		auto createUncompressedTexture = [&]
		{
			auto& mipMaps = tex->MipMapsArray.emplace_back();
			auto& baseMipMap = mipMaps.emplace_back();
			baseMipMap.Format = TextureFormat::RGBA8;
			baseMipMap.Size = texMarkup.Size;
			baseMipMap.DataSize = static_cast<u32>(mergedByteSize);
			baseMipMap.Data = std::move(mergedRGBAPixels);
		};

		if (texMarkup.OutputFormat == TextureFormat::RGBA8)
		{
			createUncompressedTexture();
			return tex;
		}

		if (texMarkup.OutputFormat == TextureFormat::RGTC2)
		{
			if (!CreateYACbCrTexture(texMarkup.Size, mergedRGBAPixels.get(), TextureFormat::RGBA8, mergedByteSize, *tex))
				createUncompressedTexture();

			return tex;
		}

		auto& mipMaps = tex->MipMapsArray.emplace_back();
		auto& baseMipMap = mipMaps.emplace_back();
		baseMipMap.Format = texMarkup.OutputFormat;
		baseMipMap.Size = texMarkup.Size;
		baseMipMap.DataSize = static_cast<u32>(TextureFormatByteSize(texMarkup.Size, texMarkup.OutputFormat));
		baseMipMap.Data = std::make_unique<u8[]>(baseMipMap.DataSize);

		if (!CompressTextureData(texMarkup.Size, mergedRGBAPixels.get(), TextureFormat::RGBA8, mergedByteSize, baseMipMap.Data.get(), texMarkup.OutputFormat, baseMipMap.DataSize))
		{
			createUncompressedTexture();
			return tex;
		}

		return tex;
	}

	std::unique_ptr<u8[]> SprPacker::CreateMergedTexMarkupRGBAPixels(const SprTexMarkup& texMarkup)
	{
		const size_t texDataSize = Area(texMarkup.Size) * RGBABytesPerPixel;
		auto texData = std::make_unique<u8[]>(texDataSize);

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
					reinterpret_cast<u32*>(texData.get())[i] = backgroundColor;
			}

			for (const auto& sprBox : texMarkup.SpriteBoxes)
				CopySprIntoTex(texMarkup, texData.get(), sprBox);
		}

		// TODO: Remove this as it can easily causes sampling artifacts (?)
		if (Settings.TransparencyColor.has_value())
			SetPixelsUniformTransparency(texMarkup.Size, texData.get(), Settings.TransparencyColor.value());

		if (Settings.FlipTexturesY)
			FlipTextureY(texMarkup.Size, texData.get());

		return texData;
	}

	SprCompressionType SprPacker::GetCompressionType(TextureFormat format) const
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
			return SprCompressionType::NoComp;
		case TextureFormat::DXT1:
		case TextureFormat::DXT1a:
		case TextureFormat::DXT3:
		case TextureFormat::DXT5:
			return SprCompressionType::D5Comp;
		case TextureFormat::RGTC1:
			return SprCompressionType::BC4Comp;
		case TextureFormat::RGTC2:
			return SprCompressionType::BC5Comp;
		default:
			return SprCompressionType::UnkComp;
		}
	}

	cstr SprPacker::GetMergeName(SprMergeType merge) const
	{
		return (merge == SprMergeType::Merge) ? "MERGE" : "NOMERGE";
	}

	cstr SprPacker::GetCompressionName(SprCompressionType compression) const
	{
		switch (compression)
		{
		case SprCompressionType::NoComp: return "NOCOMP";
		case SprCompressionType::D5Comp: return "D5COMP";
		case SprCompressionType::BC4Comp: return "BC4COMP";
		case SprCompressionType::BC5Comp: return "BC5COMP";
		case SprCompressionType::UnkComp: default: return "UNKCOMP";
		}
	}

	std::string SprPacker::FormatTextureName(SprMergeType merge, SprCompressionType compression, size_t index) const
	{
		char nameBuffer[32];
		sprintf_s(nameBuffer, "%s_%s_%03zu", GetMergeName(merge), GetCompressionName(compression), index);
		return nameBuffer;
	}
}
