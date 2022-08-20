#pragma once
#include "core_types.h"
#include "file_format_common.h"
#include "file_format_db.h"
#include <optional>

namespace Comfy
{
	enum class TextureFormat : u32
	{
		Unknown = 0xFFFFFFFF,
		A8 = 0,			// NOTE: DXGI_FORMAT_R8_UINT / GL_ALPHA8
		RGB8 = 1,		// NOTE: DXGI_FORMAT_UNKNOWN / GL_RGB8
		RGBA8 = 2,		// NOTE: DXGI_FORMAT_R8G8B8A8_UNORM / GL_RGBA8
		RGB5 = 3,		// NOTE: DXGI_FORMAT_UNKNOWN / GL_RGB5
		RGB5_A1 = 4,	// NOTE: DXGI_FORMAT_UNKNOWN / GL_RGB5_A1
		RGBA4 = 5,		// NOTE: DXGI_FORMAT_UNKNOWN / GL_RGBA4
		DXT1 = 6,		// NOTE: DXGI_FORMAT_BC1_UNORM / GL_COMPRESSED_RGB_S3TC_DXT1_EXT
		DXT1a = 7,		// NOTE: DXGI_FORMAT_BC1_UNORM / GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
		DXT3 = 8,		// NOTE: DXGI_FORMAT_BC2_UNORM_SRGB / GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
		DXT5 = 9,		// NOTE: DXGI_FORMAT_BC3_UNORM / GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
		RGTC1 = 10,		// NOTE: DXGI_FORMAT_BC4_UNORM / GL_COMPRESSED_RED_RGTC1
		RGTC2 = 11,		// NOTE: DXGI_FORMAT_BC5_UNORM / GL_COMPRESSED_RG_RGTC2
		L8 = 12,		// NOTE: DXGI_FORMAT_A8_UNORM / GL_LUMINANCE8
		L8A8 = 13,		// NOTE: DXGI_FORMAT_A8P8 / GL_LUMINANCE8_ALPHA8
		Count
	};

	enum class ScreenMode : u32
	{
		QVGA = 0,		// NOTE:  320 x  240
		VGA = 1,		// NOTE:  640 x  480
		SVGA = 2,		// NOTE:  800 x  600
		XGA = 3,		// NOTE: 1024 x  768
		SXGA = 4,		// NOTE: 1280 x 1024
		SXGA_PLUS = 5,	// NOTE: 1400 x 1050
		UXGA = 6,		// NOTE: 1600 x 1200
		WVGA = 7,		// NOTE:  800 x  480
		WSVGA = 8,		// NOTE: 1024 x  600
		WXGA = 9,		// NOTE: 1280 x  768
		WXGA_ = 10,		// NOTE: 1360 x  768
		WUXGA = 11,		// NOTE: 1920 x 1200
		WQXGA = 12,		// NOTE: 2560 x 1536
		HDTV720 = 13,	// NOTE: 1280 x  720
		HDTV1080 = 14,	// NOTE: 1920 x 1080
		WQHD = 15,		// NOTE: 2560 x 1440
		HVGA = 16,		// NOTE:  480 x  272
		qHD = 17,		// NOTE:  960 x  544
		Custom = 18,	// NOTE: ____ x ____
		Count = Custom
	};

	enum class TxpSig : u32
	{
		MipMap = '\02PXT',
		TexSet = '\03PXT',
		Texture2D = '\04PXT',
		CubeMap = '\05PXT',
	};

	constexpr std::string_view FallbackTextureName = "F_COMFY_UNKNOWN";

	struct TexMipMap
	{
		ivec2 Size;
		TextureFormat Format;
		u32 DataSize;
		std::unique_ptr<u8[]> Data;
	};

	struct Tex
	{
		// NOTE: Two dimensional array [CubeFace][MipMap]
		std::vector<std::vector<TexMipMap>> MipMapsArray;
		std::optional<std::string> Name;

		inline const std::vector<TexMipMap>& GetMipMaps(u32 arrayIndex) const { return MipMapsArray[arrayIndex]; }
		inline TxpSig GetSignature() const { return (MipMapsArray.size() == 6) ? TxpSig::CubeMap : TxpSig::Texture2D; }
		inline ivec2 GetSize() const { return (MipMapsArray.size() < 1 || MipMapsArray.front().size() < 1) ? ivec2(0, 0) : MipMapsArray.front().front().Size; }
		inline TextureFormat GetFormat() const { return (MipMapsArray.size() < 1 || MipMapsArray.front().size() < 1) ? TextureFormat::Unknown : MipMapsArray.front().front().Format; }
		inline std::string_view GetName() const { return (Name.has_value()) ? Name.value() : FallbackTextureName; }
		StreamResult Read(StreamReader& reader);
	};

	struct TexSet final : IStreamReadable, IStreamWritable
	{
		std::vector<std::shared_ptr<Tex>> Textures;

		StreamResult Read(StreamReader& reader) override;
		StreamResult Write(StreamWriter& writer) override;
	};

	struct Spr
	{
		i32 TextureIndex;
		i32 Rotate;
		vec4 TexelRegion;
		vec4 PixelRegion;
		std::string Name;
		struct ExtraData
		{
			u32 Flags;
			ScreenMode ScreenMode;
		} Extra;

		inline vec2 GetSize() const { return vec2(PixelRegion.z, PixelRegion.w); }
	};

	struct SprSet final : IStreamReadable, IStreamWritable
	{
		std::string Name;
		u32 Flags;
		TexSet TexSet;
		std::vector<Spr> Sprites;

		void ApplyDBNames(const SprSetEntry& sprSetEntry);
		StreamResult Read(StreamReader& reader) override;
		StreamResult Write(StreamWriter& writer) override;
	};
}
