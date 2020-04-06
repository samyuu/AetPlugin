#include "AetSet.h"
#include <assert.h>

namespace Comfy::Graphics::Aet
{
	VideoSource* Video::GetSource(int index)
	{
		return InBounds(index, Sources) ? &Sources[index] : nullptr;
	}

	const VideoSource* Video::GetSource(int index) const
	{
		return InBounds(index, Sources) ? &Sources[index] : nullptr;
	}

	VideoSource* Video::GetFront()
	{
		return (!Sources.empty()) ? &Sources.front() : nullptr;
	}

	VideoSource* Video::GetBack()
	{
		return (!Sources.empty()) ? &Sources.back() : nullptr;
	}

	const std::string& Layer::GetName() const
	{
		return name;
	}

	void Layer::SetName(const std::string& value)
	{
		name = value;
	}

	bool Layer::GetIsVisible() const
	{
		return Flags.VideoActive;
	}

	void Layer::SetIsVisible(bool value)
	{
		Flags.VideoActive = value;
	}

	bool Layer::GetIsAudible() const
	{
		return Flags.AudioActive;
	}

	void Layer::SetIsAudible(bool value)
	{
		Flags.AudioActive = value;
	}

	const RefPtr<Video>& Layer::GetVideoItem()
	{
		return references.Video;
	}

	const RefPtr<Audio>& Layer::GetAudioItem()
	{
		return references.Audio;
	}

	const RefPtr<Composition>& Layer::GetCompItem()
	{
		return references.Composition;
	}

	const Video* Layer::GetVideoItem() const
	{
		return references.Video.get();
	}

	const Audio* Layer::GetAudioItem() const
	{
		return references.Audio.get();
	}

	const Composition* Layer::GetCompItem() const
	{
		return references.Composition.get();
	}

	void Layer::SetItem(const RefPtr<Video>& value)
	{
		references.Video = value;
	}

	void Layer::SetItem(const RefPtr<Audio>& value)
	{
		references.Audio = value;
	}

	void Layer::SetItem(const RefPtr<Composition>& value)
	{
		references.Composition = value;
	}

	const RefPtr<Layer>& Layer::GetRefParentLayer()
	{
		return references.ParentLayer;
	}

	const Layer* Layer::GetRefParentLayer() const
	{
		return references.ParentLayer.get();
	}

	void Layer::SetRefParentLayer(const RefPtr<Layer>& value)
	{
		references.ParentLayer = value;
	}

	Scene* Layer::GetParentScene()
	{
		assert(parentComposition != nullptr);
		return parentComposition->GetParentScene();
	}

	const Scene* Layer::GetParentScene() const
	{
		assert(parentComposition != nullptr);
		return parentComposition->GetParentScene();
	}

	Composition* Layer::GetParentComposition()
	{
		return parentComposition;
	}

	const Composition* Layer::GetParentComposition() const
	{
		return parentComposition;
	}

	Scene* Composition::GetParentScene() const
	{
		return parentScene;
	}

	bool Composition::IsRootComposition() const
	{
		return this == parentScene->RootComposition.get();
	}

	RefPtr<Layer> Composition::FindLayer(std::string_view name)
	{
		auto result = std::find_if(layers.begin(), layers.end(), [&](auto& layer) { return layer->GetName() == name; });
		return (result != layers.end()) ? *result : nullptr;
	}

	RefPtr<const Layer> Composition::FindLayer(std::string_view name) const
	{
		return const_cast<Composition*>(this)->FindLayer(name);
	}

	Composition* Scene::GetRootComposition()
	{
		return RootComposition.get();
	}

	const Composition* Scene::GetRootComposition() const
	{
		return RootComposition.get();
	}

	RefPtr<Layer> Scene::FindLayer(std::string_view name)
	{
		const RefPtr<Layer>& rootFoundLayer = RootComposition->FindLayer(name);
		if (rootFoundLayer != nullptr)
			return rootFoundLayer;

		for (int32_t i = static_cast<int32_t>(Compositions.size()) - 1; i >= 0; i--)
		{
			const RefPtr<Layer>& layer = Compositions[i]->FindLayer(name);
			if (layer != nullptr)
				return layer;
		}

		return nullptr;
	}

	RefPtr<const Layer> Scene::FindLayer(std::string_view name) const
	{
		return const_cast<Scene*>(this)->FindLayer(name);
	}

	int Scene::FindLayerIndex(Composition& comp, std::string_view name) const
	{
		for (int i = static_cast<int>(comp.GetLayers().size()) - 1; i >= 0; i--)
		{
			if (comp.GetLayers()[i]->GetName() == name)
				return i;
		}

		return -1;
	}

	void Scene::UpdateParentPointers()
	{
		const auto updateParentPointers = [this](Composition& comp)
		{
			comp.parentScene = this;

			for (auto& layer : comp.GetLayers())
				layer->parentComposition = &comp;
		};

		for (auto& comp : Compositions)
			updateParentPointers(*comp);

		updateParentPointers(*RootComposition);
	}

	void Scene::UpdateCompNamesAfterLayerItems()
	{
		RootComposition->SetName(Composition::rootCompositionName);
		UpdateCompNamesAfterLayerItems(RootComposition);

		for (auto& comp : Compositions)
			UpdateCompNamesAfterLayerItems(comp);
	}

	void Scene::UpdateCompNamesAfterLayerItems(RefPtr<Composition>& comp)
	{
		for (auto& layer : comp->GetLayers())
		{
			if (layer->ItemType == ItemType::Composition)
			{
				if (auto compItem = layer->GetCompItem().get(); compItem != nullptr)
					compItem->SetName(layer->GetName());
			}
		}
	}

	void Scene::LinkPostRead()
	{
		assert(RootComposition != nullptr);

		for (auto& comp : Compositions)
			LinkCompItems(*comp);

		LinkCompItems(*RootComposition);
	}

	void Scene::LinkCompItems(Composition& comp)
	{
		auto findSetLayerItem = [](auto& layer, auto& itemCollection)
		{
			auto foundItem = std::find_if(itemCollection.begin(), itemCollection.end(), [&](auto& e) { return e->filePosition == layer.itemFilePtr; });
			if (foundItem != itemCollection.end())
				layer.SetItem(*foundItem);
		};

		for (auto& layer : comp.GetLayers())
		{
			if (layer->itemFilePtr != FileAddr::NullPtr)
			{
				if (layer->ItemType == ItemType::Video)
					findSetLayerItem(*layer, Videos);
				else if (layer->ItemType == ItemType::Audio)
					findSetLayerItem(*layer, Audios);
				else if (layer->ItemType == ItemType::Composition)
					findSetLayerItem(*layer, Compositions);
			}

			if (layer->parentFilePtr != FileAddr::NullPtr)
				FindSetLayerRefParentLayer(*layer);
		}
	}

	void Scene::FindSetLayerRefParentLayer(Layer& layer)
	{
		for (auto& otherComp : Compositions)
		{
			for (auto& otherLayer : otherComp->GetLayers())
			{
				if (otherLayer->filePosition == layer.parentFilePtr)
				{
					layer.references.ParentLayer = otherLayer;
					return;
				}
			}
		}
	}

	void AetSet::ClearSpriteCache()
	{
		for (auto& scene : scenes)
		{
			for (auto& video : scene->Videos)
			{
				for (auto& source : video->Sources)
					source.SpriteCache = nullptr;
			}
		}
	}
}
