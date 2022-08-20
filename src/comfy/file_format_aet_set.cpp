#include "file_format_aet_set.h"
#include <algorithm>

namespace Comfy::Aet
{
	f32 InterpolateHermite(const KeyFrame& start, const KeyFrame& end, frame_t frame)
	{
		const f32 range = end.Frame - start.Frame;
		const f32 t = (frame - start.Frame) / range;

		return (((((((t * t) * t) - ((t * t) * 2.0f)) + t) * start.Tangent)
			+ ((((t * t) * t) - (t * t)) * end.Tangent)) * range)
			+ (((((t * t) * 3.0f) - (((t * t) * t) * 2.0f)) * end.Value)
				+ ((((((t * t) * t) * 2.0f) - ((t * t) * 3.0f)) + 1.0f) * start.Value));
	}

	f32 SampleFCurveAt(const std::vector<KeyFrame>& keys, frame_t frame)
	{
		if (keys.size() <= 0)
			return 0.0f;

		const KeyFrame& front = keys.front();
		const KeyFrame& back = keys.back();
		if (keys.size() == 1 || frame <= front.Frame)
			return front.Value;
		else if (frame >= back.Frame)
			return back.Value;

		const KeyFrame* start = &front;
		const KeyFrame* end = start;
		for (size_t i = 1; i < keys.size(); i++)
		{
			end = &keys[i];
			if (end->Frame >= frame)
				break;
			start = end;
		}

		return InterpolateHermite(*start, *end, frame);
	}

	std::shared_ptr<Layer> Scene::FindLayer(std::string_view name)
	{
		const std::shared_ptr<Layer>& rootFoundLayer = RootComposition->FindLayer(name);
		if (rootFoundLayer != nullptr)
			return rootFoundLayer;

		for (i32 i = static_cast<i32>(Compositions.size()) - 1; i >= 0; i--)
		{
			const std::shared_ptr<Layer>& layer = Compositions[i]->FindLayer(name);
			if (layer != nullptr)
				return layer;
		}

		return nullptr;
	}

	std::shared_ptr<const Layer> Scene::FindLayer(std::string_view name) const
	{
		return const_cast<Scene*>(this)->FindLayer(name);
	}

	i32 Scene::FindLayerIndex(Composition& comp, std::string_view name) const
	{
		for (i32 i = static_cast<i32>(comp.Layers.size()) - 1; i >= 0; i--)
		{
			if (comp.Layers[i]->GetName() == name)
				return i;
		}
		return -1;
	}

	void Scene::InternalUpdateParentPointers()
	{
		ForEachComp([&](auto& comp)
		{
			comp->InternalParentScene = this;
			for (auto& layer : comp->Layers)
				layer->InternalParentComposition = comp.get();
		});
	}

	void Scene::InternalUpdateCompNamesAfterLayerItems()
	{
		RootComposition->GivenName = RootCompositionName;
		InternalUpdateCompNamesAfterLayerItems(RootComposition);

		for (auto& comp : Compositions)
			InternalUpdateCompNamesAfterLayerItems(comp);
	}

	void Scene::InternalUpdateCompNamesAfterLayerItems(std::shared_ptr<Composition>& comp)
	{
		for (auto& layer : comp->Layers)
		{
			if (layer->ItemType == ItemType::Composition)
			{
				if (auto compItem = layer->GetCompItem().get(); compItem != nullptr)
					compItem->GivenName = layer->GetName();
			}
		}
	}

	void Scene::InternalLinkPostRead()
	{
		assert(RootComposition != nullptr);
		ForEachComp([&](auto& comp) { InternalLinkCompItems(*comp); });
	}

	void Scene::InternalLinkCompItems(Composition& comp)
	{
		auto findSetLayerItem = [](auto& layer, auto& itemVector)
		{
			if (auto f = FindIfOrNull(itemVector, [&](auto& e) { return e->InternalFilePosition == layer.InternalItemFileOffset; }); f != nullptr)
				layer.SetItem(*f);
		};

		for (auto& layer : comp.Layers)
		{
			if (layer->InternalItemFileOffset != FileAddr::NullPtr)
			{
				if (layer->ItemType == ItemType::Video)
					findSetLayerItem(*layer, Videos);
				else if (layer->ItemType == ItemType::Audio)
					findSetLayerItem(*layer, Audios);
				else if (layer->ItemType == ItemType::Composition)
					findSetLayerItem(*layer, Compositions);
			}

			if (layer->InternalParentFileOffset != FileAddr::NullPtr)
				InternalFindSetLayerRefParentLayer(*layer);
		}
	}

	void Scene::InternalFindSetLayerRefParentLayer(Layer& layer)
	{
		for (auto& otherComp : Compositions)
		{
			for (auto& otherLayer : otherComp->Layers)
			{
				if (otherLayer->InternalFilePosition == layer.InternalParentFileOffset)
				{
					layer.Ref.ParentLayer = otherLayer;
					return;
				}
			}
		}
	}

	template <typename FlagsStruct>
	static FlagsStruct ReadFlagsStruct(StreamReader& reader)
	{
		if constexpr (sizeof(FlagsStruct) == sizeof(u8))
		{
			const auto uintFlags = reader.ReadU8();
			return *reinterpret_cast<const FlagsStruct*>(&uintFlags);
		}
		else if constexpr (sizeof(FlagsStruct) == sizeof(u16))
		{
			const auto uintFlags = reader.ReadU16();
			return *reinterpret_cast<const FlagsStruct*>(&uintFlags);
		}
		else if constexpr (sizeof(FlagsStruct) == sizeof(u32))
		{
			const auto uintFlags = reader.ReadU32();
			return *reinterpret_cast<const FlagsStruct*>(&uintFlags);
		}
		else
		{
			static_assert(false);
		}
	}

	static u32 ReadU32ColorRGB(StreamReader& reader)
	{
		u32 value = 0;
		*(reinterpret_cast<u8*>(&value) + 0) = reader.ReadU8();
		*(reinterpret_cast<u8*>(&value) + 1) = reader.ReadU8();
		*(reinterpret_cast<u8*>(&value) + 2) = reader.ReadU8();
		const u8 padding = reader.ReadU8();
		return value;
	}

	static StreamResult ReadFCurvePtr(StreamReader& reader, FCurve& out)
	{
		const auto keyFrameCount = reader.ReadSize();
		const auto keyFramesOffset = reader.ReadPtr();

		if (keyFrameCount < 1)
			return StreamResult::Success;

		if (!reader.IsValidPointer(keyFramesOffset))
			return StreamResult::BadPointer;

		reader.ReadAtOffsetAware(keyFramesOffset, [keyFrameCount, &out](StreamReader& reader)
		{
			out->resize(keyFrameCount);

			if (keyFrameCount == 1)
			{
				out->front().Value = reader.ReadF32();
			}
			else
			{
				for (size_t i = 0; i < keyFrameCount; i++)
					out.Keys[i].Frame = reader.ReadF32();

				for (size_t i = 0; i < keyFrameCount; i++)
				{
					out.Keys[i].Value = reader.ReadF32();
					out.Keys[i].Tangent = reader.ReadF32();
				}
			}
		});

		return StreamResult::Success;
	}

	static StreamResult ReadFCurve2DPtr(StreamReader& reader, FCurve2D& out)
	{
		if (auto result = ReadFCurvePtr(reader, out.X); result != StreamResult::Success) return result;
		if (auto result = ReadFCurvePtr(reader, out.Y); result != StreamResult::Success) return result;
		return StreamResult::Success;
	}

	static StreamResult ReadFCurve3DPtr(StreamReader& reader, FCurve3D& out)
	{
		if (auto result = ReadFCurvePtr(reader, out.X); result != StreamResult::Success) return result;
		if (auto result = ReadFCurvePtr(reader, out.Y); result != StreamResult::Success) return result;
		if (auto result = ReadFCurvePtr(reader, out.Z); result != StreamResult::Success) return result;
		return StreamResult::Success;
	}

	static StreamResult ReadLayerVideo2D(StreamReader& reader, LayerVideo2D& out)
	{
		if (auto result = ReadFCurve2DPtr(reader, out.Origin); result != StreamResult::Success) return result;
		if (auto result = ReadFCurve2DPtr(reader, out.Position); result != StreamResult::Success) return result;
		if (auto result = ReadFCurvePtr(reader, out.Rotation); result != StreamResult::Success) return result;
		if (auto result = ReadFCurve2DPtr(reader, out.Scale); result != StreamResult::Success) return result;
		if (auto result = ReadFCurvePtr(reader, out.Opacity); result != StreamResult::Success) return result;
		return StreamResult::Success;
	}

	static StreamResult ReadLayerVideo3D(StreamReader& reader, LayerVideo3D& out)
	{
		if (auto result = ReadFCurvePtr(reader, out.OriginZ); result != StreamResult::Success) return result;
		if (auto result = ReadFCurvePtr(reader, out.PositionZ); result != StreamResult::Success) return result;
		if (auto result = ReadFCurve3DPtr(reader, out.DirectionXYZ); result != StreamResult::Success) return result;
		if (auto result = ReadFCurve2DPtr(reader, out.RotationXY); result != StreamResult::Success) return result;
		if (auto result = ReadFCurvePtr(reader, out.ScaleZ); result != StreamResult::Success) return result;
		return StreamResult::Success;
	}

	template <typename FlagsStruct>
	static void WriteFlagsBitfieldStruct(StreamWriter& writer, const FlagsStruct& in)
	{
		if constexpr (sizeof(FlagsStruct) == sizeof(u8))
			writer.WriteU8(*reinterpret_cast<const u8*>(&in));
		else if constexpr (sizeof(FlagsStruct) == sizeof(u16))
			writer.WriteU16(*reinterpret_cast<const u8*>(&in));
		else if constexpr (sizeof(FlagsStruct) == sizeof(u32))
			writer.WriteU32(*reinterpret_cast<const u32*>(&in));
		else
			static_assert(false);
	}

	static void WriteFCurvePtr(StreamWriter& writer, const FCurve& in)
	{
		if (in->size() > 0)
		{
			writer.WriteU32(static_cast<u32>(in->size()));
			writer.WriteFuncPtr([&in](StreamWriter& writer)
			{
				if (in->size() == 1)
				{
					writer.WriteF32(in->front().Value);
				}
				else
				{
					for (auto& keyFrame : in.Keys)
						writer.WriteF32(keyFrame.Frame);

					for (auto& keyFrame : in.Keys)
					{
						writer.WriteF32(keyFrame.Value);
						writer.WriteF32(keyFrame.Tangent);
					}
				}
			});
		}
		else
		{
			writer.WriteU32(0x00000000);
			writer.WritePtr(FileAddr::NullPtr);
		}
	}

	static void WriteFCurve2DPtr(StreamWriter& writer, const FCurve2D& in)
	{
		WriteFCurvePtr(writer, in.X);
		WriteFCurvePtr(writer, in.Y);
	}

	static void WriteFCurve3DPtr(StreamWriter& writer, const FCurve3D& in)
	{
		WriteFCurvePtr(writer, in.X);
		WriteFCurvePtr(writer, in.Y);
		WriteFCurvePtr(writer, in.Z);
	}

	static void WriteLayerVideo2D(StreamWriter& writer, const LayerVideo2D& in)
	{
		WriteFCurve2DPtr(writer, in.Origin);
		WriteFCurve2DPtr(writer, in.Position);
		WriteFCurvePtr(writer, in.Rotation);
		WriteFCurve2DPtr(writer, in.Scale);
		WriteFCurvePtr(writer, in.Opacity);
	}

	static void WriteLayerVideo3D(StreamWriter& writer, const LayerVideo3D& in)
	{
		WriteFCurvePtr(writer, in.OriginZ);
		WriteFCurvePtr(writer, in.PositionZ);
		WriteFCurve3DPtr(writer, in.DirectionXYZ);
		WriteFCurve2DPtr(writer, in.RotationXY);
		WriteFCurvePtr(writer, in.ScaleZ);
	}

	static void ReadLayerVideo(StreamReader& reader, std::shared_ptr<LayerVideo>& out)
	{
		out = std::make_shared<LayerVideo>();
		out->TransferMode.BlendMode = static_cast<BlendMode>(reader.ReadU8());
		out->TransferMode.Flags = ReadFlagsStruct<TransferFlags>(reader);
		out->TransferMode.TrackMatte = static_cast<TrackMatte>(reader.ReadU8());
		reader.Skip(static_cast<FileAddr>(sizeof(u8)));

		if (reader.GetPtrSize() == PtrSize::Mode64Bit)
			reader.ReadU32();

		ReadLayerVideo2D(reader, out->Transform);

		const auto perspectivePropertiesOffset = reader.ReadPtr();
		if (perspectivePropertiesOffset != FileAddr::NullPtr)
		{
			reader.ReadAtOffsetAware(perspectivePropertiesOffset, [out](StreamReader& reader)
			{
				out->Transform3D = std::make_shared<LayerVideo3D>();
				ReadLayerVideo3D(reader, *out->Transform3D);
			});
		}
	}

	static void SetFCurveStartFrame(FCurve& inOut, frame_t startFrame)
	{
		if (inOut->size() == 1)
			inOut->front().Frame = startFrame;
	}

	static void SetLayerVideoStartFrame(LayerVideo2D& inOut, frame_t startFrame)
	{
		SetFCurveStartFrame(inOut.Origin.X, startFrame);
		SetFCurveStartFrame(inOut.Origin.Y, startFrame);
		SetFCurveStartFrame(inOut.Position.X, startFrame);
		SetFCurveStartFrame(inOut.Position.Y, startFrame);
		SetFCurveStartFrame(inOut.Rotation, startFrame);
		SetFCurveStartFrame(inOut.Scale.X, startFrame);
		SetFCurveStartFrame(inOut.Scale.Y, startFrame);
		SetFCurveStartFrame(inOut.Opacity, startFrame);
	}

	static void SetLayerVideoStartFrame(LayerVideo3D& inOut, frame_t startFrame)
	{
		SetFCurveStartFrame(inOut.OriginZ, startFrame);
		SetFCurveStartFrame(inOut.PositionZ, startFrame);
		SetFCurveStartFrame(inOut.DirectionXYZ.X, startFrame);
		SetFCurveStartFrame(inOut.DirectionXYZ.Y, startFrame);
		SetFCurveStartFrame(inOut.DirectionXYZ.Z, startFrame);
		SetFCurveStartFrame(inOut.RotationXY.X, startFrame);
		SetFCurveStartFrame(inOut.RotationXY.Y, startFrame);
		SetFCurveStartFrame(inOut.ScaleZ, startFrame);
	}

	void Layer::Read(StreamReader& reader)
	{
		InternalFilePosition = reader.GetPositionOffsetAware();
		Name = reader.ReadStrPtrOffsetAware();
		StartFrame = reader.ReadF32();
		EndFrame = reader.ReadF32();
		StartOffset = reader.ReadF32();
		TimeScale = reader.ReadF32();

		Flags = ReadFlagsStruct<LayerFlags>(reader);
		Quality = static_cast<LayerQuality>(reader.ReadU8());
		ItemType = static_cast<Aet::ItemType>(reader.ReadU8());

		if (reader.GetPtrSize() == PtrSize::Mode64Bit)
			reader.ReadU32();

		InternalItemFileOffset = reader.ReadPtr();
		InternalParentFileOffset = reader.ReadPtr();

		const auto markerCount = reader.ReadSize();
		const auto markersOffset = reader.ReadPtr();

		if (markerCount > 0 && markersOffset != FileAddr::NullPtr)
		{
			Markers.reserve(markerCount);
			for (size_t i = 0; i < markerCount; i++)
				Markers.push_back(std::make_shared<Marker>());

			reader.ReadAtOffsetAware(markersOffset, [this](StreamReader& reader)
			{
				for (auto& marker : Markers)
				{
					marker->Frame = reader.ReadF32();
					if (reader.GetPtrSize() == PtrSize::Mode64Bit)
						reader.ReadU32();
					marker->Name = reader.ReadStrPtrOffsetAware();
				}
			});
		}

		const auto layerVideoOffset = reader.ReadPtr();
		if (layerVideoOffset != FileAddr::NullPtr)
		{
			reader.ReadAtOffsetAware(layerVideoOffset, [&](StreamReader& reader)
			{
				ReadLayerVideo(reader, this->LayerVideo);
				SetLayerVideoStartFrame(this->LayerVideo->Transform, StartFrame);

				if (this->LayerVideo->Transform3D != nullptr)
					SetLayerVideoStartFrame(*this->LayerVideo->Transform3D, StartFrame);
			});
		}

		InternalAudioDataFileOffset = reader.ReadPtr();
	}

	void Scene::Read(StreamReader& reader)
	{
		Name = reader.ReadStrPtrOffsetAware();
		StartFrame = reader.ReadF32();
		EndFrame = reader.ReadF32();
		FrameRate = reader.ReadF32();
		BackgroundColor = ReadU32ColorRGB(reader);
		Resolution = reader.ReadIVec2();

		const auto cameraOffset = reader.ReadPtr();
		if (cameraOffset != FileAddr::NullPtr)
		{
			Camera = std::make_shared<Aet::Camera>();
			reader.ReadAtOffsetAware(cameraOffset, [this](StreamReader& reader)
			{
				ReadFCurve3DPtr(reader, Camera->Eye);
				ReadFCurve3DPtr(reader, Camera->Position);
				ReadFCurve3DPtr(reader, Camera->Direction);
				ReadFCurve3DPtr(reader, Camera->Rotation);
				ReadFCurvePtr(reader, Camera->Zoom);
			});
		}

		const auto compCount = reader.ReadSize();
		const auto compsOffset = reader.ReadPtr();
		if (compCount > 0 && compsOffset != FileAddr::NullPtr)
		{
			Compositions.resize(compCount - 1);
			reader.ReadAtOffsetAware(compsOffset, [this](StreamReader& reader)
			{
				const auto readMakeComp = [](StreamReader& reader, std::shared_ptr<Composition>& comp)
				{
					comp = std::make_shared<Composition>();
					comp->InternalFilePosition = reader.GetPositionOffsetAware();

					const auto layerCount = reader.ReadSize();
					const auto layersOffset = reader.ReadPtr();

					if (layerCount > 0 && layersOffset != FileAddr::NullPtr)
					{
						comp->Layers.resize(layerCount);
						reader.ReadAtOffsetAware(layersOffset, [&comp](StreamReader& reader)
						{
							for (auto& layer : comp->Layers)
							{
								layer = std::make_shared<Layer>();
								layer->Read(reader);
							}
						});
					}
				};

				for (auto& comp : Compositions)
					readMakeComp(reader, comp);

				readMakeComp(reader, RootComposition);
			});
		}

		const auto videoCount = reader.ReadSize();
		const auto videosOffset = reader.ReadPtr();
		if (videoCount > 0 && videosOffset != FileAddr::NullPtr)
		{
			Videos.reserve(videoCount);
			reader.ReadAtOffsetAware(videosOffset, [&](StreamReader& reader)
			{
				for (size_t i = 0; i < videoCount; i++)
				{
					auto& video = *Videos.emplace_back(std::make_shared<Video>());
					video.InternalFilePosition = reader.GetPositionOffsetAware();
					video.Color = ReadU32ColorRGB(reader);
					video.Size.x = reader.ReadU16();
					video.Size.y = reader.ReadU16();
					video.FilesPerFrame = reader.ReadF32();

					const auto spriteCount = reader.ReadU32();
					const auto spritesOffset = reader.ReadPtr();

					if (spriteCount > 0 && spritesOffset != FileAddr::NullPtr)
					{
						video.Sources.resize(spriteCount);
						reader.ReadAtOffsetAware(spritesOffset, [&video](StreamReader& reader)
						{
							for (auto& source : video.Sources)
							{
								source.Name = reader.ReadStrPtrOffsetAware();
								source.ID = SprID(reader.ReadU32());
							}
						});
					}
				}
			});
		}

		const auto audioCount = reader.ReadSize();
		const auto audiosOffset = reader.ReadPtr();
		if (audioCount > 0 && audiosOffset != FileAddr::NullPtr)
		{
			Audios.reserve(audioCount);
			reader.ReadAtOffsetAware(audiosOffset, [&](StreamReader& reader)
			{
				for (size_t i = 0; i < audioCount; i++)
				{
					auto& audio = *Audios.emplace_back(std::make_shared<Audio>());
					audio.InternalFilePosition = reader.GetPositionOffsetAware();
					audio.SoundID = reader.ReadU32();
				}
			});
		}
	}

	void Scene::Write(StreamWriter& writer)
	{
		writer.WriteFuncPtr([this](StreamWriter& writer)
		{
			writer.WriteStrPtr(Name);
			writer.WriteF32(StartFrame);
			writer.WriteF32(EndFrame);
			writer.WriteF32(FrameRate);
			writer.WriteU32(BackgroundColor);
			writer.WriteI32(Resolution.x);
			writer.WriteI32(Resolution.y);

			if (this->Camera != nullptr)
			{
				writer.WriteFuncPtr([this](StreamWriter& writer)
				{
					WriteFCurve3DPtr(writer, Camera->Eye);
					WriteFCurve3DPtr(writer, Camera->Position);
					WriteFCurve3DPtr(writer, Camera->Position);
					WriteFCurve3DPtr(writer, Camera->Direction);
					WriteFCurve3DPtr(writer, Camera->Rotation);
					WriteFCurvePtr(writer, Camera->Zoom);
				});
			}
			else
			{
				writer.WritePtr(FileAddr::NullPtr);
			}

			assert(RootComposition != nullptr);
			writer.WriteU32(static_cast<u32>(Compositions.size()) + 1);
			writer.WriteFuncPtr([this](StreamWriter& writer)
			{
				const auto writeComp = [](StreamWriter& writer, const std::shared_ptr<Composition>& comp)
				{
					comp->InternalFilePosition = writer.GetPosition();
					if (comp->Layers.size() > 0)
					{
						writer.WriteU32(static_cast<u32>(comp->Layers.size()));
						writer.WriteFuncPtr([&comp](StreamWriter& writer)
						{
							for (auto& layer : comp->Layers)
							{
								layer->InternalFilePosition = writer.GetPosition();
								writer.WriteStrPtr(layer->Name);
								writer.WriteF32(layer->StartFrame);
								writer.WriteF32(layer->EndFrame);
								writer.WriteF32(layer->StartOffset);
								writer.WriteF32(layer->TimeScale);
								WriteFlagsBitfieldStruct<LayerFlags>(writer, layer->Flags);
								writer.WriteU8(static_cast<u8>(layer->Quality));
								writer.WriteU8(static_cast<u8>(layer->ItemType));

								FileAddr itemFileOffset = FileAddr::NullPtr;
								if (layer->ItemType == ItemType::Video && layer->GetVideoItem() != nullptr)
									itemFileOffset = layer->GetVideoItem()->InternalFilePosition;
								else if (layer->ItemType == ItemType::Audio && layer->GetAudioItem() != nullptr)
									itemFileOffset = layer->GetAudioItem()->InternalFilePosition;
								else if (layer->ItemType == ItemType::Composition && layer->GetCompItem() != nullptr)
									itemFileOffset = layer->GetCompItem()->InternalFilePosition;

								if (itemFileOffset != FileAddr::NullPtr)
								{
									writer.WriteDelayedPtr([itemFileOffset](StreamWriter& writer)
									{
										writer.WritePtr(itemFileOffset);
									});
								}
								else
								{
									writer.WritePtr(FileAddr::NullPtr);
								}

								if (layer->GetRefParentLayer() != nullptr)
								{
									writer.WriteDelayedPtr([&layer](StreamWriter& writer)
									{
										writer.WritePtr(layer->GetRefParentLayer()->InternalFilePosition);
									});
								}
								else
								{
									writer.WritePtr(FileAddr::NullPtr);
								}

								if (layer->Markers.size() > 0)
								{
									writer.WriteU32(static_cast<u32>(layer->Markers.size()));
									writer.WriteFuncPtr([&layer](StreamWriter& writer)
									{
										for (auto& marker : layer->Markers)
										{
											writer.WriteF32(marker->Frame);
											writer.WriteStrPtr(marker->Name);
										}
									});
								}
								else
								{
									writer.WriteU32(0x00000000);
									writer.WritePtr(FileAddr::NullPtr);
								}

								if (layer->LayerVideo != nullptr)
								{
									LayerVideo& layerVideo = *layer->LayerVideo;
									writer.WriteFuncPtr([&layerVideo](StreamWriter& writer)
									{
										writer.WriteU8(static_cast<u8>(layerVideo.TransferMode.BlendMode));
										WriteFlagsBitfieldStruct<TransferFlags>(writer, layerVideo.TransferMode.Flags);
										writer.WriteU8(static_cast<u8>(layerVideo.TransferMode.TrackMatte));
										writer.WriteU8(0xCC);

										WriteLayerVideo2D(writer, layerVideo.Transform);

										if (layerVideo.Transform3D != nullptr)
										{
											writer.WriteFuncPtr([&layerVideo](StreamWriter& writer)
											{
												WriteLayerVideo3D(writer, *layerVideo.Transform3D);
												writer.WriteAlignmentPadding(16);
											});
										}
										else
										{
											writer.WritePtr(FileAddr::NullPtr);
										}

										writer.WriteAlignmentPadding(16);
									});
								}
								else
								{
									writer.WritePtr(FileAddr::NullPtr);
								}

								// TODO: audioDataFilePtr
								writer.WritePtr(FileAddr::NullPtr);
							}

							writer.WriteAlignmentPadding(16);
						});
					}
					else
					{
						writer.WriteU32(0x00000000);
						writer.WritePtr(FileAddr::NullPtr);
					}
				};

				for (auto& comp : Compositions)
					writeComp(writer, comp);
				writeComp(writer, RootComposition);

				writer.WriteAlignmentPadding(16);
			});

			if (!Videos.empty())
			{
				writer.WriteU32(static_cast<u32>(Videos.size()));
				writer.WriteFuncPtr([this](StreamWriter& writer)
				{
					for (auto& video : Videos)
					{
						video->InternalFilePosition = writer.GetPosition();
						writer.WriteU32(video->Color);
						writer.WriteI16(static_cast<i16>(video->Size.x));
						writer.WriteI16(static_cast<i16>(video->Size.y));
						writer.WriteF32(video->FilesPerFrame);
						if (!video->Sources.empty())
						{
							writer.WriteU32(static_cast<u32>(video->Sources.size()));
							writer.WriteFuncPtr([&video](StreamWriter& writer)
							{
								for (auto& source : video->Sources)
								{
									writer.WriteStrPtr(source.Name);
									writer.WriteU32(static_cast<u32>(source.ID));
								}
							});
						}
						else
						{
							writer.WriteU32(0x00000000);
							writer.WritePtr(FileAddr::NullPtr);
						}
					}
					writer.WriteAlignmentPadding(16);
				});
			}
			else
			{
				writer.WriteU32(0x00000000);
				writer.WritePtr(FileAddr::NullPtr);
			}

			if (Audios.size() > 0)
			{
				writer.WriteU32(static_cast<u32>(Audios.size()));
				writer.WriteFuncPtr([this](StreamWriter& writer)
				{
					for (auto& audio : Audios)
					{
						audio->InternalFilePosition = writer.GetPosition();
						writer.WriteU32(audio->SoundID);
					}
					writer.WriteAlignmentPadding(16);
				});
			}
			else
			{
				writer.WriteU32(0x00000000);
				writer.WritePtr(FileAddr::NullPtr);
			}

			writer.WriteAlignmentPadding(16);
		});
	}

	StreamResult AetSet::Read(StreamReader& reader)
	{
		const auto baseHeader = SectionHeader::TryRead(reader, SectionSignature::AETC);
		SectionHeader::ScanPOFSectionsForPtrSize(reader);

		if (baseHeader.has_value())
		{
			reader.SetEndianness(baseHeader->Endianness);
			reader.Seek(baseHeader->StartOfSubSectionAddress());

			if (reader.GetPtrSize() == PtrSize::Mode64Bit)
				reader.PushBaseOffset();
		}

		size_t sceneCount = 0;
		reader.ReadAt(reader.GetPosition(), [&](StreamReader& reader)
		{
			while (reader.ReadPtr() != FileAddr::NullPtr)
				sceneCount++;
		});

		Scenes.reserve(sceneCount);
		for (size_t i = 0; i < sceneCount; i++)
		{
			const auto sceneOffset = reader.ReadPtr();
			if (!reader.IsValidPointer(sceneOffset))
				return StreamResult::BadPointer;

			auto& scene = *Scenes.emplace_back(std::make_shared<Scene>());
			reader.ReadAtOffsetAware(sceneOffset, [&](StreamReader& reader)
			{
				scene.Read(reader);
			});

			scene.InternalUpdateParentPointers();
			scene.InternalLinkPostRead();
			scene.InternalUpdateCompNamesAfterLayerItems();
		}

		if (baseHeader.has_value() && reader.GetPtrSize() == PtrSize::Mode64Bit)
			reader.PopBaseOffset();

		return StreamResult::Success;
	}

	StreamResult AetSet::Write(StreamWriter& writer)
	{
		for (auto& scene : Scenes)
		{
			assert(scene != nullptr);
			scene->Write(writer);
		}

		writer.WritePtr(FileAddr::NullPtr);
		writer.WriteAlignmentPadding(16);

		writer.FlushPointerPool();
		writer.WriteAlignmentPadding(16);

		writer.FlushStringPointerPool();
		writer.WriteAlignmentPadding(16);

		writer.FlushDelayedWritePool();

		return StreamResult::Success;
	}
}
