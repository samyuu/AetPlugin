#pragma once
#include "GraphicTypes.h"
#include "CoreTypes.h"

namespace Comfy::Graphics
{
	constexpr std::array<const char*, static_cast<size_t>(PrimitiveType::Count)> PrimitiveTypeNames =
	{
		"Points",
		"Lines",
		"Line Strip",
		"Line Loop",
		"Triangles",
		"Triangle Strip",
		"Triangle Fan",
		"Quads",
		"Quad Strip",
		"Polygon",
	};

	constexpr std::array<const char*, static_cast<size_t>(AetBlendMode::Count)> AetBlendModeNames =
	{
		"Unknown",
		"Copy",
		"Behind",
		"Normal",
		"Dissolve",
		"Additive",
		"Multiply",
		"Screen",
		"Overlay",
		"Soft Light",
		"Hard Light",
		"Darken",
		"Lighten",
		"Classic Difference",
		"Hue",
		"Saturation",
		"Color",
		"Luminosity",
		"Stenciil Alpha",
		"Stencil Luma",
		"Silhouette Alpha",
		"Silhouette Luma",
		"Luminescent Premul",
		"Alpha Add",
		"Classic Color Dodge",
		"Classic Color Burn",
		"Exclusion",
		"Difference",
		"Color Dodge",
		"Color Burn",
		"Linear Dodge",
		"Linear Burn",
		"Linear Light",
		"Vivid Light",
		"Pin Light",
		"Hard Mix",
		"Lighter Color",
		"Darker Color",
		"Subtract",
		"Divide",
	};

	constexpr std::array<const char*, static_cast<size_t>(TextureFormat::Count)> TextureFormatNames =
	{
		"A8",
		"RGB8",
		"RGBA8",
		"RGB5",
		"RGB5_A1",
		"RGBA4",
		"DXT1",
		"DXT1a",
		"DXT3",
		"DXT5",
		"RGTC1",
		"RGTC2",
		"L8,",
		"L8A8",
	};

	constexpr std::array<const char*, static_cast<size_t>(ScreenMode::Custom)> ScreenModeNames =
	{
		"VGA",
		"SVGA",
		"XGA",
		"SXGA",
		"SXGA_PLUS",
		"UXGA",
		"WVGA",
		"WSVGA",
		"WXGA",
		"WXGA",
		"WUXGA",
		"WQXGA",
		"HDTV720",
		"HDTV1080",
		"WQHD",
		"HVGA",
		"qHD",
	};

	constexpr std::array<const char*, static_cast<size_t>(ToneMapMethod::Count)> ToneMapMethodNames =
	{
		"YCC Exponent",
		"RGB Linear",
		"RGB Linear 2",
	};

	constexpr std::array<const char*, static_cast<size_t>(FogType::Count)> FogTypeNames =
	{
		"None",
		"Linear",
		"Exp",
		"Exp2",
	};

	constexpr std::array<const char*, static_cast<size_t>(LightSourceType::Count)> LightSourceTypeNames =
	{
		"None",
		"Parallel",
		"Point",
		"Spot",
	};

	constexpr std::array<const char*, static_cast<size_t>(LightTargetType::Count)> LightTargetTypeNames =
	{
		"Character",
		"Stage",
		"Sun",
		"Reflect",
		"Shadow",
		"Character Color",
		"Character F",
		"Projection",
	};
}
