#pragma once

#include "Rendererfwd.hpp"
#include "Storage/FixedArray.h"

namespace BB
{
	using ShaderObject = FrameworkHandle<struct ShaderObjectTag>;

	enum class QUEUE_TYPE : uint32_t
	{
		GRAPHICS,
		TRANSFER,
		COMPUTE,

		ENUM_SIZE
	};

	enum class RESOURCE_TYPE : uint32_t
	{
		DESCRIPTOR_HEAP,
		DESCRIPTOR,
		QUEUE,
		COMMAND_ALLOCATOR,
		COMMANT_LIST,
		BUFFER,
		IMAGE,
		SAMPLER,
		FENCE,

		ENUM_SIZE
	};

	enum class SAMPLER_ADDRESS_MODE : uint32_t
	{
		REPEAT,
		MIRROR,
		BORDER,
		CLAMP,

		ENUM_SIZE
	};

	enum class SAMPLER_BORDER_COLOR : uint32_t
	{
		COLOR_FLOAT_TRANSPARENT_BLACK,
		COLOR_INT_TRANSPARENT_BLACK,
		COLOR_FLOAT_OPAQUE_BLACK,
		COLOR_INT_OPAQUE_BLACK,
		COLOR_FLOAT_OPAQUE_WHITE,
		COLOR_INT_OPAQUE_WHITE,

		ENUM_SIZE
	};

	enum class SAMPLER_FILTER : uint32_t
	{
		NEAREST,
		LINEAR
	};

	struct SamplerCreateInfo
	{
		const char* name;
		SAMPLER_ADDRESS_MODE mode_u{};
		SAMPLER_ADDRESS_MODE mode_v{};
		SAMPLER_ADDRESS_MODE mode_w{};

		SAMPLER_FILTER filter{};
		float max_anistoropy;

		float min_lod;
		float max_lod;
		SAMPLER_BORDER_COLOR border_color;
	};

	using RQueue = FrameworkHandle<struct RQueueTag>;

	struct ExecuteCommandsInfo
	{
		const RCommandList* lists;
		uint32_t list_count;

		const RFence* wait_fences;
		const uint64_t* wait_values;
		uint32_t wait_count;

		const RFence* signal_fences;
		const uint64_t* signal_values;
		uint32_t signal_count;
	};

	struct PushConstantRange
	{
		SHADER_STAGE stages;
		uint32_t size; // zero for no push constant
	};

	struct ShaderObjectCreateInfo
	{
		//maybe flags.
		SHADER_STAGE stage;
		SHADER_STAGE_FLAGS next_stages;
		size_t shader_code_size;
		const void* shader_code;
		const char* shader_entry;
		uint32_t descriptor_layout_count;
		FixedArray<RDescriptorLayout, SPACE_AMOUNT> descriptor_layouts;
		PushConstantRange push_constant_range;
	};
}
