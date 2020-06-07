#pragma once
#include <Types.h>
#include <CoreTypes.h>
#include <Graphics/Auth2D/Aet/AetUtil.h>

namespace AetPlugin::StreamUtil
{
	using namespace Comfy::Graphics;

	struct StreamRemapData2D
	{
		AEGP_LayerStream StreamType;
		Transform2DField_Enum FieldX, FieldY;
		float ScaleFactor;
	};

	constexpr std::array Transform2DRemapData =
	{
		StreamRemapData2D { AEGP_LayerStream_ANCHORPOINT,	Transform2DField_OriginX,	Transform2DField_OriginY,	1.0f },
		StreamRemapData2D { AEGP_LayerStream_POSITION,		Transform2DField_PositionX,	Transform2DField_PositionY,	1.0f },
		StreamRemapData2D { AEGP_LayerStream_ROTATION,		Transform2DField_Rotation,	Transform2DField_Rotation,	1.0f },
		StreamRemapData2D { AEGP_LayerStream_SCALE,			Transform2DField_ScaleX,	Transform2DField_ScaleY,	100.0f },
		StreamRemapData2D { AEGP_LayerStream_OPACITY,		Transform2DField_Opacity,	Transform2DField_Opacity,	100.0f },

		// NOTE: LayerVideo3D only, zero transform to avoid out of bounds
		// StreamRemapData2D { AEGP_LayerStream_ORIENTATION,	{},							{},	0.0f },
	};

#if 0
	enum class AetTransform2DField3D
	{
		None = -1,
		OriginZ,
		PositionZ,
		DirectionX,
		DirectionY,
		DirectionZ,
		RotationX,
		RotationY,
		ScaleZ,
		Count,
	};

	struct StreamRemapData3D
	{
		AEGP_LayerStream StreamType;
		AetTransform2DField3D FieldX, FieldY, FieldZ;
		float ScaleFactor;
	};

	constexpr std::array Transform3DRemapData =
	{
		StreamRemapData3D { AEGP_LayerStream_ANCHORPOINT,	AetTransform2DField3D::None,		AetTransform2DField3D::None,		AetTransform2DField3D::OriginZ,		1.0f },
		StreamRemapData3D { AEGP_LayerStream_POSITION,		AetTransform2DField3D::None,		AetTransform2DField3D::None,		AetTransform2DField3D::PositionZ,	1.0f },
		StreamRemapData3D { AEGP_LayerStream_ORIENTATION,	AetTransform2DField3D::DirectionX,	AetTransform2DField3D::DirectionY,	AetTransform2DField3D::DirectionZ,	1.0f },
		StreamRemapData3D { AEGP_LayerStream_ROTATION,		AetTransform2DField3D::RotationX,	AetTransform2DField3D::RotationY,	AetTransform2DField3D::None,		1.0f },
		StreamRemapData3D { AEGP_LayerStream_SCALE,			AetTransform2DField3D::None,		AetTransform2DField3D::None,		AetTransform2DField3D::ScaleZ,		100.0f },
	};

	inline Aet::Property1D* GetLayerVideo3DProperty(Aet::LayerVideo3D* layerVideo3D, AetTransform2DField3D field3D)
	{
		if (layerVideo3D == nullptr)
			return nullptr;

		switch (field3D)
		{
		case AetTransform2DField3D::OriginZ: return &layerVideo3D->OriginZ;
		case AetTransform2DField3D::PositionZ: return &layerVideo3D->PositionZ;
		case AetTransform2DField3D::DirectionX: return &layerVideo3D->DirectionXYZ.X;
		case AetTransform2DField3D::DirectionY: return &layerVideo3D->DirectionXYZ.Y;
		case AetTransform2DField3D::DirectionZ: return &layerVideo3D->DirectionXYZ.Z;
		case AetTransform2DField3D::RotationX: return &layerVideo3D->RotationXY.X;
		case AetTransform2DField3D::RotationY: return &layerVideo3D->RotationXY.Y;
		case AetTransform2DField3D::ScaleZ: return &layerVideo3D->ScaleZ;
		default: return nullptr;
		}
	}
#endif

	struct CombinedAetLayerVideo2D3D
	{
		CombinedAetLayerVideo2D3D(Aet::LayerVideo& layerVideo)
		{
			auto& transform2D = layerVideo.Transform;
			auto* transform3D = layerVideo.Transform3D.get();

			OriginX = &transform2D.Origin.X;
			OriginY = &transform2D.Origin.Y;
			OriginZ = (transform3D != nullptr) ? &transform3D->OriginZ : nullptr;

			PositionX = &transform2D.Position.X;
			PositionY = &transform2D.Position.Y;
			PositionZ = (transform3D != nullptr) ? &transform3D->PositionZ : nullptr;

			RotationX = (transform3D != nullptr) ? &transform3D->RotationXY.X : nullptr;
			RotationY = (transform3D != nullptr) ? &transform3D->RotationXY.Y : nullptr;
			RotationZ = &transform2D.Rotation;

			ScaleX = &transform2D.Scale.X;
			ScaleY = &transform2D.Scale.Y;
			ScaleZ = (transform3D != nullptr) ? &transform3D->ScaleZ : nullptr;

			Opacity = &transform2D.Opacity;

			DirectionX = (transform3D != nullptr) ? &transform3D->DirectionXYZ.X : nullptr;
			DirectionY = (transform3D != nullptr) ? &transform3D->DirectionXYZ.Y : nullptr;
			DirectionZ = (transform3D != nullptr) ? &transform3D->DirectionXYZ.Z : nullptr;
		}

		Aet::Property1D *OriginX, *OriginY, *OriginZ;
		Aet::Property1D *PositionX, *PositionY, *PositionZ;
		Aet::Property1D *RotationX, *RotationY, *RotationZ;
		Aet::Property1D *ScaleX, *ScaleY, *ScaleZ;
		Aet::Property1D *Opacity;
		Aet::Property1D *DirectionX, *DirectionY, *DirectionZ;
	};
}
