#include "TxpSet.h"
#include "Auth2D/SprSet.h"
#if 0
#include "Auth3D/ObjSet.h"
#endif
#include "FileSystem/FileInterface.h"
#include "FileSystem/BinaryReader.h"
#include "FileSystem/FileReader.h"

using namespace Comfy::FileSystem;

namespace Comfy::Graphics
{
	const std::vector<TxpMipMap>& Txp::GetMipMaps(uint32_t arrayIndex) const
	{
		return MipMapsArray[arrayIndex];
	}

	ivec2 Txp::GetSize() const
	{
		if (MipMapsArray.size() < 1 || MipMapsArray.front().size() < 1)
			return ivec2(0, 0);

		return MipMapsArray.front().front().Size;
	}

	TextureFormat Txp::GetFormat() const
	{
		if (MipMapsArray.size() < 1 || MipMapsArray.front().size() < 1)
			return TextureFormat::Unknown;

		return MipMapsArray.front().front().Format;
	}

	std::string_view Txp::GetName() const
	{
		return (Name.has_value()) ? Name.value() : UnknownName;
	}

	void TxpSet::Parse(const uint8_t* buffer, size_t bufferSize)
	{
		TxpSet& txpSet = *this;

		txpSet.Signature = *(TxpSig*)(buffer + 0);
		uint32_t textureCount = *(uint32_t*)(buffer + 4);
		uint32_t packedCount = *(uint32_t*)(buffer + 8);
		uint32_t* offsets = (uint32_t*)(buffer + 12);

		assert(txpSet.Signature.Type == TxpSig::TxpSet);

		Txps.reserve(textureCount);
		for (uint32_t i = 0; i < textureCount; i++)
		{
			Txps.push_back(MakeRef<Txp>());
			ParseTxp(buffer + offsets[i], *Txps[i]);
		}
	}

#if 0
	void TxpSet::UploadAll(SprSet* parentSprSet)
	{
		for (auto& txp : Txps)
		{
			const char* debugName = nullptr;

#if COMFY_D3D11_DEBUG_NAMES
			char debugNameBuffer[128];
			sprintf_s(debugNameBuffer, "%s %s: %s",
				(txp->Signature.Type == TxpSig::Texture2D) ? "Texture2D" : "CubeMap",
				(parentSprSet != nullptr) ? parentSprSet->Name.c_str() : "TxpSet",
				txp->GetName().data());

			debugName = debugNameBuffer;
#endif

			if (txp->Signature.Type == TxpSig::Texture2D)
				txp->GPU_Texture2D = GPU::MakeTexture2D(*txp, debugName);
			else if (txp->Signature.Type == TxpSig::CubeMap)
				txp->GPU_CubeMap = GPU::MakeCubeMap(*txp, debugName);
		}
	}

	void TxpSet::SetTextureIDs(const ObjSet& objSet)
	{
		const auto& textureIDs = objSet.TextureIDs;
		assert(textureIDs.size() <= Txps.size());

		for (size_t i = 0; i < textureIDs.size(); i++)
			Txps[i]->ID = textureIDs[i];
	}

	UniquePtr<TxpSet> TxpSet::MakeUniqueReadParseUpload(std::string_view filePath, const ObjSet* objSet)
	{
		std::vector<uint8_t> fileContent;
		FileSystem::FileReader::ReadEntireFile(filePath, &fileContent);

		if (fileContent.empty())
			return nullptr;

		auto txpSet = MakeUnique<TxpSet>();;
		{
			txpSet->Parse(fileContent.data(), fileContent.size());
			txpSet->UploadAll(nullptr);

			if (objSet != nullptr)
				txpSet->SetTextureIDs(*objSet);
		}
		return txpSet;
	}
#endif

	void TxpSet::ParseTxp(const uint8_t* buffer, Txp& txp)
	{
		txp.Signature = *(TxpSig*)(buffer + 0);
		uint32_t mipMapCount = *(uint32_t*)(buffer + 4);
		txp.MipLevels = *(uint8_t*)(buffer + 8);
		txp.ArraySize = *(uint8_t*)(buffer + 9);

		uint32_t* offsets = (uint32_t*)(buffer + 12);
		const uint8_t* mipMapBuffer = buffer + *offsets;
		++offsets;

		assert(txp.Signature.Type == TxpSig::Texture2D || txp.Signature.Type == TxpSig::CubeMap || txp.Signature.Type == TxpSig::Rectangle);
		// assert(mipMapCount == txp.MipLevels * txp.ArraySize);

		txp.MipMapsArray.resize(txp.ArraySize);

		for (auto& mipMaps : txp.MipMapsArray)
		{
			mipMaps.resize(txp.MipLevels);

			for (auto& mipMap : mipMaps)
			{
				mipMap.Signature = *(TxpSig*)(mipMapBuffer + 0);
				mipMap.Size = *(ivec2*)(mipMapBuffer + 4);
				mipMap.Format = *(TextureFormat*)(mipMapBuffer + 12);

				mipMap.MipIndex = *(uint8_t*)(mipMapBuffer + 16);
				mipMap.ArrayIndex = *(uint8_t*)(mipMapBuffer + 17);

				mipMap.DataPointerSize = *(uint32_t*)(mipMapBuffer + 20);
				mipMap.DataPointer = (mipMapBuffer + 24);

				mipMapBuffer = buffer + *offsets;
				++offsets;
			}
		}
	}
}
