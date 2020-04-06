#pragma once
#include "Types.h"

#ifdef COMFY_D3D11
#include "Graphics/D3D11/Texture/Texture.h"
#include "Graphics/D3D11/Texture/RenderTarget.h"
#include "Graphics/D3D11/Buffer/IndexBuffer.h"
#include "Graphics/D3D11/Buffer/VertexBuffer.h"
#endif

namespace Comfy::Graphics
{
	// NOTE: GPU resource aliases to hide away the graphics API implementation without relying on virtual interfaces, for now

#ifdef COMFY_D3D11
	using GPU_Texture2D = D3D11::Texture2D;
	using GPU_CubeMap = D3D11::CubeMap;
	using GPU_RenderTarget = D3D11::RenderTarget;
	using GPU_IndexBuffer = D3D11::StaticIndexBuffer;
	using GPU_VertexBuffer = D3D11::StaticVertexBuffer;
#else
	class GPU_Texture2D {};
	class GPU_CubeMap {};
	class GPU_RenderTarget {};
	class GPU_IndexBuffer {};
	class GPU_VertexBuffer {};
#endif

#if 0
	struct Txp;
	struct LightMapIBL;

	namespace GPU
	{
		UniquePtr<GPU_Texture2D> MakeTexture2D(const Txp& txp, const char* debugName = nullptr);
		UniquePtr<GPU_Texture2D> MakeTexture2D(ivec2 size, const uint32_t* rgbaBuffer, const char* debugName = nullptr);

		UniquePtr<GPU_CubeMap> MakeCubeMap(const Txp& txp, const char* debugName = nullptr);
		UniquePtr<GPU_CubeMap> MakeCubeMap(const LightMapIBL& lightMap, const char* debugName = nullptr);

		UniquePtr<GPU_IndexBuffer> MakeIndexBuffer(size_t dataSize, const void* data, IndexFormat indexFormat, const char* debugName = nullptr);
		UniquePtr<GPU_VertexBuffer> MakeVertexBuffer(size_t dataSize, const void* data, size_t stride, const char* debugName = nullptr);
	}
#endif
}
