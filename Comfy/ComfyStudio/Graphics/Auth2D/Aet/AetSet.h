#pragma once
#include "Types.h"
#include "CoreTypes.h"
#include "../Transform2D.h"
#include "Resource/IDTypes.h"
#include "FileSystem/FileInterface.h"
#include "Graphics/GraphicTypes.h"
#include <variant>

#if 1 // GUI_EXTRA_DATA_OVERRIDE
#include "../../AeExtraData.h"
#else
struct GuiExtraData {};
#endif /* GUI_EXTRA_DATA_OVERRIDE */

namespace Comfy::Graphics
{
	struct Spr;
}

namespace Comfy::Graphics::Aet
{
	class Scene;
	class Composition;
	class Audio;

	class ILayerItem
	{
	protected:
		ILayerItem() = default;
	};

	class NullItem : public ILayerItem, NonCopyable
	{
	};

	struct VideoSource
	{
		std::string Name;
		SprID ID;

		// HACK: Editor internal cache to avoid expensive string comparisons
		mutable const Spr* SpriteCache;
	};

	class Video : public ILayerItem, NonCopyable
	{
		friend class Scene;

	public:
		Video() = default;
		~Video() = default;

	public:
		mutable GuiExtraData GuiData;

		uint32_t Color;
		ivec2 Size;
		frame_t Frames;
		std::vector<VideoSource> Sources;

	public:
		VideoSource* GetSource(int index);
		const VideoSource* GetSource(int index) const;

		VideoSource* GetFront();
		VideoSource* GetBack();

	private:
		FileAddr filePosition;
	};

	struct TransferFlags
	{
		uint8_t PreserveAlpha : 1;
		uint8_t RandomizeDissolve : 1;
	};

	enum class TrackMatte : uint8_t
	{
		NoTrackMatte = 0,
		Alpha = 1,
		NotAlpha = 2,
		Luma = 3,
		NotLuma = 4,
	};

	struct LayerTransferMode
	{
		AetBlendMode BlendMode;
		TransferFlags Flags;
		TrackMatte TrackMatte;
	};

	struct KeyFrame
	{
		KeyFrame() = default;
		KeyFrame(float value) : Value(value) {}
		KeyFrame(frame_t frame, float value) : Frame(frame), Value(value) {}
		KeyFrame(frame_t frame, float value, float curve) : Frame(frame), Value(value), Curve(curve) {}

		frame_t Frame = 0.0f;
		float Value = 0.0f;
		float Curve = 0.0f;
	};

	struct Property1D
	{
		std::vector<KeyFrame> Keys;

		inline std::vector<KeyFrame>* operator->() { return &Keys; }
		inline const std::vector<KeyFrame>* operator->() const { return &Keys; }
	};

	struct Property2D
	{
		Property1D X, Y;
	};

	struct Property3D
	{
		Property1D X, Y, Z;
	};

	struct LayerVideo2D
	{
		static constexpr std::array<const char*, 8> FieldNames =
		{
			"Origin X",
			"Origin Y",
			"Position X",
			"Position Y",
			"Rotation",
			"Scale X",
			"Scale Y",
			"Opactiy",
		};

		Property2D Origin;
		Property2D Position;
		Property1D Rotation;
		Property2D Scale;
		Property1D Opacity;

		inline Property1D& operator[](Transform2DField field)
		{
			assert(field >= Transform2DField_OriginX && field < Transform2DField_Count);
			return (&Origin.X)[field];
		}

		inline const Property1D& operator[](Transform2DField field) const
		{
			return (*const_cast<LayerVideo2D*>(this))[field];
		}
	};

	struct LayerVideo3D
	{
		Property1D AnchorZ;
		Property1D PositionZ;
		Property3D Direction;
		Property2D Rotation;
		Property1D ScaleZ;
	};

	struct LayerVideo
	{
		mutable GuiExtraData GuiData;

		LayerTransferMode TransferMode;
		LayerVideo2D Transform;
		RefPtr<LayerVideo3D> Transform3D;

		inline bool GetUseTextureMask() const { return TransferMode.TrackMatte != TrackMatte::NoTrackMatte; }
		inline void SetUseTextureMask(bool value) { TransferMode.TrackMatte = value ? TrackMatte::Alpha : TrackMatte::NoTrackMatte; }
	};

	struct LayerAudio
	{
		Property1D VolumeL;
		Property1D VolumeR;
		Property1D PanL;
		Property1D PanR;
	};

	struct LayerFlags
	{
		uint16_t VideoActive : 1;
		uint16_t AudioActive : 1;
		uint16_t EffectsActive : 1;
		uint16_t MotionBlur : 1;
		uint16_t FrameBlending : 1;
		uint16_t Locked : 1;
		uint16_t Shy : 1;
		uint16_t Collapse : 1;
		uint16_t AutoOrientRotation : 1;
		uint16_t AdjustmentLayer : 1;
		uint16_t TimeRemapping : 1;
		uint16_t LayerIs3D : 1;
		uint16_t LookAtCamera : 1;
		uint16_t LookAtPointOfInterest : 1;
		uint16_t Solo : 1;
		uint16_t MarkersLocked : 1;
		// uint16_t NullLayer : 1;
		// uint16_t HideLockedMasks : 1;
		// uint16_t GuideLayer : 1;
		// uint16_t AdvancedFrameBlending : 1;
		// uint16_t SubLayersRenderSeparately : 1;
		// uint16_t EnvironmentLayer : 1;
	};

	enum class LayerQuality : uint8_t
	{
		None = 0,
		Wireframe = 1,
		Draft = 2,
		Best = 3,
		Count,
	};

	static constexpr std::array<const char*, static_cast<size_t>(LayerQuality::Count)> LayerQualityNames =
	{
		"None",
		"Wireframe",
		"Draft",
		"Best",
	};

	enum class ItemType : uint8_t
	{
		None = 0,
		Video = 1,
		Audio = 2,
		Composition = 3,
		Count,
	};

	static constexpr std::array<const char*, static_cast<size_t>(ItemType::Count)> ItemTypeNames =
	{
		"None",
		"Video",
		"Audio",
		"Composition",
	};

	struct Marker
	{
		Marker() = default;
		Marker(frame_t frame, std::string_view name) : Frame(frame), Name(name) {}

		frame_t Frame;
		std::string Name;
	};

	class Layer : NonCopyable
	{
		friend class Scene;
		friend class Composition;

	public:
		Layer() = default;
		~Layer() = default;

	public:
		mutable GuiExtraData GuiData;

		frame_t StartFrame;
		frame_t EndFrame;
		frame_t StartOffset;
		float TimeScale;
		LayerFlags Flags;
		LayerQuality Quality;
		ItemType ItemType;
		std::vector<RefPtr<Marker>> Markers;
		RefPtr<LayerVideo> LayerVideo;
		RefPtr<LayerAudio> LayerAudio;

	public:
		const std::string& GetName() const;
		void SetName(const std::string& value);

		bool GetIsVisible() const;
		void SetIsVisible(bool value);

		bool GetIsAudible() const;
		void SetIsAudible(bool value);

		const RefPtr<Video>& GetVideoItem();
		const RefPtr<Audio>& GetAudioItem();
		const RefPtr<Composition>& GetCompItem();

		const Video* GetVideoItem() const;
		const Audio* GetAudioItem() const;
		const Composition* GetCompItem() const;

		void SetItem(const RefPtr<Video>& value);
		void SetItem(const RefPtr<Audio>& value);
		void SetItem(const RefPtr<Composition>& value);

		const RefPtr<Layer>& GetRefParentLayer();
		const Layer* GetRefParentLayer() const;
		void SetRefParentLayer(const RefPtr<Layer>& value);

	public:
		Scene* GetParentScene();
		const Scene* GetParentScene() const;

		Composition* GetParentComposition();
		const Composition* GetParentComposition() const;

	private:
		std::string name;
		Composition* parentComposition;

		struct References
		{
			RefPtr<Video> Video;
			RefPtr<Audio> Audio;
			RefPtr<Composition> Composition;
			RefPtr<Layer> ParentLayer;
		} references;

		FileAddr filePosition;
		FileAddr itemFilePtr;
		FileAddr parentFilePtr;
		FileAddr audioDataFilePtr;

		void Read(FileSystem::BinaryReader& reader);
	};

	class Composition : public ILayerItem, NonCopyable
	{
		friend class Scene;

	public:
		Composition() = default;
		~Composition() = default;

	public:
		mutable GuiExtraData GuiData;

	public:
		Scene* GetParentScene() const;
		bool IsRootComposition() const;

		inline std::vector<RefPtr<Layer>>& GetLayers() { return layers; }
		inline const std::vector<RefPtr<Layer>>& GetLayers() const { return layers; }

	public:
		inline std::string_view GetName() const { return givenName; }
		inline void SetName(std::string_view value) { givenName = value; }

		RefPtr<Layer> FindLayer(std::string_view name);
		RefPtr<const Layer> FindLayer(std::string_view name) const;

	private:
		static constexpr std::string_view rootCompositionName = "Root";
		static constexpr std::string_view unusedCompositionName = "Unused Comp";

		Scene* parentScene;
		FileAddr filePosition;

		// NOTE: The Name given to any new comp item layer referencing this comp. Assigned on AetSet load to the last layer's item name using it (= not saved if unused)
		std::string givenName;
		std::vector<RefPtr<Layer>> layers;
	};

	struct Camera
	{
		Property3D Eye;
		Property3D Position;
		Property3D Direction;
		Property3D Rotation;
		Property1D Zoom;
	};

	class Audio : public ILayerItem, NonCopyable
	{
		friend class Scene;

	public:
		Audio() = default;
		~Audio() = default;

	public:
		mutable GuiExtraData GuiData;

		unk32_t SoundID;

	private:
		// TODO:
		// std::string givenName;

		FileAddr filePosition;
	};

	class Scene : NonCopyable
	{
		friend class AetSet;
		friend class Composition;
		friend class Layer;

	public:
		// NOTE: Typically "MAIN", "TOUCH" or named after the display mode
		std::string Name;

		frame_t StartFrame;
		frame_t EndFrame;
		frame_t FrameRate;

		uint32_t BackgroundColor;
		ivec2 Resolution;

		RefPtr<Camera> Camera;

		std::vector<RefPtr<Composition>> Compositions;
		RefPtr<Composition> RootComposition;

		std::vector<RefPtr<Video>> Videos;
		std::vector<RefPtr<Audio>> Audios;

	public:
		Composition* GetRootComposition();
		const Composition* GetRootComposition() const;

		RefPtr<Layer> FindLayer(std::string_view name);
		RefPtr<const Layer> FindLayer(std::string_view name) const;

		int FindLayerIndex(Composition& comp, std::string_view name) const;

	public:
		void UpdateParentPointers();

	private:
		void Read(FileSystem::BinaryReader& reader);
		void Write(FileSystem::BinaryWriter& writer);

	private:
		void UpdateCompNamesAfterLayerItems();
		void UpdateCompNamesAfterLayerItems(RefPtr<Composition>& comp);
		void LinkPostRead();
		void LinkCompItems(Composition& comp);
		void FindSetLayerRefParentLayer(Layer& layer);
	};

	class AetSet final : public FileSystem::IBinaryReadWritable, NonCopyable
	{
	public:
		AetSet() = default;
		~AetSet() = default;

	public:
		std::string Name;

	public:
		inline std::vector<RefPtr<Scene>>& GetScenes() { return scenes; }
		inline const std::vector<RefPtr<Scene>>& GetScenes() const { return scenes; }

		void ClearSpriteCache();

	public:
		void Read(FileSystem::BinaryReader& reader) override;
		void Write(FileSystem::BinaryWriter& writer) override;

	private:
		std::vector<RefPtr<Scene>> scenes;
	};
}
