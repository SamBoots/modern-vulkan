#pragma once
#include "Common.h"

namespace BB
{
	namespace Render
	{
		struct BufferView;

		using RCommandPool = FrameworkHandle<struct RCommandPoolTag>;
		using RCommandList = FrameworkHandle<struct RCommandListTag>;

		using ShaderObject = FrameworkHandle<struct ShaderObjectTag>;
		using RDescriptorLayout = FrameworkHandle<struct RDescriptorLayoutTag>;
		using RBuffer = FrameworkHandle<struct RBufferTag>;
		using MeshHandle = FrameworkHandle<struct MeshHandleTag>;

		struct Vertex
		{
			float3 pos;
			float3 normal;
			float2 uv;
			float3 color;
		};

		struct CommandList //MUST BE 8 BYTES!!!! it's just an interface.
		{
			RCommandList api_cmd_list;
		};
	}
}