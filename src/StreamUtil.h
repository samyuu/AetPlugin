#pragma once
#include <Types.h>
#include <CoreTypes.h>
#include <Graphics/Auth2D/Aet/AetUtil.h>

namespace AetPlugin::StreamUtil
{
	using namespace Comfy::Graphics;

	constexpr float GetAetToAEStreamFactor(AEGP_LayerStream streamType)
	{
		return (streamType == AEGP_LayerStream_SCALE || streamType == AEGP_LayerStream_OPACITY) ? 100.0f : 1.0f;
	}

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
