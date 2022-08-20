#pragma once
#include "core_types.h"
#include "file_format_common.h"
#include "file_format_db.h"
#include <optional>

namespace Comfy
{
	using Transform2DField = u32;
	enum Transform2DFieldEnum : Transform2DField
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

	using Transform2DFieldFlags = u32;
	enum Transform2DFieldFlagsEnum : Transform2DFieldFlags
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
		vec2 Origin = { 0.0f,0.0f };
		vec2 Position = { 0.0f,0.0f };
		f32 Rotation = 0.0f;
		vec2 Scale = { 1.0f,1.0f };
		f32 Opacity = 1.0f;

		Transform2D() = default;
		constexpr Transform2D(vec2 position) : Position(position) {}
		constexpr Transform2D(vec2 origin, vec2 pos, f32 rot, vec2 scale, f32 opacity) : Origin(origin), Position(pos), Rotation(rot), Scale(scale), Opacity(opacity) {}

		inline b8 operator==(const Transform2D& other) const { return memcmp(this, &other, sizeof(*this)) == 0; }
		inline b8 operator!=(const Transform2D& other) const { return !(*this == other); }
	};
}

namespace Comfy::Aet
{
	struct Scene;
	struct Audio;
	struct Video;
	struct Composition;

	struct VideoSource
	{
		std::string Name;
		SprID ID;
	};

	struct Video
	{
		u32 Color;
		ivec2 Size;
		f32 FilesPerFrame;
		std::vector<VideoSource> Sources;
		FileAddr InternalFilePosition;

		inline VideoSource* GetSource(i32 index) { return IndexOrNull(index, Sources); }
		inline const VideoSource* GetSource(i32 index) const { return IndexOrNull(index, Sources); }
		inline VideoSource* GetFront() { return (!Sources.empty()) ? &Sources.front() : nullptr; }
		inline VideoSource* GetBack() { return (!Sources.empty()) ? &Sources.back() : nullptr; }
	};

	enum class BlendMode : u8
	{
		Unknown = 0,
		Copy = 1,				// NOTE: Unsupported (COPY)
		Behind = 2,				// NOTE: Unsupported (BEHIND)
		Normal = 3,				// NOTE: Supported   (IN_FRONT)
		Dissolve = 4,			// NOTE: Unsupported (DISSOLVE)
		Add = 5,				// NOTE: Supported   (ADD)
		Multiply = 6,			// NOTE: Supported   (MULTIPLY)
		Screen = 7,				// NOTE: Supported   (SCREEN)
		Overlay = 8,			// NOTE: Unsupported (OVERLAY)
		SoftLight = 9,			// NOTE: Unsupported (SOFT_LIGHT)
		HardLight = 10,			// NOTE: Unsupported (HARD_LIGHT)
		Darken = 11,			// NOTE: Unsupported (DARKEN)
		Lighten = 12,			// NOTE: Unsupported (LIGHTEN)
		ClassicDifference = 13, // NOTE: Unsupported (DIFFERENCE)
		Hue = 14,				// NOTE: Unsupported (HUE)
		Saturation = 15,		// NOTE: Unsupported (SATURATION)
		Color = 16,				// NOTE: Unsupported (COLOR)
		Luminosity = 17,		// NOTE: Unsupported (LUMINOSITY)
		StenciilAlpha = 18,		// NOTE: Unsupported (MULTIPLY_ALPHA)
		StencilLuma = 19,		// NOTE: Unsupported (MULTIPLY_ALPHA_LUMA)
		SilhouetteAlpha = 20,	// NOTE: Unsupported (MULTIPLY_NOT_ALPHA)
		SilhouetteLuma = 21,	// NOTE: Unsupported (MULTIPLY_NOT_ALPHA_LUMA)
		LuminescentPremul = 22, // NOTE: Unsupported (ADDITIVE_PREMUL)
		AlphaAdd = 23,			// NOTE: Unsupported (ALPHA_ADD)
		ClassicColorDodge = 24, // NOTE: Unsupported (COLOR_DODGE)
		ClassicColorBurn = 25,	// NOTE: Unsupported (COLOR_BURN)
		Exclusion = 26,			// NOTE: Unsupported (EXCLUSION)
		Difference = 27,		// NOTE: Unsupported (DIFFERENCE2)
		ColorDodge = 28,		// NOTE: Unsupported (COLOR_DODGE2)
		ColorBurn = 29,			// NOTE: Unsupported (COLOR_BURN2)
		LinearDodge = 30,		// NOTE: Unsupported (LINEAR_DODGE)
		LinearBurn = 31,		// NOTE: Unsupported (LINEAR_BURN)
		LinearLight = 32,		// NOTE: Unsupported (LINEAR_LIGHT)
		VividLight = 33,		// NOTE: Unsupported (VIVID_LIGHT)
		PinLight = 34,			// NOTE: Unsupported (PIN_LIGHT)
		HardMix = 35,			// NOTE: Unsupported (HARD_MIX)
		LighterColor = 36,		// NOTE: Unsupported (LIGHTER_COLOR)
		DarkerColor = 37,		// NOTE: Unsupported (DARKER_COLOR)
		Subtract = 38,			// NOTE: Unsupported (SUBTRACT)
		Divide = 39,			// NOTE: Unsupported (DIVIDE)

		Count
	};

	struct TransferFlags
	{
		u8 PreserveAlpha : 1;
		u8 RandomizeDissolve : 1;
	};

	static_assert(sizeof(TransferFlags) == sizeof(u8));

	enum class TrackMatte : u8
	{
		NoTrackMatte = 0,
		Alpha = 1,
		NotAlpha = 2,
		Luma = 3,
		NotLuma = 4,
	};

	struct LayerTransferMode
	{
		BlendMode BlendMode;
		TransferFlags Flags;
		TrackMatte TrackMatte;
	};

	struct KeyFrame;
	f32 InterpolateHermite(const KeyFrame& start, const KeyFrame& end, frame_t frame);
	f32 SampleFCurveAt(const std::vector<KeyFrame>& keys, frame_t frame);

	struct KeyFrame
	{
		frame_t Frame = 0.0f;
		f32 Value = 0.0f;
		f32 Tangent = 0.0f;

		constexpr KeyFrame() = default;
		constexpr KeyFrame(f32 value) : Value(value) {}
		constexpr KeyFrame(frame_t frame, f32 value) : Frame(frame), Value(value) {}
		constexpr KeyFrame(frame_t frame, f32 value, f32 curve) : Frame(frame), Value(value), Tangent(curve) {}
	};

	struct FCurve
	{
		std::vector<KeyFrame> Keys;

		inline f32 SampleAt(frame_t frame) const { return SampleFCurveAt(Keys, frame); }
		inline std::vector<KeyFrame>* operator->() { return &Keys; }
		inline const std::vector<KeyFrame>* operator->() const { return &Keys; }
	};

	struct FCurve2D
	{
		FCurve X, Y;
		inline vec2 SampleAt(frame_t frame) const { return vec2(X.SampleAt(frame), Y.SampleAt(frame)); }
	};

	struct FCurve3D
	{
		FCurve X, Y, Z;
		inline vec3 SampleAt(frame_t frame) const { return vec3(X.SampleAt(frame), Y.SampleAt(frame), Z.SampleAt(frame)); }
	};

	struct LayerVideo2D
	{
		FCurve2D Origin;
		FCurve2D Position;
		FCurve Rotation;
		FCurve2D Scale;
		FCurve Opacity;

		inline FCurve& operator[](Transform2DField field) { assert(field < Transform2DField_Count); return (&Origin.X)[field]; }
		inline const FCurve& operator[](Transform2DField field) const { assert(field < Transform2DField_Count); return (&Origin.X)[field]; }
	};

	struct LayerVideo3D
	{
		FCurve OriginZ;
		FCurve PositionZ;
		FCurve3D DirectionXYZ;
		FCurve2D RotationXY;
		FCurve ScaleZ;
	};

	struct LayerVideo
	{
		LayerTransferMode TransferMode;
		LayerVideo2D Transform;
		std::shared_ptr<LayerVideo3D> Transform3D;

		inline b8 GetUseTextureMask() const { return TransferMode.TrackMatte != TrackMatte::NoTrackMatte; }
		inline void SetUseTextureMask(b8 value) { TransferMode.TrackMatte = value ? TrackMatte::Alpha : TrackMatte::NoTrackMatte; }
	};

	struct LayerAudio
	{
		FCurve VolumeL;
		FCurve VolumeR;
		FCurve PanL;
		FCurve PanR;
	};

	struct LayerFlags
	{
		u16 VideoActive : 1;
		u16 AudioActive : 1;
		u16 EffectsActive : 1;
		u16 MotionBlur : 1;
		u16 FrameBlending : 1;
		u16 Locked : 1;
		u16 Shy : 1;
		u16 Collapse : 1;
		u16 AutoOrientRotation : 1;
		u16 AdjustmentLayer : 1;
		u16 TimeRemapping : 1;
		u16 LayerIs3D : 1;
		u16 LookAtCamera : 1;
		u16 LookAtPointOfInterest : 1;
		u16 Solo : 1;
		u16 MarkersLocked : 1;
		// NOTE: These *should* be included but aren't beacuse they don't fit inside the 16 bits stores inside the file :NotLikeThis:
		// u16 NullLayer : 1;
		// u16 HideLockedMasks : 1;
		// u16 GuideLayer : 1;
		// u16 AdvancedFrameBlending : 1;
		// u16 SubLayersRenderSeparately : 1;
		// u16 EnvironmentLayer : 1;
	};

	static_assert(sizeof(LayerFlags) == sizeof(u16));

	enum class LayerQuality : u8
	{
		None = 0,
		Wireframe = 1,
		Draft = 2,
		Best = 3,
		Count,
	};

	enum class ItemType : u8
	{
		None = 0,
		Video = 1,
		Audio = 2,
		Composition = 3,
		Count,
	};

	struct Marker
	{
		frame_t Frame;
		std::string Name;
	};

	struct Layer
	{
		frame_t StartFrame;
		frame_t EndFrame;
		frame_t StartOffset;
		f32 TimeScale;
		LayerFlags Flags;
		LayerQuality Quality;
		ItemType ItemType;
		std::vector<std::shared_ptr<Marker>> Markers;
		std::shared_ptr<LayerVideo> LayerVideo;
		std::shared_ptr<LayerAudio> LayerAudio;
		std::string Name;
		struct References
		{
			std::shared_ptr<Video> Video;
			std::shared_ptr<Audio> Audio;
			std::shared_ptr<Composition> Composition;
			std::shared_ptr<Layer> ParentLayer;
		} Ref;
		Composition* InternalParentComposition;
		FileAddr InternalFilePosition;
		FileAddr InternalItemFileOffset;
		FileAddr InternalParentFileOffset;
		FileAddr InternalAudioDataFileOffset;

		inline const std::string& GetName() const { return Name; }
		inline void SetName(const std::string& value) { Name = value; }
		inline b8 GetIsVisible() const { return Flags.VideoActive; }
		inline void SetIsVisible(b8 value) { Flags.VideoActive = value; }
		inline b8 GetIsAudible() const { return Flags.AudioActive; }
		inline void SetIsAudible(b8 value) { Flags.AudioActive = value; }
		inline const std::shared_ptr<Video>& GetVideoItem() { return Ref.Video; }
		inline const std::shared_ptr<Audio>& GetAudioItem() { return Ref.Audio; }
		inline const std::shared_ptr<Composition>& GetCompItem() { return Ref.Composition; }
		inline const Video* GetVideoItem() const { return Ref.Video.get(); }
		inline const Audio* GetAudioItem() const { return Ref.Audio.get(); }
		inline const Composition* GetCompItem() const { return Ref.Composition.get(); }
		inline void SetItem(const std::shared_ptr<Video>& value) { Ref.Video = value; }
		inline void SetItem(const std::shared_ptr<Audio>& value) { Ref.Audio = value; }
		inline void SetItem(const std::shared_ptr<Composition>& value) { Ref.Composition = value; }
		inline const std::shared_ptr<Layer>& GetRefParentLayer() { return Ref.ParentLayer; }
		inline const Layer* GetRefParentLayer() const { return Ref.ParentLayer.get(); }
		inline void SetRefParentLayer(const std::shared_ptr<Layer>& value) { Ref.ParentLayer = value; }
		inline std::optional<frame_t> FindMarkerFrame(std::string_view markerName) const { auto f = FindIfOrNull(Markers, [&](auto& m) { return m->Name == markerName; }); return (f != nullptr) ? (*f)->Frame : std::optional<frame_t> {}; }
		inline Composition* GetParentComposition() { return InternalParentComposition; }
		inline const Composition* GetParentComposition() const { return InternalParentComposition; }

		void Read(StreamReader& reader);
	};

	constexpr std::string_view RootCompositionName = "Root";
	constexpr std::string_view UnusedCompositionName = "Unused Comp";

	struct Composition
	{
		std::vector<std::shared_ptr<Layer>> Layers;
		// NOTE: The Name given to any new comp item layer referencing this comp. Assigned on AetSet load to the last layer's item name using it (= not saved if unused)
		std::string GivenName;
		Scene* InternalParentScene;
		FileAddr InternalFilePosition;

		inline Scene* GetParentScene() const { return InternalParentScene; }
		inline std::shared_ptr<Layer> FindLayer(std::string_view name) { auto f = std::find_if(Layers.begin(), Layers.end(), [&](auto& it) { return it->GetName() == name; }); return (f != Layers.end()) ? *f : nullptr; }
		inline std::shared_ptr<const Layer> FindLayer(std::string_view name) const { return const_cast<Composition*>(this)->FindLayer(name); }
	};

	struct Camera
	{
		FCurve3D Eye;
		FCurve3D Position;
		FCurve3D Direction;
		FCurve3D Rotation;
		FCurve Zoom;
	};

	struct Audio
	{
		u32 SoundID;
		FileAddr InternalFilePosition;
	};

	struct Scene
	{
		// NOTE: Typically "MAIN", "TOUCH" or named after the screen mode
		std::string Name;
		frame_t StartFrame;
		frame_t EndFrame;
		frame_t FrameRate;
		u32 BackgroundColor;
		ivec2 Resolution;
		std::shared_ptr<Camera> Camera;
		std::vector<std::shared_ptr<Composition>> Compositions;
		std::shared_ptr<Composition> RootComposition;
		std::vector<std::shared_ptr<Video>> Videos;
		std::vector<std::shared_ptr<Audio>> Audios;

		std::shared_ptr<Layer> FindLayer(std::string_view name);
		std::shared_ptr<const Layer> FindLayer(std::string_view name) const;
		i32 FindLayerIndex(Composition& comp, std::string_view name) const;

		template <typename Func> inline void ForEachComp(Func func) { for (auto& it : Compositions) { func(it); } func(RootComposition); }
		template <typename Func> inline void ForEachComp(Func func) const { for (const auto& it : Compositions) { func(it); } func(RootComposition); }

		void Read(StreamReader& reader);
		void Write(StreamWriter& writer);

		void InternalUpdateParentPointers();
		void InternalUpdateCompNamesAfterLayerItems();
		void InternalUpdateCompNamesAfterLayerItems(std::shared_ptr<Composition>& comp);
		void InternalLinkPostRead();
		void InternalLinkCompItems(Composition& comp);
		void InternalFindSetLayerRefParentLayer(Layer& layer);
	};

	struct AetSet final : IStreamReadable, IStreamWritable
	{
		std::string Name;
		std::vector<std::shared_ptr<Scene>> Scenes;

		StreamResult Read(StreamReader& reader) override;
		StreamResult Write(StreamWriter& writer) override;
	};
}
