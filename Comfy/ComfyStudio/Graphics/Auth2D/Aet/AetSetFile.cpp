#include "AetSet.h"
#include "FileSystem/BinaryReader.h"
#include "FileSystem/BinaryWriter.h"

using namespace Comfy::FileSystem;

namespace Comfy::Graphics::Aet
{
	namespace
	{
		template <typename FlagsStruct>
		FlagsStruct ReadFlagsStruct(BinaryReader& reader)
		{
			if constexpr (sizeof(FlagsStruct) == sizeof(uint8_t))
			{
				const auto uintFlags = reader.ReadU8();
				return *reinterpret_cast<const FlagsStruct*>(&uintFlags);
			}
			else if constexpr (sizeof(FlagsStruct) == sizeof(uint16_t))
			{
				const auto uintFlags = reader.ReadU16();
				return *reinterpret_cast<const FlagsStruct*>(&uintFlags);
			}
			else if constexpr (sizeof(FlagsStruct) == sizeof(uint32_t))
			{
				const auto uintFlags = reader.ReadU32();
				return *reinterpret_cast<const FlagsStruct*>(&uintFlags);
			}
			else
			{
				static_assert(false);
			}
		}

		uint32_t ReadColor(BinaryReader& reader)
		{
			uint32_t value = 0;
			*(reinterpret_cast<uint8_t*>(&value) + 0) = reader.ReadU8();
			*(reinterpret_cast<uint8_t*>(&value) + 1) = reader.ReadU8();
			*(reinterpret_cast<uint8_t*>(&value) + 2) = reader.ReadU8();
			const auto padding = reader.ReadU8();
			return value;
		}

		void ReadProperty1DPointer(Property1D& property, BinaryReader& reader)
		{
			size_t keyFrameCount = reader.ReadSize();
			FileAddr keyFramesPointer = reader.ReadPtr();

			if (keyFrameCount > 0 && keyFramesPointer != FileAddr::NullPtr)
			{
				reader.ReadAt(keyFramesPointer, [keyFrameCount, &property](BinaryReader& reader)
				{
					property->resize(keyFrameCount);

					if (keyFrameCount == 1)
					{
						property->front().Value = reader.ReadF32();
					}
					else
					{
						for (size_t i = 0; i < keyFrameCount; i++)
							property.Keys[i].Frame = reader.ReadF32();

						for (size_t i = 0; i < keyFrameCount; i++)
						{
							property.Keys[i].Value = reader.ReadF32();
							property.Keys[i].Curve = reader.ReadF32();
						}
					}
				});
			}
		}

		void ReadProperty2DPointer(Property2D& property, BinaryReader& reader)
		{
			ReadProperty1DPointer(property.X, reader);
			ReadProperty1DPointer(property.Y, reader);
		}

		void ReadProperty3DPointer(Property3D& property, BinaryReader& reader)
		{
			ReadProperty1DPointer(property.X, reader);
			ReadProperty1DPointer(property.Y, reader);
			ReadProperty1DPointer(property.Z, reader);
		}

		void ReadLayerVideo2D(LayerVideo2D& transform, BinaryReader& reader)
		{
			ReadProperty2DPointer(transform.Origin, reader);
			ReadProperty2DPointer(transform.Position, reader);
			ReadProperty1DPointer(transform.Rotation, reader);
			ReadProperty2DPointer(transform.Scale, reader);
			ReadProperty1DPointer(transform.Opacity, reader);
		}

		void ReadLayerVideo3D(LayerVideo3D& transform, BinaryReader& reader)
		{
			ReadProperty1DPointer(transform.AnchorZ, reader);
			ReadProperty1DPointer(transform.PositionZ, reader);
			ReadProperty3DPointer(transform.Direction, reader);
			ReadProperty2DPointer(transform.Rotation, reader);
			ReadProperty1DPointer(transform.ScaleZ, reader);
		}

		template <typename FlagsStruct>
		void WriteFlagsStruct(const FlagsStruct& flags, BinaryWriter& writer)
		{
			if constexpr (sizeof(FlagsStruct) == sizeof(uint8_t))
				writer.WriteU8(*reinterpret_cast<const uint8_t*>(&flags));
			else if constexpr (sizeof(FlagsStruct) == sizeof(uint16_t))
				writer.WriteU16(*reinterpret_cast<const uint8_t*>(&flags));
			else if constexpr (sizeof(FlagsStruct) == sizeof(uint32_t))
				writer.WriteU32(*reinterpret_cast<const uint32_t*>(&flags));
			else
				static_assert(false);
		}

		void WriteProperty1DPointer(const Property1D& property, BinaryWriter& writer)
		{
			if (property->size() > 0)
			{
				writer.WriteU32(static_cast<uint32_t>(property->size()));
				writer.WritePtr([&property](BinaryWriter& writer)
				{
					if (property->size() == 1)
					{
						writer.WriteF32(property->front().Value);
					}
					else
					{
						for (auto& keyFrame : property.Keys)
							writer.WriteF32(keyFrame.Frame);

						for (auto& keyFrame : property.Keys)
						{
							writer.WriteF32(keyFrame.Value);
							writer.WriteF32(keyFrame.Curve);
						}
					}
				});
			}
			else
			{
				writer.WriteU32(0x00000000);		// key frames count
				writer.WritePtr(FileAddr::NullPtr); // key frames offset
			}
		}

		void WriteProperty2DPointer(const Property2D& property, BinaryWriter& writer)
		{
			WriteProperty1DPointer(property.X, writer);
			WriteProperty1DPointer(property.Y, writer);
		}

		void WriteProperty3DPointer(const Property3D& property, BinaryWriter& writer)
		{
			WriteProperty1DPointer(property.X, writer);
			WriteProperty1DPointer(property.Y, writer);
			WriteProperty1DPointer(property.Z, writer);
		}

		void WriteTransform(const LayerVideo2D& transform, BinaryWriter& writer)
		{
			WriteProperty2DPointer(transform.Origin, writer);
			WriteProperty2DPointer(transform.Position, writer);
			WriteProperty1DPointer(transform.Rotation, writer);
			WriteProperty2DPointer(transform.Scale, writer);
			WriteProperty1DPointer(transform.Opacity, writer);
		}

		void WriteTransform(const LayerVideo3D& transform, BinaryWriter& writer)
		{
			WriteProperty1DPointer(transform.AnchorZ, writer);
			WriteProperty1DPointer(transform.PositionZ, writer);
			WriteProperty3DPointer(transform.Direction, writer);
			WriteProperty2DPointer(transform.Rotation, writer);
			WriteProperty1DPointer(transform.ScaleZ, writer);
		}

		void ReadLayerVideo(RefPtr<LayerVideo>& layerVideo, BinaryReader& reader)
		{
			layerVideo = MakeRef<LayerVideo>();
			layerVideo->TransferMode.BlendMode = static_cast<AetBlendMode>(reader.ReadU8());
			layerVideo->TransferMode.Flags = ReadFlagsStruct<TransferFlags>(reader);
			layerVideo->TransferMode.TrackMatte = static_cast<TrackMatte>(reader.ReadU8());
			reader.ReadU8();

			if (reader.GetPointerMode() == PtrMode::Mode64Bit)
				reader.ReadU32();

			ReadLayerVideo2D(layerVideo->Transform, reader);

			FileAddr perspectivePropertiesPointer = reader.ReadPtr();
			if (perspectivePropertiesPointer != FileAddr::NullPtr)
			{
				reader.ReadAt(perspectivePropertiesPointer, [layerVideo](BinaryReader& reader)
				{
					layerVideo->Transform3D = MakeRef<LayerVideo3D>();
					ReadLayerVideo3D(*layerVideo->Transform3D, reader);
				});
			}
		}

		void SetProperty1DStartFrame(Property1D& property, frame_t startFrame)
		{
			if (property->size() == 1)
				property->front().Frame = startFrame;
		}

		void SetLayerVideoStartFrame(LayerVideo2D& transform, frame_t startFrame)
		{
			SetProperty1DStartFrame(transform.Origin.X, startFrame);
			SetProperty1DStartFrame(transform.Origin.Y, startFrame);
			SetProperty1DStartFrame(transform.Position.X, startFrame);
			SetProperty1DStartFrame(transform.Position.Y, startFrame);
			SetProperty1DStartFrame(transform.Rotation, startFrame);
			SetProperty1DStartFrame(transform.Scale.X, startFrame);
			SetProperty1DStartFrame(transform.Scale.Y, startFrame);
			SetProperty1DStartFrame(transform.Opacity, startFrame);
		}

		void SetLayerVideoStartFrame(LayerVideo3D& transform, frame_t startFrame)
		{
			SetProperty1DStartFrame(transform.AnchorZ, startFrame);
			SetProperty1DStartFrame(transform.PositionZ, startFrame);
			SetProperty1DStartFrame(transform.Direction.X, startFrame);
			SetProperty1DStartFrame(transform.Direction.Y, startFrame);
			SetProperty1DStartFrame(transform.Direction.Z, startFrame);
			SetProperty1DStartFrame(transform.Rotation.X, startFrame);
			SetProperty1DStartFrame(transform.Rotation.Y, startFrame);
			SetProperty1DStartFrame(transform.ScaleZ, startFrame);
		}
	}

	void Layer::Read(BinaryReader& reader)
	{
		filePosition = reader.GetPosition();
		name = reader.ReadStrPtr();
		StartFrame = reader.ReadF32();
		EndFrame = reader.ReadF32();
		StartOffset = reader.ReadF32();
		TimeScale = reader.ReadF32();

		Flags = ReadFlagsStruct<LayerFlags>(reader);
		Quality = static_cast<LayerQuality>(reader.ReadU8());
		ItemType = static_cast<Aet::ItemType>(reader.ReadU8());

		if (reader.GetPointerMode() == PtrMode::Mode64Bit)
			reader.ReadU32();

		itemFilePtr = reader.ReadPtr();
		parentFilePtr = reader.ReadPtr();

		size_t markerCount = reader.ReadSize();
		FileAddr markersPointer = reader.ReadPtr();

		if (markerCount > 0 && markersPointer != FileAddr::NullPtr)
		{
			Markers.reserve(markerCount);
			for (size_t i = 0; i < markerCount; i++)
				Markers.push_back(MakeRef<Marker>());

			reader.ReadAt(markersPointer, [this](BinaryReader& reader)
			{
				for (auto& marker : Markers)
				{
					marker->Frame = reader.ReadF32();
					if (reader.GetPointerMode() == PtrMode::Mode64Bit)
						reader.ReadU32();
					marker->Name = reader.ReadStrPtr();
				}
			});
		}

		FileAddr layerVideoPointer = reader.ReadPtr();
		if (layerVideoPointer != FileAddr::NullPtr)
		{
			reader.ReadAt(layerVideoPointer, [this](BinaryReader& reader)
			{
				ReadLayerVideo(this->LayerVideo, reader);
				SetLayerVideoStartFrame(this->LayerVideo->Transform, StartFrame);
				if (this->LayerVideo->Transform3D != nullptr)
					SetLayerVideoStartFrame(*this->LayerVideo->Transform3D, StartFrame);
			});
		}

		audioDataFilePtr = reader.ReadPtr();
	}

	void Scene::Read(BinaryReader& reader)
	{
		Name = reader.ReadStrPtr();
		StartFrame = reader.ReadF32();
		EndFrame = reader.ReadF32();
		FrameRate = reader.ReadF32();
		BackgroundColor = ReadColor(reader);
		Resolution = reader.ReadIV2();

		FileAddr cameraOffsetPtr = reader.ReadPtr();
		if (cameraOffsetPtr != FileAddr::NullPtr)
		{
			Camera = MakeRef<Aet::Camera>();
			reader.ReadAt(cameraOffsetPtr, [this](BinaryReader& reader)
			{
				ReadProperty3DPointer(Camera->Eye, reader);
				ReadProperty3DPointer(Camera->Position, reader);
				ReadProperty3DPointer(Camera->Direction, reader);
				ReadProperty3DPointer(Camera->Rotation, reader);
				ReadProperty1DPointer(Camera->Zoom, reader);
			});
		}

		size_t compCount = reader.ReadSize();
		FileAddr compsPtr = reader.ReadPtr();
		if (compCount > 0 && compsPtr != FileAddr::NullPtr)
		{
			Compositions.resize(compCount - 1);
			reader.ReadAt(compsPtr, [this](BinaryReader& reader)
			{
				const auto readCompFunction = [](BinaryReader& reader, RefPtr<Composition>& comp)
				{
					comp = MakeRef<Composition>();
					comp->filePosition = reader.GetPosition();

					size_t layerCount = reader.ReadSize();
					FileAddr layersPointer = reader.ReadPtr();

					if (layerCount > 0 && layersPointer != FileAddr::NullPtr)
					{
						comp->GetLayers().resize(layerCount);
						reader.ReadAt(layersPointer, [&comp](BinaryReader& reader)
						{
							for (auto& layer : comp->GetLayers())
							{
								layer = MakeRef<Layer>();
								layer->Read(reader);
							}
						});
					}
				};

				for (auto& comp : Compositions)
					readCompFunction(reader, comp);

				readCompFunction(reader, RootComposition);
			});
		}

		size_t videoCount = reader.ReadSize();
		FileAddr videosPtr = reader.ReadPtr();
		if (videoCount > 0 && videosPtr != FileAddr::NullPtr)
		{
			Videos.resize(videoCount);
			reader.ReadAt(videosPtr, [this](BinaryReader& reader)
			{
				for (auto& video : Videos)
				{
					video = MakeRef<Video>();
					video->filePosition = reader.GetPosition();
					video->Color = ReadColor(reader);
					video->Size.x = reader.ReadU16();
					video->Size.y = reader.ReadU16();
					video->Frames = reader.ReadF32();

					uint32_t spriteCount = reader.ReadU32();
					FileAddr spritesPointer = reader.ReadPtr();

					if (spriteCount > 0 && spritesPointer != FileAddr::NullPtr)
					{
						video->Sources.resize(spriteCount);
						reader.ReadAt(spritesPointer, [&video](BinaryReader& reader)
						{
							for (auto& source : video->Sources)
							{
								source.Name = reader.ReadStrPtr();
								source.ID = SprID(reader.ReadU32());
							}
						});
					}
				}
			});
		}

		size_t audioCount = reader.ReadSize();
		FileAddr audiosPtr = reader.ReadPtr();
		if (audioCount > 0 && audiosPtr != FileAddr::NullPtr)
		{
			Audios.resize(audioCount);
			reader.ReadAt(audiosPtr, [this](BinaryReader& reader)
			{
				for (auto& audio : Audios)
				{
					audio = MakeRef<Audio>();
					audio->filePosition = reader.GetPosition();
					audio->SoundID = reader.ReadU32();
				}
			});
		}
	}

	void Scene::Write(BinaryWriter& writer)
	{
		writer.WritePtr([this](BinaryWriter& writer)
		{
			FileAddr sceneFilePosition = writer.GetPosition();
			writer.WriteStrPtr(Name);
			writer.WriteF32(StartFrame);
			writer.WriteF32(EndFrame);
			writer.WriteF32(FrameRate);
			writer.WriteU32(BackgroundColor);
			writer.WriteI32(Resolution.x);
			writer.WriteI32(Resolution.y);

			if (this->Camera != nullptr)
			{
				writer.WritePtr([this](BinaryWriter& writer)
				{
					WriteProperty3DPointer(Camera->Eye, writer);
					WriteProperty3DPointer(Camera->Position, writer);
					WriteProperty3DPointer(Camera->Position, writer);
					WriteProperty3DPointer(Camera->Direction, writer);
					WriteProperty3DPointer(Camera->Rotation, writer);
					WriteProperty1DPointer(Camera->Zoom, writer);
				});
			}
			else
			{
				writer.WritePtr(FileAddr::NullPtr); // camera offset
			}

			assert(RootComposition != nullptr);
			writer.WriteU32(static_cast<uint32_t>(Compositions.size()) + 1);
			writer.WritePtr([this](BinaryWriter& writer)
			{
				const auto writeCompFunction = [](BinaryWriter& writer, const RefPtr<Composition>& comp)
				{
					comp->filePosition = writer.GetPosition();
					if (comp->GetLayers().size() > 0)
					{
						writer.WriteU32(static_cast<uint32_t>(comp->GetLayers().size()));
						writer.WritePtr([&comp](BinaryWriter& writer)
						{
							for (auto& layer : comp->GetLayers())
							{
								layer->filePosition = writer.GetPosition();
								writer.WriteStrPtr(layer->name);
								writer.WriteF32(layer->StartFrame);
								writer.WriteF32(layer->EndFrame);
								writer.WriteF32(layer->StartOffset);
								writer.WriteF32(layer->TimeScale);
								WriteFlagsStruct<LayerFlags>(layer->Flags, writer);
								writer.WriteU8(static_cast<uint8_t>(layer->Quality));
								writer.WriteU8(static_cast<uint8_t>(layer->ItemType));

								FileAddr itemFilePosition = FileAddr::NullPtr;
								if (layer->ItemType == ItemType::Video && layer->GetVideoItem() != nullptr)
									itemFilePosition = layer->GetVideoItem()->filePosition;
								else if (layer->ItemType == ItemType::Audio && layer->GetAudioItem() != nullptr)
									itemFilePosition = layer->GetAudioItem()->filePosition;
								else if (layer->ItemType == ItemType::Composition && layer->GetCompItem() != nullptr)
									itemFilePosition = layer->GetCompItem()->filePosition;

								if (itemFilePosition != FileAddr::NullPtr)
								{
									writer.WriteDelayedPtr([itemFilePosition](BinaryWriter& writer)
									{
										writer.WritePtr(itemFilePosition);
									});
								}
								else
								{
									writer.WritePtr(FileAddr::NullPtr); // item offset
								}

								if (layer->GetRefParentLayer() != nullptr)
								{
									writer.WriteDelayedPtr([&layer](BinaryWriter& writer)
									{
										writer.WritePtr(layer->GetRefParentLayer()->filePosition);
									});
								}
								else
								{
									writer.WritePtr(FileAddr::NullPtr); // parent offset
								}

								if (layer->Markers.size() > 0)
								{
									writer.WriteU32(static_cast<uint32_t>(layer->Markers.size()));
									writer.WritePtr([&layer](BinaryWriter& writer)
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
									writer.WriteU32(0x00000000);		// markers size
									writer.WritePtr(FileAddr::NullPtr); // markers offset
								}

								if (layer->LayerVideo != nullptr)
								{
									LayerVideo& layerVideo = *layer->LayerVideo;
									writer.WritePtr([&layerVideo](BinaryWriter& writer)
									{
										writer.WriteU8(static_cast<uint8_t>(layerVideo.TransferMode.BlendMode));
										WriteFlagsStruct<TransferFlags>(layerVideo.TransferMode.Flags, writer);
										writer.WriteU8(static_cast<uint8_t>(layerVideo.TransferMode.TrackMatte));
										writer.WriteU8(0xCC);

										WriteTransform(layerVideo.Transform, writer);

										if (layerVideo.Transform3D != nullptr)
										{
											writer.WritePtr([&layerVideo](BinaryWriter& writer)
											{
												WriteTransform(*layerVideo.Transform3D, writer);
												writer.WriteAlignmentPadding(16);
											});
										}
										else
										{
											writer.WritePtr(FileAddr::NullPtr); // LayerVideo3D offset
										}

										writer.WriteAlignmentPadding(16);
									});
								}
								else
								{
									writer.WritePtr(FileAddr::NullPtr); // LayerVideo offset
								}

								// TODO: audioDataFilePtr
								writer.WritePtr(FileAddr::NullPtr); // LayerAudio offset
							}

							writer.WriteAlignmentPadding(16);
						});
					}
					else
					{
						writer.WriteU32(0x00000000);		// compositions size
						writer.WritePtr(FileAddr::NullPtr); // compositions offset
					}
				};

				for (auto& comp : Compositions)
					writeCompFunction(writer, comp);
				writeCompFunction(writer, RootComposition);

				writer.WriteAlignmentPadding(16);
			});

			if (Videos.size() > 0)
			{
				writer.WriteU32(static_cast<uint32_t>(Videos.size()));
				writer.WritePtr([this](BinaryWriter& writer)
				{
					for (auto& video : Videos)
					{
						video->filePosition = writer.GetPosition();
						writer.WriteU32(video->Color);
						writer.WriteI16(static_cast<int16_t>(video->Size.x));
						writer.WriteI16(static_cast<int16_t>(video->Size.y));
						writer.WriteF32(video->Frames);
						if (!video->Sources.empty())
						{
							writer.WriteU32(static_cast<uint32_t>(video->Sources.size()));
							writer.WritePtr([&video](BinaryWriter& writer)
							{
								for (auto& source : video->Sources)
								{
									writer.WriteStrPtr(source.Name);
									writer.WriteU32(static_cast<uint32_t>(source.ID));
								}
							});
						}
						else
						{
							writer.WriteU32(0x00000000);		// sources size
							writer.WritePtr(FileAddr::NullPtr); // sources offset
						}
					}
					writer.WriteAlignmentPadding(16);
				});
			}
			else
			{
				writer.WriteU32(0x00000000);		// videos size
				writer.WritePtr(FileAddr::NullPtr); // videos offset
			}

			if (Audios.size() > 0)
			{
				writer.WriteU32(static_cast<uint32_t>(Audios.size()));
				writer.WritePtr([this](BinaryWriter& writer)
				{
					for (auto& audio : Audios)
					{
						audio->filePosition = writer.GetPosition();
						writer.WriteU32(audio->SoundID);
					}
					writer.WriteAlignmentPadding(16);
				});
			}
			else
			{
				writer.WriteU32(0x00000000);		// audios size
				writer.WritePtr(FileAddr::NullPtr);	// audios offset
			}

			writer.WriteAlignmentPadding(16);
		});
	}

	void AetSet::Read(BinaryReader& reader)
	{
		uint32_t signature = reader.ReadU32();
		if (signature == 'AETC' || signature == 'CTEA')
		{
			reader.SetEndianness(Endianness::Little);
			uint32_t dataSize = reader.ReadU32();
			uint32_t dataOffset = reader.ReadU32();
			uint32_t endianSignaure = reader.ReadU32();

			enum { LittleEndian = 0x10000000, BigEndian = 0x18000000 };

			reader.SetPosition(static_cast<FileAddr>(dataOffset));
			reader.SetEndianness(endianSignaure == LittleEndian ? Endianness::Little : Endianness::Big);

			if (dataOffset < 0x40)
			{
				reader.SetPointerMode(PtrMode::Mode64Bit);
				reader.SetStreamSeekOffset(static_cast<FileAddr>(dataOffset));
			}
		}
		else
		{
			reader.SetPosition(reader.GetPosition() - FileAddr(sizeof(signature)));
		}

		FileAddr startAddress = reader.GetPosition();

		size_t sceneCount = 0;
		while (reader.ReadPtr() != FileAddr::NullPtr)
			sceneCount++;
		scenes.reserve(sceneCount);

		reader.ReadAt(startAddress, [this, sceneCount](BinaryReader& reader)
		{
			for (size_t i = 0; i < sceneCount; i++)
			{
				scenes.push_back(MakeRef<Scene>());
				auto& scene = scenes.back();

				reader.ReadAt(reader.ReadPtr(), [&scene](BinaryReader& reader)
				{
					scene->Read(reader);
				});

				scene->UpdateParentPointers();
				scene->LinkPostRead();
				scene->UpdateCompNamesAfterLayerItems();
			}
		});
	}

	void AetSet::Write(BinaryWriter& writer)
	{
		for (auto& scene : scenes)
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
	}
}
