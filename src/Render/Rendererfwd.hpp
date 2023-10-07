#pragma once
#include "Common.h"
//shared shader include
#include "shared_common.hlsl.h"

namespace BB
{
	struct BufferView;

	using RCommandPool = FrameworkHandle<struct RCommandPoolTag>;
	using RCommandList = FrameworkHandle<struct RCommandListTag>;

	using ShaderCode = FrameworkHandle<struct ShdaerCodeTag>;
	using ShaderObject = FrameworkHandle<struct ShaderObjectTag>;
	using RPipeline = FrameworkHandle<struct RPipelineTag>;
	using RDescriptorLayout = FrameworkHandle<struct RDescriptorLayoutTag>;
	using RPipelineLayout = FrameworkHandle<struct RPipelineLayoutTag>;
	using RBuffer = FrameworkHandle<struct RBufferTag>;
	using MeshHandle = FrameworkHandle<struct MeshHandleTag>;

	enum class SHADER_STAGE : uint32_t
	{
		NONE,
		ALL,
		VERTEX,
		FRAGMENT_PIXEL
	};
}