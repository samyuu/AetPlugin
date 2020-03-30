#include "AetMgr.h"

namespace Comfy::Graphics::Aet
{
	namespace
	{
		void TransformByParent(const Transform2D& input, Transform2D& output)
		{
			// BUG: This is still not 100% accurate (?)

			output.Position -= input.Origin;
			output.Position *= input.Scale;

			if (input.Rotation != 0.0f)
			{
				float radians = glm::radians(input.Rotation);
				float sin = glm::sin(radians);
				float cos = glm::cos(radians);

				output.Position = vec2(output.Position.x * cos - output.Position.y * sin, output.Position.x * sin + output.Position.y * cos);
			}

			output.Position += input.Position;

			if ((input.Scale.x < 0.0f) ^ (input.Scale.y < 0.0f))
				output.Rotation *= -1.0f;

			output.Rotation += input.Rotation;

			output.Scale *= input.Scale;
			output.Opacity *= input.Opacity;
		}
	}

	void AetMgr::GetAddObjects(std::vector<ObjCache>& objects, const Composition* comp, frame_t frame)
	{
		for (int i = static_cast<int>(comp->GetLayers().size()) - 1; i >= 0; i--)
			GetAddObjects(objects, comp->GetLayers()[i].get(), frame);
	}

	void AetMgr::GetAddObjects(std::vector<ObjCache>& objects, const Layer* layer, frame_t frame)
	{
		Transform2D transform = Transform2D(vec2(0.0f));
		InternalAddObjects(objects, &transform, layer, frame);

		if (layer->ItemType == ItemType::Composition)
		{
			for (auto& object : objects)
			{
				if (object.FirstParent == nullptr)
					object.FirstParent = layer;
			}
		}
	}

	float AetMgr::Interpolate(const KeyFrame* start, const KeyFrame* end, frame_t frame)
	{
		float range = end->Frame - start->Frame;
		float t = (frame - start->Frame) / range;

		return (((((((t * t) * t) - ((t * t) * 2.0f)) + t) * start->Curve)
			+ ((((t * t) * t) - (t * t)) * end->Curve)) * range)
			+ (((((t * t) * 3.0f) - (((t * t) * t) * 2.0f)) * end->Value)
				+ ((((((t * t) * t) * 2.0f) - ((t * t) * 3.0f)) + 1.0f) * start->Value));
	}

	float AetMgr::GetValueAt(const std::vector<KeyFrame>& keyFrames, frame_t frame)
	{
		if (keyFrames.size() <= 0)
			return 0.0f;

		auto first = keyFrames.front();
		auto last = keyFrames.back();

		if (keyFrames.size() == 1 || frame <= first.Frame)
			return first.Value;

		if (frame >= last.Frame)
			return last.Value;

		const KeyFrame* start = &keyFrames[0];
		const KeyFrame* end = start;

		for (int i = 1; i < keyFrames.size(); i++)
		{
			end = &keyFrames[i];
			if (end->Frame >= frame)
				break;
			start = end;
		}

		return Interpolate(start, end, frame);
	}

	float AetMgr::GetValueAt(const Property1D& property, frame_t frame)
	{
		return AetMgr::GetValueAt(property.Keys, frame);
	}

	vec2 AetMgr::GetValueAt(const Property2D& property, frame_t frame)
	{
		return vec2(AetMgr::GetValueAt(property.X, frame), AetMgr::GetValueAt(property.Y, frame));
	}

	Transform2D AetMgr::GetTransformAt(const LayerVideo2D& transform, frame_t frame)
	{
		Transform2D result;
		result.Origin = AetMgr::GetValueAt(transform.Origin, frame);
		result.Position = AetMgr::GetValueAt(transform.Position, frame);
		result.Rotation = AetMgr::GetValueAt(transform.Rotation, frame);
		result.Scale = AetMgr::GetValueAt(transform.Scale, frame);
		result.Opacity = AetMgr::GetValueAt(transform.Opacity, frame);
		return result;
	}

	Transform2D AetMgr::GetTransformAt(const LayerVideo& animationData, frame_t frame)
	{
		return AetMgr::GetTransformAt(animationData.Transform, frame);
	}

	bool AetMgr::AreFramesTheSame(frame_t frameA, frame_t frameB)
	{
		// NOTE: Completely arbitrary threshold, in practice even a frame round or an int cast would probably suffice
		constexpr frame_t frameComparisonThreshold = 0.001f;
		
		return std::abs(frameA - frameB) < frameComparisonThreshold;
	}

	KeyFrame* AetMgr::GetKeyFrameAt(Property1D& property, frame_t frame)
	{
		// NOTE: The aet editor should always try to prevent this itself
		assert(property->size() > 0);

		// TODO: This could implement a binary search although the usually small number keyframes might not warrant it
		for (auto& keyFrame : property.Keys)
		{
			if (AreFramesTheSame(keyFrame.Frame, frame))
				return &keyFrame;
		}

		return nullptr;
	}

	void AetMgr::InsertKeyFrameAt(std::vector<KeyFrame>& keyFrames, frame_t frame, float value)
	{
		keyFrames.emplace_back(frame, value);
		AetMgr::SortKeyFrames(keyFrames);
	}

	void AetMgr::DeleteKeyFrameAt(std::vector<KeyFrame>& keyFrames, frame_t frame)
	{
		auto existing = std::find_if(keyFrames.begin(), keyFrames.end(), [frame](const KeyFrame& keyFrame)
		{
			return AreFramesTheSame(keyFrame.Frame, frame);
		});

		if (existing != keyFrames.end())
		{
			keyFrames.erase(existing);
		}
		else
		{
			assert(false);
		}
	}

	void AetMgr::SortKeyFrames(std::vector<KeyFrame>& keyFrames)
	{
		std::sort(keyFrames.begin(), keyFrames.end(), [](const KeyFrame& keyFrameA, const KeyFrame& keyFrameB)
		{
			return keyFrameA.Frame < keyFrameB.Frame;
		});
	}

	void AetMgr::OffsetAllKeyFrames(LayerVideo2D& transform, frame_t frameIncrement)
	{
		for (Transform2DField i = 0; i < Transform2DField_Count; i++)
		{
			for (auto& keyFrame : transform[i].Keys)
				keyFrame.Frame += frameIncrement;
		}
	}

	void AetMgr::ApplyParentTransform(Transform2D& outTransform, const Layer* parent, frame_t frame, int32_t& recursionCount)
	{
		assert(recursionCount < ParentRecursionLimit);
		if (parent == nullptr || recursionCount > ParentRecursionLimit)
			return;

		recursionCount++;

		const Layer* parentParent = parent->GetRefParentLayer();
		assert(parentParent != parent);

		const Transform2D parentTransform = AetMgr::GetTransformAt(*parent->LayerVideo, frame);

		outTransform.Position += parentTransform.Position - parentTransform.Origin;
		outTransform.Rotation += parentTransform.Rotation;
		outTransform.Scale *= parentTransform.Scale;

		if (parentParent != nullptr && parentParent != parent)
			ApplyParentTransform(outTransform, parentParent, frame, recursionCount);
	}

	void AetMgr::FindAddCompositionUsages(const RefPtr<Scene>& aetToSearch, const RefPtr<Composition>& compToFind, std::vector<RefPtr<Layer>*>& outObjects)
	{
		const auto compSearchFunction = [&compToFind, &outObjects](const RefPtr<Composition>& compToSearch)
		{
			for (auto& layer : compToSearch->GetLayers())
			{
				if (layer->GetCompItem() == compToFind)
					outObjects.push_back(&layer);
			}
		};

		compSearchFunction(aetToSearch->RootComposition);

		for (auto& compToTest : aetToSearch->Compositions)
			compSearchFunction(compToTest);
	}

	void AetMgr::InternalAddObjects(std::vector<AetMgr::ObjCache>& objects, const Transform2D* parentTransform, const Layer* layer, frame_t frame)
	{
		if (layer->ItemType == ItemType::Video)
		{
			InternalPicAddObjects(objects, parentTransform, layer, frame);
		}
		else if (layer->ItemType == ItemType::Composition)
		{
			InternalEffAddObjects(objects, parentTransform, layer, frame);
		}
	}

	void AetMgr::InternalPicAddObjects(std::vector<AetMgr::ObjCache>& objects, const Transform2D* parentTransform, const Layer* layer, frame_t frame)
	{
		assert(layer->ItemType == ItemType::Video);

		if (frame < layer->StartFrame || frame >= layer->EndFrame)
			return;

		objects.emplace_back();
		ObjCache& objCache = objects.back();

		objCache.FirstParent = nullptr;
		objCache.Source = layer;
		objCache.Video = layer->GetVideoItem();
		objCache.BlendMode = layer->LayerVideo->TransferMode.BlendMode;
		objCache.UseTextureMask = layer->LayerVideo->TransferMode.TrackMatte == TrackMatte::Alpha;
		objCache.Visible = layer->GetIsVisible();

		if (objCache.Video != nullptr && objCache.Video->Sources.size() > 1)
		{
			// BUG: This should factor in the layer->StartFrame (?)
			objCache.SpriteIndex = static_cast<int>(glm::round((frame + layer->StartOffset) * layer->TimeScale));
			objCache.SpriteIndex = glm::clamp(objCache.SpriteIndex, 0, static_cast<int>(objCache.Video->Sources.size()) - 1);
		}
		else
		{
			objCache.SpriteIndex = 0;
		}

		objCache.Transform = AetMgr::GetTransformAt(*layer->LayerVideo, frame);

		int32_t recursionCount = 0;
		AetMgr::ApplyParentTransform(objCache.Transform, layer->GetRefParentLayer(), frame, recursionCount);
		TransformByParent(*parentTransform, objCache.Transform);
	}

	void AetMgr::InternalEffAddObjects(std::vector<AetMgr::ObjCache>& objects, const Transform2D* parentTransform, const Layer* layer, frame_t frame)
	{
		assert(layer->ItemType == ItemType::Composition);

		if (frame < layer->StartFrame || frame >= layer->EndFrame || !layer->GetIsVisible())
			return;

		const Composition* comp = layer->GetCompItem();
		if (comp == nullptr)
			return;

		Transform2D effTransform = AetMgr::GetTransformAt(*layer->LayerVideo, frame);

		int32_t recursionCount = 0;
		AetMgr::ApplyParentTransform(effTransform, layer->GetRefParentLayer(), frame, recursionCount);
		TransformByParent(*parentTransform, effTransform);

		frame_t adjustedFrame = ((frame - layer->StartFrame) * layer->TimeScale) + layer->StartOffset;

		for (int i = static_cast<int>(comp->GetLayers().size()) - 1; i >= 0; i--)
			InternalAddObjects(objects, &effTransform, comp->GetLayers().at(i).get(), adjustedFrame);
	}
}
