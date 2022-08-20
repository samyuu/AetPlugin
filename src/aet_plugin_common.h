#pragma once
#include "core_types.h"
#include "comfy/file_format_aet_set.h"
#include <AEConfig.h>
#include <AE_GeneralPlug.h>
#include <AE_Effect.h>
#include <AE_EffectUI.h>
#include <AE_EffectCB.h>
#include <AE_AdvEffectSuites.h>
#include <AE_EffectCBSuites.h>
#include <AEGP_SuiteHandler.h>
#include <AE_Macros.h>
#include <charconv>
#include <array>

namespace Comfy {}
namespace AetPlugin { using namespace Comfy; }

namespace AetPlugin
{
	struct AetExDataTag
	{
		// NOTE: Explicitly disallow copying to avoid accidentally creating a copy instead of a reference when looking up via a map
		AetExDataTag() = default;
		AetExDataTag(const AetExDataTag& other) = delete;
		AetExDataTag(AetExDataTag&& other) = default;
		AetExDataTag& operator=(const AetExDataTag& other) = delete;

		AEGP_CompH AE_Comp = nullptr;
		AEGP_ItemH AE_CompItem = nullptr;
		AEGP_FootageH AE_Footage = nullptr;
		AEGP_ItemH AE_FootageItem = nullptr;
		AEGP_LayerH AE_Layer = nullptr;
	};

	struct AetExDataTagMap
	{
		inline AetExDataTag& Get(const Aet::Layer& object) { return VoidPtrToTagMap[&object]; }
		inline AetExDataTag& Get(const Aet::Composition& object) { return VoidPtrToTagMap[&object]; }
		inline AetExDataTag& Get(const Aet::Video& object) { return VoidPtrToTagMap[&object]; }
		inline AetExDataTag& Get(const Aet::Audio& object) { return VoidPtrToTagMap[&object]; }
		std::unordered_map<const void*, AetExDataTag> VoidPtrToTagMap;
	};
}

namespace AetPlugin::AEUtil
{
	struct RGB8 { u8 R, G, B, A; };

	constexpr f32 RGB8ToF32 = static_cast<f32>(U8Max);
	constexpr A_Ratio OneToOneRatio = { 1, 1 };
	constexpr f32 FixedPoint = 10000.0f;

	inline const A_UTF16Char* UTF16Cast(const wchar_t* value)
	{
		return reinterpret_cast<const A_UTF16Char*>(value);
	}

	inline const wchar_t* WCast(const A_UTF16Char* value)
	{
		return reinterpret_cast<const wchar_t*>(value);
	}

	inline std::wstring MoveFreeUTF16String(AEGP_MemorySuite1* memorySuite1, AEGP_MemHandle memHandle)
	{
		void* underlyingData;
		memorySuite1->AEGP_LockMemHandle(memHandle, &underlyingData);
		if (underlyingData == nullptr)
			return L"";

		const auto underlyingString = reinterpret_cast<const A_UTF16Char*>(underlyingData);
		const auto result = std::wstring(WCast(underlyingString));

		memorySuite1->AEGP_UnlockMemHandle(memHandle);
		memorySuite1->AEGP_FreeMemHandle(memHandle);

		return result;
	}

	inline A_Time FrameToAETime(frame_t frame, frame_t frameRate)
	{
		return { static_cast<A_long>(frame * FixedPoint), static_cast<A_u_long>(frameRate * FixedPoint) };
	}

	inline frame_t AETimeToFrame(A_Time time, frame_t frameRate)
	{
		return static_cast<frame_t>((static_cast<f64>(time.value) / static_cast<f64>(time.scale)) * static_cast<f64>(frameRate));
	}

	inline A_Ratio Ratio(f32 ratio)
	{
		return { static_cast<A_long>(ratio * AEUtil::FixedPoint), static_cast<A_u_long>(AEUtil::FixedPoint) };
	}

	inline f32 Ratio(A_Ratio ratio)
	{
		return static_cast<f32>(static_cast<f64>(ratio.num) / static_cast<f64>(ratio.den));
	}

	inline AEGP_ColorVal ColorRGB8(u32 inputColor)
	{
		const RGB8 colorRGB8 = *reinterpret_cast<const RGB8*>(&inputColor);
		return AEGP_ColorVal
		{
			static_cast<f32>(1.0f),
			static_cast<f32>(colorRGB8.R / RGB8ToF32),
			static_cast<f32>(colorRGB8.G / RGB8ToF32),
			static_cast<f32>(colorRGB8.B / RGB8ToF32),
		};
	}

	inline u32 ColorRGB8(AEGP_ColorVal inputColor)
	{
		const RGB8 colorRGB8 =
		{
			static_cast<u8>(inputColor.redF * RGB8ToF32),
			static_cast<u8>(inputColor.greenF * RGB8ToF32),
			static_cast<u8>(inputColor.blueF * RGB8ToF32),
			static_cast<u8>(0x00),
		};
		return *reinterpret_cast<const u32*>(&colorRGB8);
	}
}

namespace AetPlugin::CommentUtil
{
	constexpr std::string_view AetID = "#Aet";

	namespace Keys
	{
		// NOTE: #Aet::Set: { %set_name%, %aet_set_id%, %spr_set_id% }
		constexpr std::string_view AetSet = "Set";
		// NOTE: #Aet::Scene[%index%]: { %scene_name%, %scene_id% }
		constexpr std::string_view AetScene = "Scene";
		// NOTE: #Aet::Spr: { %spr_id%, ... }
		constexpr std::string_view Spr = "Spr";
	}

	// NOTE: Enough space to store "#Aet::Spr: { 0x00000000, ... }"
	using Buffer = std::array<char, 8192>;

	struct Property
	{
		std::string_view Key = {};
		std::string_view Value = {};
		std::optional<size_t> KeyIndex = {};

		Property() = default;
		Property(std::string_view key, std::string_view value) : Key(key), Value(value) {}
		Property(std::string_view key, std::string_view value, std::optional<size_t> keyIndex) : Key(key), Value(value), KeyIndex(keyIndex) {}
	};

	namespace Detail
	{
		inline Buffer Format(const Property& property)
		{
			char indexStr[24] = {};
			if (property.KeyIndex.has_value())
				sprintf(indexStr, "[%zu]", property.KeyIndex.value());

			Buffer commentBuffer;
			sprintf(commentBuffer.data(), "%.*s::%.*s%s: { %.*s }", FmtStrViewArgs(AetID), FmtStrViewArgs(property.Key), indexStr, FmtStrViewArgs(property.Value));
			return commentBuffer;
		}

		inline Property Parse(const std::string_view buffer)
		{
			Property result = {};

			const auto trimmedBuffer = ASCII::Trim(buffer);
			if (!ASCII::StartsWith(trimmedBuffer, AetID) || (trimmedBuffer.size() < (AetID.size() + std::strlen("::x: {}"))))
				return result;

			const std::string_view postIDBuffer = trimmedBuffer.substr(AetID.size() + std::strlen("::"));
			const std::string_view keyBuffer = postIDBuffer.substr(0, std::min(postIDBuffer.find(':'), postIDBuffer.find('[')));

			if (keyBuffer.empty())
				return result;

			std::string_view postKeyBuffer = postIDBuffer.substr(keyBuffer.size());
			if (postKeyBuffer.front() == '[')
			{
				auto indexEndPos = postKeyBuffer.find(']');
				if (indexEndPos == std::string_view::npos)
					return result;

				const auto indexBuffer = postKeyBuffer.substr(1, indexEndPos - 1);
				if (indexBuffer.empty())
					return result;

				unsigned long long parsedIndex = {};
				if (const auto parseResult = std::from_chars(indexBuffer.data(), indexBuffer.data() + indexBuffer.size(), parsedIndex); parseResult.ec == std::errc::invalid_argument)
					return result;

				result.KeyIndex = static_cast<size_t>(parsedIndex);
				postKeyBuffer = postKeyBuffer.substr(indexBuffer.size() + std::strlen("[]"));
			}

			constexpr std::string_view bracketsStart = ": {", bracketsEnd = "}";
			if (!ASCII::StartsWith(postKeyBuffer, bracketsStart) || !ASCII::EndsWith(postKeyBuffer, bracketsEnd))
				return result;

			const std::string_view valueBuffer = ASCII::Trim(postKeyBuffer.substr(bracketsStart.size(), postKeyBuffer.size() - bracketsStart.size() - bracketsEnd.size()));

			result.Key = keyBuffer;
			result.Value = valueBuffer;

			return result;
		}

		inline std::string_view AdvanceCommaSeparateList(std::string_view& inOutList)
		{
			if (inOutList.empty())
				return inOutList.substr(0, 0);

			if (const auto nextComma = inOutList.find(','); nextComma == std::string_view::npos)
			{
				const auto subString = inOutList.substr(0);
				inOutList = inOutList.substr(inOutList.size() - 1, 0);
				return subString;
			}
			else
			{
				const auto subString = inOutList.substr(0, nextComma);
				inOutList = inOutList.substr(subString.length() + 1);
				return subString;
			}
		}
	}

	inline void Set(AEGP_ItemSuite8* itemSuite8, AEGP_ItemH item, const Property& property)
	{
		if (item == nullptr)
			return;

		const auto buffer = Detail::Format(property);
		itemSuite8->AEGP_SetItemComment(item, buffer.data());
	}

	inline Property Get(AEGP_ItemSuite8* itemSuite8, AEGP_ItemH item, Buffer& outBuffer)
	{
		if (item == nullptr)
			return {};

		itemSuite8->AEGP_GetItemComment(item, static_cast<A_u_long>(outBuffer.size()), outBuffer.data());
		return Detail::Parse(outBuffer.data());
	}

	inline u32 ParseID(std::string_view commentValue)
	{
		const auto preHexTrimSize = commentValue.size();
		commentValue = ASCII::TrimPrefixInsensitive(commentValue, "0x");

		u32 resultID = {};
		const auto result = std::from_chars(commentValue.data(), commentValue.data() + commentValue.size(), resultID, (commentValue.size() == preHexTrimSize) ? 10 : 16);;

		return (result.ec != std::errc::invalid_argument) ? resultID : 0xFFFFFFFF;
	}

	template <typename Func>
	inline void IterateCommaSeparateList(const std::string_view commaSeparateList, Func func)
	{
		std::string_view readHead = commaSeparateList;
		const char* endOfList = commaSeparateList.data() + commaSeparateList.size() - 1;

		while (true)
		{
			const auto currentItem = Detail::AdvanceCommaSeparateList(readHead);
			const auto itemTrimmed = ASCII::Trim(currentItem);

			func(itemTrimmed);

			if (readHead.data() >= endOfList)
				return;
		}
	}

	template <size_t Size>
	inline std::array<std::string_view, Size> ParseArray(std::string_view commaSeparateList)
	{
		std::array<std::string_view, Size> result = {};

		size_t index = 0;
		IterateCommaSeparateList(commaSeparateList, [&](const auto& item)
		{
			if (index < Size)
				result[index] = item;
			index++;
		});

		return result;
	}
}

namespace AetPlugin::StreamUtil
{
	constexpr f32 GetAetToAEStreamFactor(AEGP_LayerStream streamType)
	{
		return (streamType == AEGP_LayerStream_SCALE || streamType == AEGP_LayerStream_OPACITY) ? 100.0f : 1.0f;
	}

	struct CombinedAetLayerVideo2D3D
	{
		CombinedAetLayerVideo2D3D(Aet::LayerVideo& layerVideo)
		{
			Aet::LayerVideo2D& x2D = layerVideo.Transform;
			Aet::LayerVideo3D* x3D = layerVideo.Transform3D.get();
			OriginX = &x2D.Origin.X;
			OriginY = &x2D.Origin.Y;
			OriginZ = (x3D != nullptr) ? &x3D->OriginZ : nullptr;
			PositionX = &x2D.Position.X;
			PositionY = &x2D.Position.Y;
			PositionZ = (x3D != nullptr) ? &x3D->PositionZ : nullptr;
			RotationX = (x3D != nullptr) ? &x3D->RotationXY.X : nullptr;
			RotationY = (x3D != nullptr) ? &x3D->RotationXY.Y : nullptr;
			RotationZ = &x2D.Rotation;
			ScaleX = &x2D.Scale.X;
			ScaleY = &x2D.Scale.Y;
			ScaleZ = (x3D != nullptr) ? &x3D->ScaleZ : nullptr;
			Opacity = &x2D.Opacity;
			DirectionX = (x3D != nullptr) ? &x3D->DirectionXYZ.X : nullptr;
			DirectionY = (x3D != nullptr) ? &x3D->DirectionXYZ.Y : nullptr;
			DirectionZ = (x3D != nullptr) ? &x3D->DirectionXYZ.Z : nullptr;
		}

		Aet::FCurve *OriginX, *OriginY, *OriginZ;
		Aet::FCurve *PositionX, *PositionY, *PositionZ;
		Aet::FCurve *RotationX, *RotationY, *RotationZ;
		Aet::FCurve *ScaleX, *ScaleY, *ScaleZ;
		Aet::FCurve *Opacity;
		Aet::FCurve *DirectionX, *DirectionY, *DirectionZ;
	};
}
