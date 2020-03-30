#pragma once
#include "Types.h"

namespace Comfy::Graphics
{
	using Transform2DField = int32_t;
	using Transform2DFieldFlags = int32_t;

	enum Transform2DField_Enum : Transform2DField
	{
		Transform2DField_OriginX,
		Transform2DField_OriginY,
		Transform2DField_PositionX,
		Transform2DField_PositionY,
		Transform2DField_Rotation,
		Transform2DField_ScaleX,
		Transform2DField_ScaleY,
		Transform2DField_Opacity,
		Transform2DField_Count,
	};

	enum Transform2DFieldFlags_Enum : Transform2DFieldFlags
	{
		Transform2DFieldFlags_OriginX = (1 << Transform2DField_OriginX),
		Transform2DFieldFlags_OriginY = (1 << Transform2DField_OriginY),
		Transform2DFieldFlags_PositionX = (1 << Transform2DField_PositionX),
		Transform2DFieldFlags_PositionY = (1 << Transform2DField_PositionY),
		Transform2DFieldFlags_Rotation = (1 << Transform2DField_Rotation),
		Transform2DFieldFlags_ScaleX = (1 << Transform2DField_ScaleX),
		Transform2DFieldFlags_ScaleY = (1 << Transform2DField_ScaleY),
		Transform2DFieldFlags_Opacity = (1 << Transform2DField_Opacity),
	};

	struct Transform2D
	{
		vec2 Origin;
		vec2 Position;
		float Rotation;
		vec2 Scale;
		float Opacity;

		Transform2D() = default;

		constexpr Transform2D(vec2 position)
			: Origin(0.0f), Position(position), Rotation(0.0f), Scale(1.0f), Opacity(1.0f)
		{
		};

		constexpr Transform2D(vec2 origin, vec2 position, float rotation, vec2 scale, float opacity)
			: Origin(origin), Position(position), Rotation(rotation), Scale(scale), Opacity(opacity)
		{
		};

		inline Transform2D& operator=(const Transform2D& other)
		{
			Origin = other.Origin;
			Position = other.Position;
			Rotation = other.Rotation;
			Scale = other.Scale;
			Opacity = other.Opacity;
			return *this;
		}

		inline bool operator==(const Transform2D& other) const
		{
			return (Origin == other.Origin) && (Position == other.Position) && (Rotation == other.Rotation) && (Scale == other.Scale) && (Opacity == other.Opacity);
		}

		inline bool operator!=(const Transform2D& other) const
		{
			return !(*this == other);
		}
	};
}
