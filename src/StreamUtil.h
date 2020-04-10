#pragma once
#include "Types.h"
#include "CoreTypes.h"
#include "Graphics/Auth2D/Aet/AetMgr.h"

namespace StreamUtil
{
	using namespace Comfy;
	using namespace Comfy::Graphics;

	struct StreamRemapData
	{
		AEGP_LayerStream StreamType;
		Transform2DField_Enum FieldX, FieldY;
		float ScaleFactor;
	};

	constexpr std::array Transform2DRemapData =
	{
		StreamRemapData { AEGP_LayerStream_ANCHORPOINT,	Transform2DField_OriginX,	Transform2DField_OriginY,	1.0f },
		StreamRemapData { AEGP_LayerStream_POSITION,	Transform2DField_PositionX,	Transform2DField_PositionY,	1.0f },
		StreamRemapData { AEGP_LayerStream_ROTATION,	Transform2DField_Rotation,	Transform2DField_Rotation,	1.0f },
		StreamRemapData { AEGP_LayerStream_SCALE,		Transform2DField_ScaleX,	Transform2DField_ScaleY,	100.0f },
		StreamRemapData { AEGP_LayerStream_OPACITY,		Transform2DField_Opacity,	Transform2DField_Opacity,	100.0f },
	};
}
