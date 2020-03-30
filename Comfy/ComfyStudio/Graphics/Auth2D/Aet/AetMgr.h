#pragma once
#include "Types.h"
#include "AetSet.h"
#include "../Transform2D.h"

namespace Comfy::Graphics::Aet
{
	class AetMgr
	{
	public:
		// NOTE: Arbitrary safety limit, prevent stack overflows no matter the input
		static constexpr int32_t ParentRecursionLimit = 0x100;

		struct ObjCache
		{
			Transform2D Transform;
			int32_t SpriteIndex;
			const Video* Video;
			AetBlendMode BlendMode;
			const Layer* FirstParent;
			const Layer* Source;
			bool UseTextureMask;
			bool Visible;
		};

		static void GetAddObjects(std::vector<AetMgr::ObjCache>& objects, const Composition* comp, frame_t frame);
		static void GetAddObjects(std::vector<AetMgr::ObjCache>& objects, const Layer* layer, frame_t frame);

		static float Interpolate(const KeyFrame* start, const KeyFrame* end, frame_t frame);
		
		static float GetValueAt(const std::vector<KeyFrame>& keyFrames, frame_t frame);
		static float GetValueAt(const Property1D& property, frame_t frame);
		static vec2 GetValueAt(const Property2D& property, frame_t frame);
		
		static Transform2D GetTransformAt(const LayerVideo2D& transform, frame_t frame);
		static Transform2D GetTransformAt(const LayerVideo& animationData, frame_t frame);

		// NOTE: Threshold frame foat comparison
		static bool AreFramesTheSame(frame_t frameA, frame_t frameB);

		static KeyFrame* GetKeyFrameAt(Property1D& property, frame_t frame);
		
		static void InsertKeyFrameAt(std::vector<KeyFrame>& keyFrames, frame_t frame, float value);
		static void DeleteKeyFrameAt(std::vector<KeyFrame>& keyFrames, frame_t frame);
		
		// NOTE: Because a KeyFrameCollection is expected to always be sorted
		static void SortKeyFrames(std::vector<KeyFrame>& keyFrames);

		// NOTE: To be used after changing the StartFrame frame of a layer
		static void OffsetAllKeyFrames(LayerVideo2D& transform, frame_t frameIncrement);

		// NOTE: Recursively add the properties of the parent layer to the input properties if there is one
		static void ApplyParentTransform(Transform2D& outTransform, const Layer* parent, frame_t frame, int32_t& recursionCount);

		// NOTE: To easily navigate between composition references in the tree view
		static void FindAddCompositionUsages(const RefPtr<Scene>& aetToSearch, const RefPtr<Composition>& compToFind, std::vector<RefPtr<Layer>*>& outObjects);

	private:
		static void InternalAddObjects(std::vector<AetMgr::ObjCache>& objects, const Transform2D* parentTransform, const Layer* layer, frame_t frame);
		static void InternalPicAddObjects(std::vector<AetMgr::ObjCache>& objects, const Transform2D* parentTransform, const Layer* layer, frame_t frame);
		static void InternalEffAddObjects(std::vector<AetMgr::ObjCache>& objects, const Transform2D* parentTransform, const Layer* layer, frame_t frame);
	};
}
