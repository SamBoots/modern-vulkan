#pragma once
#include "Common.h"

namespace BB
{
	namespace Render
	{
		struct Vertex
		{
			float3 pos;
			float3 normal;
			float2 uv;
			float3 color;
		};
		struct BufferView;
		using RDescriptor = FrameworkHandle<struct RDescriptorTag>;
		using RBuffer = FrameworkHandle<struct RBufferTag>;
		using MeshHandle = FrameworkHandle<struct MeshHandleTag>;
	}
}