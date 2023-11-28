#include "VulkanRenderer.hpp"

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif //_WIN32

BB_WARNINGS_OFF
#include <Vulkan/vulkan.h>
#define VMA_STATIC_VULKAN_FUNCTIONS 1
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#define VMA_VULKAN_VERSION 1003000 // Vulkan 1.3
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
BB_WARNINGS_ON

#include "Storage/Slotmap.h"
#include "Storage/Hashmap.h"

using namespace BB;

#ifdef _DEBUG
#define VKASSERT(vk_result, a_msg)\
	if (vk_result != VK_SUCCESS)\
		BB_ASSERT(false, a_msg)
#else
#define VKASSERT(vk_result, a_msg) vk_result
#endif //_DEBUG

#if defined(__GNUC__) || defined(__MINGW32__) || defined(__clang__) || defined(__clang_major__)
//for vulkan I initialize the struct name only. So ignore this warning
BB_PRAGMA(clang diagnostic push)
BB_PRAGMA(clang diagnostic ignored "-Wmissing-field-initializers")
BB_PRAGMA(clang diagnostic ignored "-Wcast-function-type-strict")
#endif 


//vulkan's function pointers all have the prefix PFN_<functionname>, so this works perfectly.
#define VkGetFuncPtr(inst, func) reinterpret_cast<BB_CONCAT(PFN_, func)>(vkGetInstanceProcAddr(inst, #func))

//for performance reasons this can be turned off. I need to profile this.
#define ENUM_CONVERSATION_BY_ARRAY

struct VulkanQueuesIndices
{
	uint32_t present; //Is currently always same as graphics.
	uint32_t graphics;
	uint32_t graphics_count;
	uint32_t compute;
	uint32_t compute_count;
	uint32_t transfer;
	uint32_t transfer_count;
};

struct VulkanQueueDeviceInfo
{
	uint32_t index;
	uint32_t queueCount;
};

struct VulkanBuffer
{
	VkBuffer buffer;
	VmaAllocation allocation;
};

struct VulkanImage
{
	VkImage image;
	VmaAllocation allocation;
};

struct VulkanDepth
{
	VkImage image;
	VmaAllocation allocation;
	VkImageView view;
};

static inline VkDeviceSize GetBufferDeviceAddress(const VkDevice a_device, const VkBuffer a_buffer)
{
	VkBufferDeviceAddressInfo buffer_address_info{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, a_buffer };
	return vkGetBufferDeviceAddress(a_device, &buffer_address_info);
}

class VulkanDescriptorLinearBuffer
{
public:
	VulkanDescriptorLinearBuffer(const size_t a_buffer_size, const VkBufferUsageFlags a_buffer_usage);
	~VulkanDescriptorLinearBuffer();

	DescriptorAllocation AllocateDescriptor(const RDescriptorLayout a_descriptor);

	VkDeviceAddress GPUStartAddress() const { return m_start_address; }

private:
	//using uint32_t since descriptor buffers on some drivers only spend 32-bits virtual address.
	uint32_t m_size;
	uint32_t m_offset;
	VkBuffer m_buffer;
	VmaAllocation m_allocation;
	VkDeviceAddress m_start_address = 0;
	void* m_start;
};

constexpr int VULKAN_VERSION = 3;

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT a_message_severity,
	VkDebugUtilsMessageTypeFlagsEXT a_message_type,
	const VkDebugUtilsMessengerCallbackDataEXT* a_pcallback_data,
	void* a_puser_data)
{
	(void)a_puser_data;
	(void)a_message_type;
	switch (a_message_severity)
	{
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
		BB_WARNING(false, a_pcallback_data->pMessage, WarningType::INFO);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		BB_WARNING(false, a_pcallback_data->pMessage, WarningType::INFO);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		BB_WARNING(false, a_pcallback_data->pMessage, WarningType::MEDIUM);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		BB_WARNING(false, a_pcallback_data->pMessage, WarningType::HIGH);
		break;
	default:
		BB_ASSERT(false, "something weird happened");
		break;
	}
	return VK_FALSE;
}

static VkDebugUtilsMessengerCreateInfoEXT CreateDebugCallbackCreateInfo()
{
	VkDebugUtilsMessengerCreateInfoEXT create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	create_info.messageSeverity =
		//VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	create_info.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	create_info.pfnUserCallback = debugCallback;
	create_info.pUserData = nullptr;

	return create_info;
}

static bool CheckExtensionSupport(Allocator a_temp_allocator, Slice<const char*> a_extensions)
{
	// check extensions if they are available.
	uint32_t extension_count;
	vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
	VkExtensionProperties* extensions = BBnewArr(a_temp_allocator, extension_count, VkExtensionProperties);
	vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions);

	for (auto it = a_extensions.begin(); it < a_extensions.end(); it++)
	{
		for (size_t i = 0; i < extension_count; i++)
		{

			if (strcmp(*it, extensions[i].extensionName) == 0)
				break;

			if (it == a_extensions.end())
				return false;
		}
	}
	return true;
}

static bool CheckValidationLayerSupport(Allocator a_temp_allocator, const Slice<const char*> a_layers)
{
	// check layers if they are available.
	uint32_t layer_count;
	vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
	VkLayerProperties* layers = BBnewArr(a_temp_allocator, layer_count, VkLayerProperties);
	vkEnumerateInstanceLayerProperties(&layer_count, layers);

	for (auto it = a_layers.begin(); it < a_layers.end(); it++)
	{
		for (size_t i = 0; i < layer_count; i++)
		{

			if (strcmp(*it, layers[i].layerName) == 0)
				break;

			if (it == a_layers.end())
				return false;
		}
	}
	return true;
}

//If a_Index is nullptr it will just check if we have a queue that has a graphics bit.
static bool QueueFindGraphicsBit(Allocator a_temp_allocator, VkPhysicalDevice a_physical_device)
{
	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(a_physical_device, &queue_family_count, nullptr);
	VkQueueFamilyProperties* queue_families = BBnewArr(a_temp_allocator, queue_family_count, VkQueueFamilyProperties);
	vkGetPhysicalDeviceQueueFamilyProperties(a_physical_device, &queue_family_count, queue_families);

	for (uint32_t i = 0; i < queue_family_count; i++)
	{
		if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			return true;
		}
	}
	return false;
}

static VkPhysicalDevice FindPhysicalDevice(Allocator a_temp_allocator, const VkInstance a_instance)
{
	uint32_t device_count = 0;
	vkEnumeratePhysicalDevices(a_instance, &device_count, nullptr);
	BB_ASSERT(device_count != 0, "Failed to find any GPU's with vulkan support.");
	VkPhysicalDevice* physical_device = BBnewArr(a_temp_allocator, device_count, VkPhysicalDevice);
	vkEnumeratePhysicalDevices(a_instance, &device_count, physical_device);

	for (uint32_t i = 0; i < device_count; i++)
	{

		VkPhysicalDeviceProperties2 device_properties{};
		device_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		device_properties.pNext = nullptr;
		vkGetPhysicalDeviceProperties2(physical_device[i], &device_properties);

		VkPhysicalDeviceDescriptorIndexingFeatures indexing_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT };
		VkPhysicalDeviceTimelineSemaphoreFeatures sem_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES };
		sem_features.pNext = &indexing_features;
		VkPhysicalDeviceSynchronization2Features sync_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES };
		sync_features.pNext = &sem_features;
		VkPhysicalDeviceFeatures2 device_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
		device_features.pNext = &sync_features;
		vkGetPhysicalDeviceFeatures2(physical_device[i], &device_features);

		if (device_properties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
			sem_features.timelineSemaphore == VK_TRUE &&
			sync_features.synchronization2 == VK_TRUE &&
			device_features.features.geometryShader &&
			device_features.features.samplerAnisotropy &&
			QueueFindGraphicsBit(a_temp_allocator, physical_device[i]) &&
			indexing_features.descriptorBindingPartiallyBound == VK_TRUE &&
			indexing_features.runtimeDescriptorArray == VK_TRUE &&
			indexing_features.descriptorBindingSampledImageUpdateAfterBind == VK_TRUE &&
			indexing_features.descriptorBindingVariableDescriptorCount == VK_TRUE)
		{
			return physical_device[i];
		}
	}

	BB_ASSERT(false, "Failed to find a suitable GPU that is discrete and has a geometry shader.");
	return VK_NULL_HANDLE;
}

static VulkanQueueDeviceInfo FindQueueIndex(VkQueueFamilyProperties* a_queue_properties, uint32_t a_family_property_count, VkQueueFlags a_queue_flags)
{
	VulkanQueueDeviceInfo return_info{};

	//Find specialized compute queue.
	if ((a_queue_flags & VK_QUEUE_COMPUTE_BIT) == a_queue_flags)
	{
		for (uint32_t i = 0; i < a_family_property_count; i++)
		{
			if ((a_queue_properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
				((a_queue_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) &&
				((a_queue_properties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) == 0))
			{
				return_info.index = i;
				return_info.queueCount = a_queue_properties[i].queueCount;
				return return_info;
			}
		}
	}

	//Find specialized transfer queue.
	if ((a_queue_flags & VK_QUEUE_TRANSFER_BIT) == a_queue_flags)
	{
		for (uint32_t i = 0; i < a_family_property_count; i++)
		{
			if ((a_queue_properties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
				((a_queue_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) &&
				((a_queue_properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
			{
				return_info.index = i;
				return_info.queueCount = a_queue_properties[i].queueCount;
				return return_info;
			}
		}
	}

	//If we didn't find a specialized transfer/compute queue or want a differen queue then get the first we find.
	for (uint32_t i = 0; i < a_family_property_count; i++)
	{
		if ((a_queue_properties[i].queueFlags & a_queue_flags) == a_queue_flags)
		{
			return_info.index = i;
			return_info.queueCount = a_queue_properties[i].queueCount;
			return return_info;
		}
	}

	BB_ASSERT(false, "Vulkan: Failed to find required queue.");
	return return_info;
}

static VulkanQueuesIndices GetQueueIndices(Allocator a_temp_allocator, const VkPhysicalDevice a_phys_device)
{
	VulkanQueuesIndices return_value{};

	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(a_phys_device, &queue_family_count, nullptr);
	VkQueueFamilyProperties* const queue_families = BBnewArr(a_temp_allocator, queue_family_count, VkQueueFamilyProperties);
	vkGetPhysicalDeviceQueueFamilyProperties(a_phys_device, &queue_family_count, queue_families);

	{
		VulkanQueueDeviceInfo graphic_queue = FindQueueIndex(queue_families,
			queue_family_count,
			VK_QUEUE_GRAPHICS_BIT);

		return_value.graphics = graphic_queue.index;
		return_value.graphics_count = graphic_queue.queueCount;
		return_value.present = graphic_queue.index;
	}

	{
		VulkanQueueDeviceInfo transfer_queue = FindQueueIndex(queue_families,
			queue_family_count,
			VK_QUEUE_TRANSFER_BIT);
		//Check if the queueindex is the same as graphics.
		if (transfer_queue.index != return_value.graphics)
		{
			return_value.transfer = transfer_queue.index;
			return_value.transfer_count = transfer_queue.queueCount;
		}
		else
		{
			return_value.transfer = return_value.graphics;
			return_value.transfer_count = return_value.graphics_count;
		}
	}

	{
		VulkanQueueDeviceInfo compute_queue = FindQueueIndex(queue_families,
			queue_family_count,
			VK_QUEUE_COMPUTE_BIT);
		//Check if the queueindex is the same as graphics.
		if ((compute_queue.index != return_value.graphics) &&
			(compute_queue.index != return_value.compute))
		{
			return_value.compute = compute_queue.index;
			return_value.compute = compute_queue.queueCount;
		}
		else
		{
			return_value.compute = return_value.graphics;
			return_value.compute_count = return_value.graphics_count;
		}
	}

	return return_value;
}

static VkDevice CreateLogicalDevice(Allocator a_temp_allocator, const VkPhysicalDevice a_phys_device, const VulkanQueuesIndices& a_queue_indices, const BB::Slice<const char*>& a_device_extensions)
{
	VkPhysicalDeviceFeatures device_features{};
	device_features.samplerAnisotropy = VK_TRUE;
	VkPhysicalDeviceTimelineSemaphoreFeatures timeline_sem_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES };
	timeline_sem_features.timelineSemaphore = VK_TRUE;
	timeline_sem_features.pNext = nullptr;

	VkPhysicalDeviceShaderDrawParametersFeatures shader_draw_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES };
	shader_draw_features.shaderDrawParameters = VK_TRUE;
	shader_draw_features.pNext = &timeline_sem_features;

	VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES };
	dynamic_rendering.dynamicRendering = VK_TRUE;
	dynamic_rendering.pNext = &shader_draw_features;

	VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES };
	indexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
	indexingFeatures.runtimeDescriptorArray = VK_TRUE;
	indexingFeatures.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
	indexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
	indexingFeatures.pNext = &dynamic_rendering;

	VkPhysicalDeviceBufferDeviceAddressFeatures address_feature{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };
	address_feature.bufferDeviceAddress = VK_TRUE;
	address_feature.pNext = &indexingFeatures;

	VkPhysicalDeviceDescriptorBufferFeaturesEXT  descriptor_buffer_info{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT };
	descriptor_buffer_info.descriptorBuffer = VK_TRUE;
	descriptor_buffer_info.pNext = &address_feature;

	VkPhysicalDeviceSynchronization2Features sync_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES };
	sync_features.synchronization2 = VK_TRUE;
	sync_features.pNext = &descriptor_buffer_info;

	VkPhysicalDeviceShaderObjectFeaturesEXT shader_objects{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT };
	shader_objects.shaderObject = true;
	shader_objects.pNext = &sync_features;

	VkDeviceQueueCreateInfo* queue_create_infos = BBnewArr(a_temp_allocator, 3, VkDeviceQueueCreateInfo);
	float standard_queue_prios[16] = { 1.0f }; // just put it all to 1 for multiple queues;
	uint32_t unique_queue_pos = 0;
	{
		queue_create_infos[unique_queue_pos] = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
		queue_create_infos[unique_queue_pos].queueFamilyIndex = a_queue_indices.graphics;
		queue_create_infos[unique_queue_pos].queueCount = a_queue_indices.graphics_count;
		queue_create_infos[unique_queue_pos].pQueuePriorities = standard_queue_prios;

		++unique_queue_pos;

		if (a_queue_indices.graphics != a_queue_indices.compute)
		{
			queue_create_infos[unique_queue_pos] = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
			queue_create_infos[unique_queue_pos].queueFamilyIndex = a_queue_indices.compute;
			queue_create_infos[unique_queue_pos].queueCount = a_queue_indices.compute_count;
			queue_create_infos[unique_queue_pos].pQueuePriorities = standard_queue_prios;

			++unique_queue_pos;
		}

		if (a_queue_indices.graphics != a_queue_indices.transfer || a_queue_indices.compute != a_queue_indices.transfer)
		{
			queue_create_infos[unique_queue_pos] = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
			queue_create_infos[unique_queue_pos].queueFamilyIndex = a_queue_indices.transfer;
			queue_create_infos[unique_queue_pos].queueCount = a_queue_indices.transfer_count;
			queue_create_infos[unique_queue_pos].pQueuePriorities = standard_queue_prios;

			++unique_queue_pos;
		}
	}

	VkDeviceCreateInfo device_create_info{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	device_create_info.pQueueCreateInfos = queue_create_infos;
	device_create_info.queueCreateInfoCount = unique_queue_pos;
	device_create_info.pEnabledFeatures = &device_features;
	device_create_info.ppEnabledExtensionNames = a_device_extensions.data();
	device_create_info.enabledExtensionCount = static_cast<uint32_t>(a_device_extensions.size());
	device_create_info.pNext = &shader_objects;

	VkDevice return_device;
	VKASSERT(vkCreateDevice(a_phys_device,
		&device_create_info,
		nullptr,
		&return_device),
		"Failed to create logical device Vulkan.");

	return return_device;
}

struct Vulkan_inst
{
	Vulkan_inst()
	{
#ifdef ENUM_CONVERSATION_BY_ARRAY
		enum_conv.descriptor_types[static_cast<uint32_t>(DESCRIPTOR_TYPE::READONLY_CONSTANT)] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		enum_conv.descriptor_types[static_cast<uint32_t>(DESCRIPTOR_TYPE::READONLY_BUFFER)] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		enum_conv.descriptor_types[static_cast<uint32_t>(DESCRIPTOR_TYPE::READWRITE)] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		enum_conv.descriptor_types[static_cast<uint32_t>(DESCRIPTOR_TYPE::IMAGE)] = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		enum_conv.descriptor_types[static_cast<uint32_t>(DESCRIPTOR_TYPE::SAMPLER)] = VK_DESCRIPTOR_TYPE_SAMPLER;

		enum_conv.image_layouts[static_cast<uint32_t>(IMAGE_LAYOUT::UNDEFINED)] = VK_IMAGE_LAYOUT_UNDEFINED;
		enum_conv.image_layouts[static_cast<uint32_t>(IMAGE_LAYOUT::GENERAL)] = VK_IMAGE_LAYOUT_GENERAL;
		enum_conv.image_layouts[static_cast<uint32_t>(IMAGE_LAYOUT::TRANSFER_SRC)] = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		enum_conv.image_layouts[static_cast<uint32_t>(IMAGE_LAYOUT::TRANSFER_DST)] = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		enum_conv.image_layouts[static_cast<uint32_t>(IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL)] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		enum_conv.image_layouts[static_cast<uint32_t>(IMAGE_LAYOUT::DEPTH_STENCIL_ATTACHMENT)] = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		enum_conv.image_layouts[static_cast<uint32_t>(IMAGE_LAYOUT::SHADER_READ_ONLY)] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		enum_conv.image_layouts[static_cast<uint32_t>(IMAGE_LAYOUT::PRESENT)] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		enum_conv.depth_formats[static_cast<uint32_t>(DEPTH_FORMAT::D32_SFLOAT)] = VK_FORMAT_D32_SFLOAT;
		enum_conv.depth_formats[static_cast<uint32_t>(DEPTH_FORMAT::D32_SFLOAT_S8_UINT)] = VK_FORMAT_D32_SFLOAT_S8_UINT;
		enum_conv.depth_formats[static_cast<uint32_t>(DEPTH_FORMAT::D24_UNORM_S8_UINT)] = VK_FORMAT_D24_UNORM_S8_UINT;

		enum_conv.image_formats[static_cast<uint32_t>(IMAGE_FORMAT::RGBA8_SRGB)] = VK_FORMAT_R8G8B8A8_SRGB;
		enum_conv.image_formats[static_cast<uint32_t>(IMAGE_FORMAT::RGBA8_UNORM)] = VK_FORMAT_R8G8B8A8_SNORM;

		enum_conv.image_types[static_cast<uint32_t>(IMAGE_TYPE::TYPE_1D)] = VK_IMAGE_TYPE_1D;
		enum_conv.image_types[static_cast<uint32_t>(IMAGE_TYPE::TYPE_2D)] = VK_IMAGE_TYPE_2D;
		enum_conv.image_types[static_cast<uint32_t>(IMAGE_TYPE::TYPE_3D)] = VK_IMAGE_TYPE_3D;

		enum_conv.image_view_types[static_cast<uint32_t>(IMAGE_TYPE::TYPE_1D)] = VK_IMAGE_VIEW_TYPE_1D;
		enum_conv.image_view_types[static_cast<uint32_t>(IMAGE_TYPE::TYPE_2D)] = VK_IMAGE_VIEW_TYPE_2D;
		enum_conv.image_view_types[static_cast<uint32_t>(IMAGE_TYPE::TYPE_3D)] = VK_IMAGE_VIEW_TYPE_3D;

		enum_conv.image_tilings[static_cast<uint32_t>(IMAGE_TILING::LINEAR)] = VK_IMAGE_TILING_LINEAR;
		enum_conv.image_tilings[static_cast<uint32_t>(IMAGE_TILING::OPTIMAL)] = VK_IMAGE_TILING_OPTIMAL;

		enum_conv.sampler_address_modes[static_cast<uint32_t>(SAMPLER_ADDRESS_MODE::REPEAT)] = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		enum_conv.sampler_address_modes[static_cast<uint32_t>(SAMPLER_ADDRESS_MODE::MIRROR)] = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		enum_conv.sampler_address_modes[static_cast<uint32_t>(SAMPLER_ADDRESS_MODE::BORDER)] = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		enum_conv.sampler_address_modes[static_cast<uint32_t>(SAMPLER_ADDRESS_MODE::CLAMP)] = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;


		enum_conv.pipeline_stage_flags[static_cast<uint32_t>(BARRIER_PIPELINE_STAGE::TOP_OF_PIPELINE)] = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
		enum_conv.pipeline_stage_flags[static_cast<uint32_t>(BARRIER_PIPELINE_STAGE::TRANSFER)] = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		enum_conv.pipeline_stage_flags[static_cast<uint32_t>(BARRIER_PIPELINE_STAGE::VERTEX_INPUT)] = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
		enum_conv.pipeline_stage_flags[static_cast<uint32_t>(BARRIER_PIPELINE_STAGE::VERTEX_SHADER)] = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
		enum_conv.pipeline_stage_flags[static_cast<uint32_t>(BARRIER_PIPELINE_STAGE::EARLY_FRAG_TEST)] = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
		enum_conv.pipeline_stage_flags[static_cast<uint32_t>(BARRIER_PIPELINE_STAGE::FRAGMENT_SHADER)] = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		enum_conv.pipeline_stage_flags[static_cast<uint32_t>(BARRIER_PIPELINE_STAGE::END_OF_PIPELINE)] = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;

		enum_conv.access_flags[static_cast<uint32_t>(BARRIER_ACCESS_MASK::NONE)] = VK_ACCESS_2_NONE;
		enum_conv.access_flags[static_cast<uint32_t>(BARRIER_ACCESS_MASK::TRANSFER_WRITE)] = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		enum_conv.access_flags[static_cast<uint32_t>(BARRIER_ACCESS_MASK::DEPTH_STENCIL_READ_WRITE)] = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;;
		enum_conv.access_flags[static_cast<uint32_t>(BARRIER_ACCESS_MASK::SHADER_READ)] = VK_ACCESS_2_SHADER_READ_BIT;
#endif //ENUM_CONVERSATION_BY_ARRAY
	}

	VkInstance instance;
	VkPhysicalDevice phys_device;
	VkDevice device;
	VkQueue present_queue;
	VmaAllocator vma;

	//jank pointer
	VulkanDescriptorLinearBuffer* pdescriptor_buffer;

	StaticOL_HashMap<uint64_t, VkPipelineLayout> pipeline_layout_cache;
	StaticSlotmap<VulkanBuffer, GPUBuffer> buffers;
	StaticSlotmap<VulkanImage, RImage> images;
	StaticSlotmap<VulkanDepth, RDepthBuffer> depth_images;
	
	VulkanQueuesIndices queue_indices;
	struct DeviceInfo
	{
		float max_anisotropy;
	} device_info;

	struct DescriptorSizes
	{
		uint32_t uniform_buffer;
		uint32_t storage_buffer;
		uint32_t sampled_image;
		uint32_t sampler;
	} descriptor_sizes;

#ifdef ENUM_CONVERSATION_BY_ARRAY
	//maybe faster then a switch... profile!
	struct EnumConversions
	{
		VkDescriptorType descriptor_types[static_cast<uint32_t>(DESCRIPTOR_TYPE::ENUM_SIZE)];
		VkImageLayout image_layouts[static_cast<uint32_t>(IMAGE_LAYOUT::ENUM_SIZE)];
		VkFormat depth_formats[static_cast<uint32_t>(DEPTH_FORMAT::ENUM_SIZE)];
		VkFormat image_formats[static_cast<uint32_t>(IMAGE_FORMAT::ENUM_SIZE)];
		VkImageType image_types[static_cast<uint32_t>(IMAGE_TYPE::ENUM_SIZE)];
		VkImageViewType image_view_types[static_cast<uint32_t>(IMAGE_TYPE::ENUM_SIZE)];
		VkImageTiling image_tilings[static_cast<uint32_t>(IMAGE_TILING::ENUM_SIZE)];
		VkSamplerAddressMode sampler_address_modes[static_cast<uint32_t>(SAMPLER_ADDRESS_MODE::ENUM_SIZE)];
		VkPipelineStageFlags2 pipeline_stage_flags[static_cast<uint32_t>(BARRIER_PIPELINE_STAGE::ENUM_SIZE)];
		VkAccessFlags2 access_flags[static_cast<uint32_t>(BARRIER_ACCESS_MASK::ENUM_SIZE)];
	} enum_conv;
#endif //ENUM_CONVERSATION_BY_ARRAY

	struct Pfn
	{
		//naming.... these function pointers do not count.
		PFN_vkGetDescriptorSetLayoutSizeEXT GetDescriptorSetLayoutSizeEXT;
		PFN_vkGetDescriptorSetLayoutBindingOffsetEXT GetDescriptorSetLayoutBindingOffsetEXT;
		PFN_vkGetDescriptorEXT GetDescriptorEXT;
		PFN_vkCmdBindDescriptorBuffersEXT CmdBindDescriptorBuffersEXT;
		PFN_vkCmdBindDescriptorBufferEmbeddedSamplersEXT CmdBindDescriptorBufferEmbeddedSamplersEXT;
		PFN_vkCmdSetDescriptorBufferOffsetsEXT CmdSetDescriptorBufferOffsetsEXT;
		PFN_vkSetDebugUtilsObjectNameEXT SetDebugUtilsObjectNameEXT;
		PFN_vkCreateShadersEXT CreateShaderEXT;
		PFN_vkDestroyShaderEXT DestroyShaderEXT;
		PFN_vkCmdBindShadersEXT CmdBindShadersEXT;
		PFN_vkCmdSetPolygonModeEXT CmdSetPolygonModeEXT;
		PFN_vkCmdSetVertexInputEXT CmdSetVertexInputEXT;
		PFN_vkCmdSetRasterizationSamplesEXT CmdSetRasterizationSamplesEXT;
		PFN_vkCmdSetColorWriteMaskEXT CmdSetColorWriteMaskEXT;
		PFN_vkCmdSetColorBlendEnableEXT CmdSetColorBlendEnableEXT;
		PFN_vkCmdSetColorBlendEquationEXT CmdSetColorBlendEquationEXT;
		PFN_vkCmdSetAlphaToCoverageEnableEXT CmdSetAlphaToCoverageEnableEXT;
		PFN_vkCmdSetSampleMaskEXT CmdSetSampleMaskEXT;
	} pfn;
};

struct Vulkan_swapchain
{
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;
	VkFormat image_format;
	struct swapchain_frame
	{
		VkImage image;
		VkImageView image_view;
		VkSemaphore image_available_semaphore;
		VkSemaphore present_finished_semaphore;
	};
	uint32_t frame_count;
	swapchain_frame* frames;
};

static Vulkan_inst* s_vulkan_inst = nullptr;
static Vulkan_swapchain* s_vulkan_swapchain = nullptr;

#ifdef _DEBUG
static inline void SetDebugName_f(const char* a_name, const uint64_t a_object_handle, const VkObjectType a_obj_type)
{
	VkDebugUtilsObjectNameInfoEXT debug_name_info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
	debug_name_info.pNext = nullptr;
	debug_name_info.objectType = a_obj_type;
	debug_name_info.objectHandle = a_object_handle;
	debug_name_info.pObjectName = a_name;
	s_vulkan_inst->pfn.SetDebugUtilsObjectNameEXT(s_vulkan_inst->device, &debug_name_info);
}

#define SetDebugName(a_name, a_object_handle, a_obj_type) SetDebugName_f(a_name, reinterpret_cast<uintptr_t>(a_object_handle), a_obj_type)
#else
#define SetDebugName
#endif //_DEBUG

static inline VkDescriptorType DescriptorBufferType(const DESCRIPTOR_TYPE a_type)
{
#ifdef ENUM_CONVERSATION_BY_ARRAY
	return s_vulkan_inst->enum_conv.descriptor_types[static_cast<uint32_t>(a_type)];
#else
	switch (a_type)
	{
	case DESCRIPTOR_TYPE::READONLY_CONSTANT:	return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	case DESCRIPTOR_TYPE::READONLY_BUFFER:		return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	case DESCRIPTOR_TYPE::READWRITE:			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	case DESCRIPTOR_TYPE::IMAGE:				return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	case DESCRIPTOR_TYPE::SAMPLER:				return VK_DESCRIPTOR_TYPE_SAMPLER;
	default:
		BB_ASSERT(false, "Vulkan: DESCRIPTOR_TYPE failed to convert to a VkDescriptorType.");
		return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		break;
	}
#endif //ENUM_CONVERSATION_BY_ARRAY
}

static inline VkShaderStageFlags ShaderStageFlags(const SHADER_STAGE a_stage)
{
	switch (a_stage)
	{
	case SHADER_STAGE::ALL:					return VK_SHADER_STAGE_ALL;
	case SHADER_STAGE::VERTEX:				return VK_SHADER_STAGE_VERTEX_BIT;
	case SHADER_STAGE::FRAGMENT_PIXEL:		return VK_SHADER_STAGE_FRAGMENT_BIT;
	default:
		BB_ASSERT(false, "Vulkan: SHADER_STAGE failed to convert to a VkShaderStageFlagBits.");
		return VK_SHADER_STAGE_ALL;
		break;
	}
}

static inline VkShaderStageFlags ShaderStageFlagsFromFlags(const SHADER_STAGE_FLAGS a_stages)
{
	if (static_cast<SHADER_STAGE_FLAGS>(SHADER_STAGE::ALL) == a_stages)
	{
		return VK_SHADER_STAGE_ALL;
	}
	if ((static_cast<SHADER_STAGE_FLAGS>(SHADER_STAGE::NONE) & a_stages) == a_stages)
	{
		return 0;
	}

	VkShaderStageFlags stage_flags = 0;

	if ((static_cast<SHADER_STAGE_FLAGS>(SHADER_STAGE::VERTEX) & a_stages) == a_stages)
		return stage_flags |= VK_SHADER_STAGE_VERTEX_BIT;
	if ((static_cast<SHADER_STAGE_FLAGS>(SHADER_STAGE::FRAGMENT_PIXEL) & a_stages) == a_stages)
		return stage_flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
	return stage_flags;
}

static inline VkImageLayout ImageLayout(const IMAGE_LAYOUT a_image_layout)
{
#ifdef ENUM_CONVERSATION_BY_ARRAY
	return s_vulkan_inst->enum_conv.image_layouts[static_cast<uint32_t>(a_image_layout)];
#else
	switch (a_image_layout)
	{
	case IMAGE_LAYOUT::UNDEFINED:				return VK_IMAGE_LAYOUT_UNDEFINED;
	case IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL:return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	case IMAGE_LAYOUT::DEPTH_STENCIL_ATTACHMENT:return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	case IMAGE_LAYOUT::GENERAL:					return VK_IMAGE_LAYOUT_GENERAL;
	case IMAGE_LAYOUT::TRANSFER_SRC:			return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	case IMAGE_LAYOUT::TRANSFER_DST:			return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	case IMAGE_LAYOUT::SHADER_READ_ONLY:		return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	case IMAGE_LAYOUT::PRESENT:					return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	default:
		BB_ASSERT(false, "Vulkan: IMAGE_LAYOUT failed to convert to a VkImageLayout.");
		return VK_IMAGE_LAYOUT_UNDEFINED;
		break;
	}
#endif //ENUM_CONVERSATION_BY_ARRAY
}

static inline VkFormat DepthFormat(const DEPTH_FORMAT a_depth_format)
{
#ifdef ENUM_CONVERSATION_BY_ARRAY
	return s_vulkan_inst->enum_conv.depth_formats[static_cast<uint32_t>(a_depth_format)];
#else
	switch (a_depth_format)
	{
	case DEPTH_FORMAT::D32_SFLOAT:				return VK_FORMAT_D32_SFLOAT;
	case DEPTH_FORMAT::D32_SFLOAT_S8_UINT:		return VK_FORMAT_D32_SFLOAT_S8_UINT;
	case DEPTH_FORMAT::D24_UNORM_S8_UINT:		return VK_FORMAT_D24_UNORM_S8_UINT;
	default:
		BB_ASSERT(false, "Vulkan: DEPTH_FORMAT failed to convert to a VkFormat.");
		return VK_FORMAT_D32_SFLOAT;
		break;
	}
#endif //ENUM_CONVERSATION_BY_ARRAY
}

static inline VkFormat ImageFormats(const IMAGE_FORMAT a_image_format)
{
#ifdef ENUM_CONVERSATION_BY_ARRAY
	return s_vulkan_inst->enum_conv.image_formats[static_cast<uint32_t>(a_image_format)];
#else
	switch (a_image_format)
	{
	case IMAGE_FORMAT::RGBA8_SRGB:		return VK_FORMAT_R8G8B8A8_SRGB;
	case IMAGE_FORMAT::RGBA8_UNORM:		return VK_FORMAT_R8G8B8A8_SNORM;
	default:
		BB_ASSERT(false, "Vulkan: IMAGE_FORMAT failed to convert to a VkFormat.");
		return VK_FORMAT_R8G8B8A8_SRGB;
		break;
	}
#endif //ENUM_CONVERSATION_BY_ARRAY
}

static inline VkImageType ImageTypes(const IMAGE_TYPE a_image_types)
{
#ifdef ENUM_CONVERSATION_BY_ARRAY
	return s_vulkan_inst->enum_conv.image_types[static_cast<uint32_t>(a_image_types)];
#else
	switch (a_image_types)
	{
	case IMAGE_TYPE::TYPE_1D:		return VK_IMAGE_TYPE_1D;
	case IMAGE_TYPE::TYPE_2D:		return VK_IMAGE_TYPE_2D;
	case IMAGE_TYPE::TYPE_3D:		return VK_IMAGE_TYPE_3D;
	default:
		BB_ASSERT(false, "Vulkan: IMAGE_TYPE failed to convert to a VkImageType.");
		return VK_IMAGE_TYPE_1D;
		break;
	}
#endif //ENUM_CONVERSATION_BY_ARRAY
}

static inline VkImageViewType ImageViewTypes(const IMAGE_TYPE a_image_types)
{
#ifdef ENUM_CONVERSATION_BY_ARRAY
	return s_vulkan_inst->enum_conv.image_view_types[static_cast<uint32_t>(a_image_types)];
#else
	switch (a_image_types)
	{
	case IMAGE_TYPE::TYPE_1D:		return VK_IMAGE_VIEW_TYPE_1D;
	case IMAGE_TYPE::TYPE_2D:		return VK_IMAGE_VIEW_TYPE_2D;
	case IMAGE_TYPE::TYPE_3D:		return VK_IMAGE_VIEW_TYPE_3D;
	default:
		BB_ASSERT(false, "Vulkan: IMAGE_TYPE failed to convert to a VkImageViewType.");
		return VK_IMAGE_TYPE_1D;
		break;
	}
#endif //ENUM_CONVERSATION_BY_ARRAY
}

static inline VkImageTiling ImageTilings(const IMAGE_TILING a_image_tiling)
{
#ifdef ENUM_CONVERSATION_BY_ARRAY
	return s_vulkan_inst->enum_conv.image_tilings[static_cast<uint32_t>(a_image_tiling)];
#else
	switch (a_image_tiling)
	{
	case IMAGE_TILING::LINEAR:		return VK_IMAGE_TILING_LINEAR;
	case IMAGE_TILING::OPTIMAL:		return VK_IMAGE_TILING_OPTIMAL;
	default:
		BB_ASSERT(false, "Vulkan: IMAGE_TILING failed to convert to a VkImageTiling.");
		return VK_IMAGE_TYPE_1D;
		break;
	}
#endif //ENUM_CONVERSATION_BY_ARRAY
}

static inline VkSamplerAddressMode SamplerAddressModes(const SAMPLER_ADDRESS_MODE a_address_mode)
{
#ifdef ENUM_CONVERSATION_BY_ARRAY
	return s_vulkan_inst->enum_conv.sampler_address_modes[static_cast<uint32_t>(a_address_mode)];
#else
	switch (a_address_mode)
	{
	case SAMPLER_ADDRESS_MODE::REPEAT:		return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	case SAMPLER_ADDRESS_MODE::MIRROR:		return VK_SAMPLER_ADDRESS_MODE_MIRROR;
	case SAMPLER_ADDRESS_MODE::BORDER:		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	case SAMPLER_ADDRESS_MODE::CLAMP:		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	default:
		BB_ASSERT(false, "Vulkan: SAMPLER_ADDRESS_MODE failed to convert to a VkSamplerAddressMode.");
		return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		break;
	}
#endif //ENUM_CONVERSATION_BY_ARRAY
}

static inline VkPipelineStageFlags2 PipelineStage(const BARRIER_PIPELINE_STAGE a_stage)
{
#ifdef ENUM_CONVERSATION_BY_ARRAY
	return s_vulkan_inst->enum_conv.pipeline_stage_flags[static_cast<uint32_t>(a_stage)];
#else
	switch (a_stage)
	{
	case BARRIER_PIPELINE_STAGE::TOP_OF_PIPELINE:		return VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
	case BARRIER_PIPELINE_STAGE::TRANSFER:				return VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	case BARRIER_PIPELINE_STAGE::VERTEX_INPUT:			return VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
	case BARRIER_PIPELINE_STAGE::VERTEX_SHADER:			return VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
	case BARRIER_PIPELINE_STAGE::EARLY_FRAG_TEST:		return VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
	case BARRIER_PIPELINE_STAGE::FRAGMENT_SHADER:		return VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	case BARRIER_PIPELINE_STAGE::END_OF_PIPELINE:		return VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
	default:
		BB_ASSERT(false, "Vulkan: RENDER_PIPELINE_STAGE failed to convert to a VkPipelineStageFlags2.");
		return VK_PIPELINE_STAGE_2_NONE;
		break;
	}
#endif //ENUM_CONVERSATION_BY_ARRAY
}

static inline VkAccessFlags2 AccessMask(const BARRIER_ACCESS_MASK a_type)
{
#ifdef ENUM_CONVERSATION_BY_ARRAY
	return s_vulkan_inst->enum_conv.access_flags[static_cast<uint32_t>(a_type)];
#else
	switch (a_type)
	{
	case BARRIER_ACCESS_MASK::NONE:						return VK_ACCESS_2_NONE;
	case BARRIER_ACCESS_MASK::TRANSFER_WRITE:			return VK_ACCESS_2_TRANSFER_WRITE_BIT;
	case BARRIER_ACCESS_MASK::DEPTH_STENCIL_READ_WRITE:	return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	case BARRIER_ACCESS_MASK::SHADER_READ:				return VK_ACCESS_2_SHADER_READ_BIT;
	default:
		BB_ASSERT(false, "Vulkan: RENDER_ACCESS_MASK failed to convert to a VkAccessFlags2.");
		return VK_ACCESS_2_NONE;
		break;
	}
#endif //ENUM_CONVERSATION_BY_ARRAY
}

static VkSampler CreateSampler(const SamplerCreateInfo& a_CreateInfo)
{
	VkSamplerCreateInfo sampler_info{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	sampler_info.addressModeU = SamplerAddressModes(a_CreateInfo.mode_u);
	sampler_info.addressModeV = SamplerAddressModes(a_CreateInfo.mode_v);
	sampler_info.addressModeW = SamplerAddressModes(a_CreateInfo.mode_w);
	switch (a_CreateInfo.filter)
	{
	case SAMPLER_FILTER::NEAREST:
		sampler_info.magFilter = VK_FILTER_NEAREST;
		sampler_info.minFilter = VK_FILTER_NEAREST;
		break;
	case SAMPLER_FILTER::LINEAR:
		sampler_info.magFilter = VK_FILTER_LINEAR;
		sampler_info.minFilter = VK_FILTER_LINEAR;
		break;
	}
	sampler_info.minLod = a_CreateInfo.min_lod;
	sampler_info.maxLod = a_CreateInfo.max_lod;
	sampler_info.mipLodBias = 0;
	if (a_CreateInfo.max_anistoropy > 0)
	{
		sampler_info.anisotropyEnable = VK_TRUE;
		sampler_info.maxAnisotropy = a_CreateInfo.max_anistoropy;
	}

	VkSampler sampler;
	VKASSERT(vkCreateSampler(s_vulkan_inst->device, &sampler_info, nullptr, &sampler),
		"Vulkan: Failed to create image sampler!");
	SetDebugName(a_CreateInfo.name, sampler, VK_OBJECT_TYPE_SAMPLER);

	return sampler;
}

static inline VkDescriptorAddressInfoEXT GetDescriptorAddressInfo(const VkDevice a_device, const BufferView& a_Buffer, const VkFormat a_Format = VK_FORMAT_UNDEFINED)
{
	VkDescriptorAddressInfoEXT info{ VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT };
	info.range = a_Buffer.size;
	info.address = GetBufferDeviceAddress(a_device, s_vulkan_inst->buffers[a_Buffer.buffer].buffer);
	//offset the address.
	info.address += a_Buffer.offset;
	info.format = a_Format;
	return info;
}

VulkanDescriptorLinearBuffer::VulkanDescriptorLinearBuffer(const size_t a_buffer_size, const VkBufferUsageFlags a_buffer_usage)
	:	m_size(static_cast<uint32_t>(a_buffer_size)), m_offset(0)
{
	//create the CPU buffer
	VkBufferCreateInfo buffer_info{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	buffer_info.size = a_buffer_size;
	buffer_info.usage = a_buffer_usage;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo vma_alloc{};
	vma_alloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	vma_alloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

	VKASSERT(vmaCreateBuffer(s_vulkan_inst->vma,
		&buffer_info, &vma_alloc,
		&m_buffer, &m_allocation,
		nullptr), "Vulkan::VMA, Failed to allocate memory for a descriptor buffer");

	VKASSERT(vmaMapMemory(s_vulkan_inst->vma, m_allocation, &m_start),
		"Vulkan: Failed to map memory for descriptor buffer");

	m_start_address = GetBufferDeviceAddress(s_vulkan_inst->device, m_buffer);
}

VulkanDescriptorLinearBuffer::~VulkanDescriptorLinearBuffer()
{
	vmaUnmapMemory(s_vulkan_inst->vma, m_allocation);
	vmaDestroyBuffer(s_vulkan_inst->vma, m_buffer, m_allocation);
}

DescriptorAllocation VulkanDescriptorLinearBuffer::AllocateDescriptor(const RDescriptorLayout a_descriptor)
{
	DescriptorAllocation allocation;
	const VkDescriptorSetLayout descriptor_set = reinterpret_cast<VkDescriptorSetLayout>(a_descriptor.handle);
	VkDeviceSize descriptors_size;
	s_vulkan_inst->pfn.GetDescriptorSetLayoutSizeEXT(
		s_vulkan_inst->device,
		descriptor_set,
		&descriptors_size);

	BB_ASSERT(m_size > descriptors_size + m_offset, "Not enough space in the descriptor buffer!");
	allocation.descriptor = a_descriptor;
	allocation.size = static_cast<uint32_t>(descriptors_size);
	allocation.offset = m_offset;
	allocation.buffer_start = m_start; //Maybe just get this from the descriptor heap? We only have one heap anyway.

	m_offset += allocation.size;

	return allocation;
}

bool Vulkan::InitializeVulkan(StackAllocator_t& a_stack_allocator, const char* a_app_name, const char* a_engine_name, const bool a_debug)
{
	BB_ASSERT(s_vulkan_inst == nullptr, "trying to initialize vulkan while it's already initialized");
	s_vulkan_inst = BBnew(a_stack_allocator, Vulkan_inst) {};

	//just enable then all, no fallback layer for now lol.
	const char* instance_extensions[] = {
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
		VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME
	};

	const char* device_extensions[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
		VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
		VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
		VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
		VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
		VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME,
		VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME,
		VK_EXT_SHADER_OBJECT_EXTENSION_NAME
	};

	BBStackAllocatorScope(a_stack_allocator)
	{
		//Check if the extensions and layers work.
		BB_ASSERT(CheckExtensionSupport(a_stack_allocator,
			Slice(instance_extensions, _countof(instance_extensions))),
			"Vulkan: extension(s) not supported.");

		BB_ASSERT(CheckExtensionSupport(a_stack_allocator,
			Slice(device_extensions, _countof(device_extensions))),
			"Vulkan: extension(s) not supported.");

		VkApplicationInfo app_info{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
		app_info.pApplicationName = a_app_name;
		app_info.pEngineName = a_engine_name;
		app_info.applicationVersion = VK_MAKE_API_VERSION(0, 1, VULKAN_VERSION, 0);
		app_info.engineVersion = VK_MAKE_API_VERSION(0, 1, VULKAN_VERSION, 0);
		app_info.apiVersion = VK_MAKE_API_VERSION(0, 1, VULKAN_VERSION, 0);

		VkInstanceCreateInfo instance_create_info{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
		instance_create_info.pApplicationInfo = &app_info;
		VkDebugUtilsMessengerCreateInfoEXT debug_create_info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
		if (a_debug)
		{
			const char* validationLayer = "VK_LAYER_KHRONOS_validation";
			BB_WARNING(CheckValidationLayerSupport(a_stack_allocator, Slice(&validationLayer, 1)), "Vulkan: Validation layer(s) not available.", WarningType::MEDIUM);
			debug_create_info = CreateDebugCallbackCreateInfo();
			instance_create_info.ppEnabledLayerNames = &validationLayer;
			instance_create_info.enabledLayerCount = 1;
			instance_create_info.pNext = &debug_create_info;
		}
		else
		{
			instance_create_info.ppEnabledLayerNames = nullptr;
			instance_create_info.enabledLayerCount = 0;
		}
		instance_create_info.ppEnabledExtensionNames = instance_extensions;
		instance_create_info.enabledExtensionCount = _countof(instance_extensions);

		VKASSERT(vkCreateInstance(&instance_create_info,
			nullptr,
			&s_vulkan_inst->instance), "Failed to create Vulkan Instance!");

		s_vulkan_inst->pfn.GetDescriptorSetLayoutSizeEXT = VkGetFuncPtr(s_vulkan_inst->instance, vkGetDescriptorSetLayoutSizeEXT);
		s_vulkan_inst->pfn.GetDescriptorSetLayoutBindingOffsetEXT = VkGetFuncPtr(s_vulkan_inst->instance, vkGetDescriptorSetLayoutBindingOffsetEXT);
		s_vulkan_inst->pfn.GetDescriptorEXT = VkGetFuncPtr(s_vulkan_inst->instance, vkGetDescriptorEXT);
		s_vulkan_inst->pfn.CmdBindDescriptorBuffersEXT = VkGetFuncPtr(s_vulkan_inst->instance, vkCmdBindDescriptorBuffersEXT);
		s_vulkan_inst->pfn.CmdBindDescriptorBufferEmbeddedSamplersEXT = VkGetFuncPtr(s_vulkan_inst->instance, vkCmdBindDescriptorBufferEmbeddedSamplersEXT);
		s_vulkan_inst->pfn.CmdSetDescriptorBufferOffsetsEXT = VkGetFuncPtr(s_vulkan_inst->instance, vkCmdSetDescriptorBufferOffsetsEXT);
		s_vulkan_inst->pfn.SetDebugUtilsObjectNameEXT = VkGetFuncPtr(s_vulkan_inst->instance, vkSetDebugUtilsObjectNameEXT);
		s_vulkan_inst->pfn.CreateShaderEXT = VkGetFuncPtr(s_vulkan_inst->instance, vkCreateShadersEXT);
		s_vulkan_inst->pfn.DestroyShaderEXT = VkGetFuncPtr(s_vulkan_inst->instance, vkDestroyShaderEXT);
		s_vulkan_inst->pfn.CmdBindShadersEXT = VkGetFuncPtr(s_vulkan_inst->instance, vkCmdBindShadersEXT);
		s_vulkan_inst->pfn.CmdSetPolygonModeEXT = VkGetFuncPtr(s_vulkan_inst->instance, vkCmdSetPolygonModeEXT);
		s_vulkan_inst->pfn.CmdSetVertexInputEXT = VkGetFuncPtr(s_vulkan_inst->instance, vkCmdSetVertexInputEXT);
		s_vulkan_inst->pfn.CmdSetRasterizationSamplesEXT = VkGetFuncPtr(s_vulkan_inst->instance, vkCmdSetRasterizationSamplesEXT);
		s_vulkan_inst->pfn.CmdSetColorWriteMaskEXT = VkGetFuncPtr(s_vulkan_inst->instance, vkCmdSetColorWriteMaskEXT);
		s_vulkan_inst->pfn.CmdSetColorBlendEnableEXT = VkGetFuncPtr(s_vulkan_inst->instance, vkCmdSetColorBlendEnableEXT);
		s_vulkan_inst->pfn.CmdSetColorBlendEquationEXT = VkGetFuncPtr(s_vulkan_inst->instance, vkCmdSetColorBlendEquationEXT);
		s_vulkan_inst->pfn.CmdSetAlphaToCoverageEnableEXT = VkGetFuncPtr(s_vulkan_inst->instance, vkCmdSetAlphaToCoverageEnableEXT);
		s_vulkan_inst->pfn.CmdSetSampleMaskEXT = VkGetFuncPtr(s_vulkan_inst->instance, vkCmdSetSampleMaskEXT);

		{	//device & queues
			s_vulkan_inst->phys_device = FindPhysicalDevice(a_stack_allocator, s_vulkan_inst->instance);
			//do some queue stuff.....

			s_vulkan_inst->queue_indices = GetQueueIndices(a_stack_allocator, s_vulkan_inst->phys_device);

			s_vulkan_inst->device = CreateLogicalDevice(a_stack_allocator, 
				s_vulkan_inst->phys_device, 
				s_vulkan_inst->queue_indices,
				Slice(device_extensions, _countof(device_extensions)));
		}

		{	//descriptor info & general device properties
			VkPhysicalDeviceDescriptorBufferPropertiesEXT desc_info{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT, nullptr };
			VkPhysicalDeviceProperties2 device_properties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
			device_properties.pNext = &desc_info;
			vkGetPhysicalDeviceProperties2(s_vulkan_inst->phys_device, &device_properties);
			s_vulkan_inst->device_info.max_anisotropy = device_properties.properties.limits.maxSamplerAnisotropy;

			s_vulkan_inst->descriptor_sizes.uniform_buffer = static_cast<uint32_t>(desc_info.uniformBufferDescriptorSize);
			s_vulkan_inst->descriptor_sizes.storage_buffer = static_cast<uint32_t>(desc_info.storageBufferDescriptorSize);
			s_vulkan_inst->descriptor_sizes.sampled_image = static_cast<uint32_t>(desc_info.sampledImageDescriptorSize);
			s_vulkan_inst->descriptor_sizes.sampler = static_cast<uint32_t>(desc_info.samplerDescriptorSize);
		}

		{	//VMA stuff
			//Setup the Vulkan Memory Allocator
			VmaVulkanFunctions vk_functions{};
			vk_functions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
			vk_functions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

			VmaAllocatorCreateInfo allocator_create_info = {};
			allocator_create_info.vulkanApiVersion = VK_MAKE_API_VERSION(0, 1, VULKAN_VERSION, 0);
			allocator_create_info.physicalDevice = s_vulkan_inst->phys_device;
			allocator_create_info.device = s_vulkan_inst->device;
			allocator_create_info.instance = s_vulkan_inst->instance;
			allocator_create_info.pVulkanFunctions = &vk_functions;
			allocator_create_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

			vmaCreateAllocator(&allocator_create_info, &s_vulkan_inst->vma);
		}
	}

	s_vulkan_inst->buffers.Init(a_stack_allocator, 128);
	s_vulkan_inst->images.Init(a_stack_allocator, 256);
	s_vulkan_inst->depth_images.Init(a_stack_allocator, 32);
	s_vulkan_inst->pdescriptor_buffer = BBnew(a_stack_allocator, VulkanDescriptorLinearBuffer)(
		mbSize * 4,
		VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

	s_vulkan_inst->pipeline_layout_cache.Init(a_stack_allocator, 64);

	//Get the present queue.
	vkGetDeviceQueue(s_vulkan_inst->device,
		s_vulkan_inst->queue_indices.present,
		0,
		&s_vulkan_inst->present_queue);

	return true;
}

bool Vulkan::CreateSwapchain(StackAllocator_t& a_stack_allocator, const WindowHandle a_window_handle, const uint32_t a_width, const uint32_t a_height, uint32_t& a_backbuffer_count)
{
	BB_ASSERT(s_vulkan_inst != nullptr, "trying to create a swapchain while vulkan is not initialized");
	BB_ASSERT(s_vulkan_swapchain == nullptr, "trying to create a swapchain while one exists");
	s_vulkan_swapchain = BBnew(a_stack_allocator, Vulkan_swapchain) {};
	
	BBStackAllocatorScope(a_stack_allocator)
	{
		//Surface
		VkWin32SurfaceCreateInfoKHR surface_create_info{};
		surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surface_create_info.hwnd = reinterpret_cast<HWND>(a_window_handle.handle);
		surface_create_info.hinstance = GetModuleHandle(nullptr);
		VKASSERT(vkCreateWin32SurfaceKHR(s_vulkan_inst->instance,
			&surface_create_info,
			nullptr,
			&s_vulkan_swapchain->surface),
			"Failed to create Win32 vulkan surface.");

		VkSurfaceCapabilitiesKHR capabilities;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s_vulkan_inst->phys_device, s_vulkan_swapchain->surface, &capabilities);

		VkSurfaceFormatKHR* formats;
		uint32_t format_count;
		vkGetPhysicalDeviceSurfaceFormatsKHR(s_vulkan_inst->phys_device, s_vulkan_swapchain->surface, &format_count, nullptr);
		formats = BBnewArr(a_stack_allocator, format_count, VkSurfaceFormatKHR);
		vkGetPhysicalDeviceSurfaceFormatsKHR(s_vulkan_inst->phys_device, s_vulkan_swapchain->surface, &format_count, formats);

		VkPresentModeKHR* present_modes;
		uint32_t present_mode_count;
		vkGetPhysicalDeviceSurfacePresentModesKHR(s_vulkan_inst->phys_device, s_vulkan_swapchain->surface, &present_mode_count, nullptr);
		present_modes = BBnewArr(a_stack_allocator, present_mode_count, VkPresentModeKHR);
		vkGetPhysicalDeviceSurfacePresentModesKHR(s_vulkan_inst->phys_device, s_vulkan_swapchain->surface, &present_mode_count, present_modes);

		BB_ASSERT(format_count != 0 && present_mode_count != 0, "physical device does not support a swapchain!");

		VkPresentModeKHR optimal_present = VK_PRESENT_MODE_FIFO_KHR;
		for (uint32_t i = 0; i < present_mode_count; i++)
		{
			if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				optimal_present = VK_PRESENT_MODE_MAILBOX_KHR;
				break;
			}
		}

		VkSurfaceFormatKHR optimal_surface_format = formats[0];
		for (uint32_t i = 0; i < format_count; i++)
		{
			if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				optimal_surface_format = formats[i];
				break;
			}
		}

		VkExtent2D swapchain_extent
		{
			Clamp(a_width,
				capabilities.minImageExtent.width,
				capabilities.maxImageExtent.width),
			Clamp(a_height,
				capabilities.minImageExtent.height,
				capabilities.maxImageExtent.height)
		};

		const uint32_t graphics_family = s_vulkan_inst->queue_indices.graphics;
		const uint32_t present_family = s_vulkan_inst->queue_indices.present;
		const uint32_t queue_family_indices[] = 
		{ 
			graphics_family,
			present_family
		};

		VkSwapchainCreateInfoKHR swapchain_create_info{};
		swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchain_create_info.surface = s_vulkan_swapchain->surface;
		swapchain_create_info.imageFormat = optimal_surface_format.format;
		swapchain_create_info.imageColorSpace = optimal_surface_format.colorSpace;
		swapchain_create_info.imageExtent = swapchain_extent;
		swapchain_create_info.imageArrayLayers = 1;
		swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		swapchain_create_info.preTransform = capabilities.currentTransform;
		swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapchain_create_info.presentMode = optimal_present;
		swapchain_create_info.clipped = VK_TRUE;
		swapchain_create_info.oldSwapchain = nullptr;

		if (graphics_family != present_family)
		{
			swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			swapchain_create_info.queueFamilyIndexCount = 2;
			swapchain_create_info.pQueueFamilyIndices = queue_family_indices;
		}
		else
		{
			swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			swapchain_create_info.queueFamilyIndexCount = 0;
			swapchain_create_info.pQueueFamilyIndices = nullptr;
		}
		s_vulkan_swapchain->image_format = optimal_surface_format.format;

		//Now create the swapchain and set the framecount.
		const uint32_t backbuffer_count = Clamp(a_backbuffer_count, capabilities.minImageCount, capabilities.maxImageCount);
		swapchain_create_info.minImageCount = backbuffer_count;
		s_vulkan_swapchain->frame_count = backbuffer_count;
		a_backbuffer_count = backbuffer_count;

		VKASSERT(vkCreateSwapchainKHR(s_vulkan_inst->device, 
			&swapchain_create_info, 
			nullptr, 
			&s_vulkan_swapchain->swapchain), 
			"Vulkan: Failed to create swapchain.");
	}

	s_vulkan_swapchain->frames = BBnewArr(a_stack_allocator, s_vulkan_swapchain->frame_count, Vulkan_swapchain::swapchain_frame);

	BBStackAllocatorScope(a_stack_allocator)
	{
		VkImage* swapchain_images = BBnewArr(a_stack_allocator, s_vulkan_swapchain->frame_count, VkImage);
		vkGetSwapchainImagesKHR(s_vulkan_inst->device,
			s_vulkan_swapchain->swapchain,
			&s_vulkan_swapchain->frame_count,
			swapchain_images);

		VkImageViewCreateInfo image_view_create_info{};
		image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		image_view_create_info.format = s_vulkan_swapchain->image_format;
		image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		image_view_create_info.subresourceRange.baseMipLevel = 0;
		image_view_create_info.subresourceRange.levelCount = 1;
		image_view_create_info.subresourceRange.baseArrayLayer = 0;
		image_view_create_info.subresourceRange.layerCount = 1;

		VkSemaphoreCreateInfo sem_info{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

		for (uint32_t i = 0; i < s_vulkan_swapchain->frame_count; i++)
		{
			s_vulkan_swapchain->frames[i].image = swapchain_images[i];

			image_view_create_info.image = swapchain_images[i];
			VKASSERT(vkCreateImageView(s_vulkan_inst->device,
				&image_view_create_info,
				nullptr,
				&s_vulkan_swapchain->frames[i].image_view),
				"Vulkan: Failed to create swapchain image views.");

			vkCreateSemaphore(s_vulkan_inst->device,
				&sem_info,
				nullptr,
				&s_vulkan_swapchain->frames[i].image_available_semaphore);
			vkCreateSemaphore(s_vulkan_inst->device,
				&sem_info,
				nullptr,
				&s_vulkan_swapchain->frames[i].present_finished_semaphore);
		}
	}

	return true;
}

void Vulkan::CreateCommandPool(const QUEUE_TYPE a_queue_type, const uint32_t a_command_list_count, RCommandPool& a_pool, RCommandList* a_plists)
{
	VkCommandPoolCreateInfo pool_create_info{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	switch (a_queue_type)
	{
	case QUEUE_TYPE::GRAPHICS:
		pool_create_info.queueFamilyIndex = s_vulkan_inst->queue_indices.graphics;
		break;
	case QUEUE_TYPE::TRANSFER:
		pool_create_info.queueFamilyIndex = s_vulkan_inst->queue_indices.transfer;
		break;
	case QUEUE_TYPE::COMPUTE:
		pool_create_info.queueFamilyIndex = s_vulkan_inst->queue_indices.compute;
		break;
	default:
		BB_ASSERT(false, "Vulkan: Tried to make a command allocator with a queue type that does not exist.");
		break;
	}

	VkCommandPool command_pool;

	VKASSERT(vkCreateCommandPool(s_vulkan_inst->device,
		&pool_create_info,
		nullptr,
		&command_pool),
		"Vulkan: Failed to create command pool.");

	VkCommandBufferAllocateInfo list_create_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	list_create_info.commandPool = command_pool;
	list_create_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	list_create_info.commandBufferCount = a_command_list_count;

	VkCommandBuffer* cmd_buffers = reinterpret_cast<VkCommandBuffer*>(a_plists);

	VKASSERT(vkAllocateCommandBuffers(s_vulkan_inst->device,
		&list_create_info,
		cmd_buffers),
		"Vulkan: Failed to allocate command buffers!");


	a_pool = RCommandPool(reinterpret_cast<uintptr_t>(command_pool));
}

void Vulkan::FreeCommandPool(const RCommandPool a_pool)
{
	vkDestroyCommandPool(s_vulkan_inst->device, reinterpret_cast<VkCommandPool>(a_pool.handle), nullptr);
}

const GPUBuffer Vulkan::CreateBuffer(const GPUBufferCreateInfo& a_create_info)
{
	VulkanBuffer buffer;

	VkBufferCreateInfo buffer_info{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	buffer_info.size = a_create_info.size;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo vma_alloc{};

	switch (a_create_info.type)
	{
	case BUFFER_TYPE::UPLOAD:
		buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		break;
	case BUFFER_TYPE::STORAGE:
		buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		break;
	case BUFFER_TYPE::UNIFORM:
		buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		break;
	case BUFFER_TYPE::VERTEX:
		buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		break;
	case BUFFER_TYPE::INDEX:
		buffer_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		break;
	default:
		BB_ASSERT(false, "unknown buffer type");
		break;
	}

	if (a_create_info.host_writable)
	{
		vma_alloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
		vma_alloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	}
	else
		vma_alloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	VKASSERT(vmaCreateBuffer(s_vulkan_inst->vma,
		&buffer_info, &vma_alloc,
		&buffer.buffer, &buffer.allocation,
		nullptr), "Vulkan::VMA, Failed to allocate memory");

	SetDebugName(a_create_info.name, buffer.buffer, VK_OBJECT_TYPE_BUFFER);

	return GPUBuffer(s_vulkan_inst->buffers.insert(buffer));
}

void Vulkan::FreeBuffer(const GPUBuffer a_buffer)
{
	const VulkanBuffer& buf = s_vulkan_inst->buffers.find(a_buffer);
	vmaDestroyBuffer(s_vulkan_inst->vma, buf.buffer, buf.allocation);
	s_vulkan_inst->buffers.erase(a_buffer);
}

const RImage Vulkan::CreateImage(const ImageCreateInfo& a_create_info)
{
	VkImageCreateInfo image_create_info{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	image_create_info.extent.width = a_create_info.width;
	image_create_info.extent.height = a_create_info.height;
	image_create_info.extent.depth = a_create_info.depth;
	image_create_info.mipLevels = a_create_info.mip_levels;
	image_create_info.arrayLayers = a_create_info.array_layers;

	image_create_info.imageType = ImageTypes(a_create_info.type);
	image_create_info.tiling = ImageTilings(a_create_info.tiling);
	image_create_info.format = ImageFormats(a_create_info.format);
	image_create_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	//Will be defined in the first layout transition.
	image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_create_info.flags = 0;

	VmaAllocationCreateInfo alloc_info{};
	alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	VulkanImage image;
	VKASSERT(vmaCreateImage(s_vulkan_inst->vma, 
		&image_create_info,
		&alloc_info,
		&image.image,
		&image.allocation,
		nullptr), 
		"Vulkan: Failed to create image");

	SetDebugName(a_create_info.name, image.image, VK_OBJECT_TYPE_IMAGE);

	return RImage(s_vulkan_inst->images.insert(image).handle);
}

void Vulkan::FreeImage(const RImage a_image)
{
	const VulkanImage& image = s_vulkan_inst->images.find(a_image);
	vmaDestroyImage(s_vulkan_inst->vma, image.image, image.allocation);
	s_vulkan_inst->images.erase(a_image);
}

const RImageView Vulkan::CreateViewImage(const ImageViewCreateInfo& a_create_info)
{
	const VulkanImage image = s_vulkan_inst->images.find(a_create_info.image);

	VkImageViewCreateInfo view_info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	view_info.image = image.image;
	view_info.viewType = ImageViewTypes(a_create_info.type);
	view_info.format = ImageFormats(a_create_info.format);
	view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_info.subresourceRange.baseMipLevel = 0;
	view_info.subresourceRange.levelCount = a_create_info.mip_levels;
	view_info.subresourceRange.baseArrayLayer = 0;
	view_info.subresourceRange.layerCount = a_create_info.array_layers;

	VkImageView view;
	VKASSERT(vkCreateImageView(s_vulkan_inst->device, 
		&view_info, 
		nullptr,
		&view),
		"Vulkan: Failed to create image view.");

	SetDebugName(a_create_info.name, view, VK_OBJECT_TYPE_IMAGE_VIEW);

	return RImageView(reinterpret_cast<uintptr_t>(view));
}

void Vulkan::FreeViewImage(const RImageView a_image_view)
{
	vkDestroyImageView(s_vulkan_inst->device, 
		reinterpret_cast<VkImageView>(a_image_view.handle),
			nullptr);
}

const RDepthBuffer Vulkan::CreateDepthBuffer(const RenderDepthCreateInfo& a_create_info)
{
	VkImageCreateInfo image_create_info{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	image_create_info.extent.width = a_create_info.width;
	image_create_info.extent.height = a_create_info.height;
	image_create_info.extent.depth = a_create_info.depth;
	image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_create_info.imageType = VK_IMAGE_TYPE_2D;
	image_create_info.format = DepthFormat(a_create_info.depth_format);
	image_create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	image_create_info.mipLevels = 1;
	image_create_info.arrayLayers = 1;
	//Will be defined in the first layout transition.
	image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_create_info.flags = 0;

	VkImageViewCreateInfo view_info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_info.format = image_create_info.format;
	if (a_create_info.depth_format == DEPTH_FORMAT::D32_SFLOAT)
		view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	else
		view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	view_info.subresourceRange.baseMipLevel = 0;
	view_info.subresourceRange.levelCount = image_create_info.mipLevels;
	view_info.subresourceRange.baseArrayLayer = 0;
	view_info.subresourceRange.layerCount = image_create_info.arrayLayers;

	VmaAllocationCreateInfo alloc_info{};
	alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	VulkanDepth depth;
	VKASSERT(vmaCreateImage(s_vulkan_inst->vma, &image_create_info, &alloc_info, &depth.image, &depth.allocation, nullptr), "Vulkan: Failed to create image");
	view_info.image = depth.image;
	VKASSERT(vkCreateImageView(s_vulkan_inst->device, &view_info, nullptr, &depth.view), "Vulkan: Failed to create image view.");

	SetDebugName(a_create_info.name, depth.image, VK_OBJECT_TYPE_IMAGE);
	SetDebugName(a_create_info.name, depth.view, VK_OBJECT_TYPE_IMAGE_VIEW);

	return RDepthBuffer(s_vulkan_inst->depth_images.insert(depth).handle);
}

void Vulkan::FreeDepthBuffer(const RDepthBuffer a_depth_buffer)
{
	const VulkanDepth& image = s_vulkan_inst->depth_images.find(a_depth_buffer);
	vkDestroyImageView(s_vulkan_inst->device, image.view, nullptr);
	vmaDestroyImage(s_vulkan_inst->vma, image.image, image.allocation);
	s_vulkan_inst->depth_images.erase(a_depth_buffer);
}

RDescriptorLayout Vulkan::CreateDescriptorLayout(Allocator a_temp_allocator, Slice<DescriptorBindingInfo> a_bindings)
{
	VkDescriptorSetLayout set_layout;

	VkDescriptorSetLayoutBinding* layout_binds = BBnewArr(
		a_temp_allocator,
		a_bindings.size(),
		VkDescriptorSetLayoutBinding);

	VkDescriptorBindingFlags* bindless_flags = BBnewArr(
		a_temp_allocator,
		a_bindings.size(),
		VkDescriptorBindingFlags);
	bool is_bindless = false;

	for (size_t i = 0; i < a_bindings.size(); i++)
	{
		const DescriptorBindingInfo& binding = a_bindings[i];
		layout_binds[i].binding = binding.binding;
		layout_binds[i].descriptorCount = binding.count;
		layout_binds[i].descriptorType = DescriptorBufferType(binding.type);
		layout_binds[i].stageFlags = ShaderStageFlags(binding.shader_stage);

		if (binding.count > 1)
		{
			is_bindless = true;
			bindless_flags[i] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
				VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT;
		}
		else
			bindless_flags[i] = 0;
	}

	VkDescriptorSetLayoutCreateInfo layout_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	layout_info.pBindings = layout_binds;
	layout_info.bindingCount = static_cast<uint32_t>(a_bindings.size());
	layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

	if (is_bindless) //if bindless add another struct and return here.
	{
		VkDescriptorSetLayoutBindingFlagsCreateInfo t_LayoutExtInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT };
		t_LayoutExtInfo.bindingCount = static_cast<uint32_t>(a_bindings.size());
		t_LayoutExtInfo.pBindingFlags = bindless_flags;

		layout_info.pNext = &t_LayoutExtInfo;

		//Do some algorithm to see if I already made a descriptorlayout like this one.
		VKASSERT(vkCreateDescriptorSetLayout(s_vulkan_inst->device,
			&layout_info, nullptr, &set_layout),
			"Vulkan: Failed to create a descriptorsetlayout.");
	}
	else
	{
		//Do some algorithm to see if I already made a descriptorlayout like this one.
		VKASSERT(vkCreateDescriptorSetLayout(s_vulkan_inst->device,
			&layout_info, nullptr, &set_layout),
			"Vulkan: Failed to create a descriptorsetlayout.");
	}

	return RDescriptorLayout(reinterpret_cast<uintptr_t>(set_layout));
}

RDescriptorLayout Vulkan::CreateDescriptorSamplerLayout(const Slice<SamplerCreateInfo> a_static_samplers)
{
	BB_ASSERT(a_static_samplers.size() <= STATIC_SAMPLER_MAX, "too many static samplers on pipeline!");

	VkDescriptorSetLayoutBinding layout_binds[STATIC_SAMPLER_MAX];
	VkSampler samplers[STATIC_SAMPLER_MAX];
	for (uint32_t i = 0; i < a_static_samplers.size(); i++)
	{
		samplers[i] = CreateSampler(a_static_samplers[i]);
		layout_binds[i].binding = i;
		layout_binds[i].descriptorCount = 1;
		layout_binds[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		layout_binds[i].pImmutableSamplers = &samplers[i];
		layout_binds[i].stageFlags = VK_SHADER_STAGE_ALL;
	}
	VkDescriptorSetLayoutCreateInfo layout_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	layout_info.pBindings = layout_binds;
	layout_info.bindingCount = static_cast<uint32_t>(a_static_samplers.size());
	layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_EMBEDDED_IMMUTABLE_SAMPLERS_BIT_EXT | VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

	VkDescriptorSetLayout set_layout;
	//Do some algorithm to see if I already made a descriptorlayout like this one.
	VKASSERT(vkCreateDescriptorSetLayout(s_vulkan_inst->device,
		&layout_info, nullptr, &set_layout),
		"Vulkan: Failed to create a descriptorsetlayout for immutable samplers in pipeline init.");
	return RDescriptorLayout(reinterpret_cast<uintptr_t>(set_layout));
}

DescriptorAllocation Vulkan::AllocateDescriptor(const RDescriptorLayout a_descriptor)
{
	return s_vulkan_inst->pdescriptor_buffer->AllocateDescriptor(a_descriptor);
}

void Vulkan::WriteDescriptors(const WriteDescriptorInfos& a_write_info)
{
	for (size_t i = 0; i < a_write_info.data.size(); i++)
	{
		const WriteDescriptorData& write_data = a_write_info.data[i];

		union VkDescData
		{
			VkDescriptorAddressInfoEXT buffer;
			VkDescriptorImageInfo image;
		};

		VkDescData data{};

		VkDeviceSize descriptor_size;
		VkDescriptorGetInfoEXT desc_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
		switch (write_data.type)
		{
		case DESCRIPTOR_TYPE::READONLY_CONSTANT:
			data.buffer = GetDescriptorAddressInfo(s_vulkan_inst->device, write_data.buffer_view);
			desc_info.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			desc_info.data.pUniformBuffer = &data.buffer;
			descriptor_size = s_vulkan_inst->descriptor_sizes.uniform_buffer;
			break;
		case DESCRIPTOR_TYPE::READONLY_BUFFER:
		case DESCRIPTOR_TYPE::READWRITE:
			data.buffer = GetDescriptorAddressInfo(s_vulkan_inst->device, write_data.buffer_view);
			desc_info.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			desc_info.data.pStorageBuffer = &data.buffer;
			descriptor_size = s_vulkan_inst->descriptor_sizes.storage_buffer;
			break;
		case DESCRIPTOR_TYPE::IMAGE:
			data.image.imageView = reinterpret_cast<VkImageView>(write_data.image_view.view.handle);
			data.image.imageLayout = ImageLayout(write_data.image_view.layout);
			data.image.sampler = VK_NULL_HANDLE; //we only do static samplers :)
			desc_info.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			desc_info.data.pSampledImage = &data.image;
			descriptor_size = s_vulkan_inst->descriptor_sizes.sampled_image;
			break;
		default:
			BB_ASSERT(false, "Vulkan: DESCRIPTOR_TYPE failed to convert to a VkDescriptorType.");
			desc_info.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptor_size = 0;
			break;
		}

		VkDeviceSize descriptor_offset;
		s_vulkan_inst->pfn.GetDescriptorSetLayoutBindingOffsetEXT(s_vulkan_inst->device,
			reinterpret_cast<VkDescriptorSetLayout>(a_write_info.descriptor_layout.handle),
			write_data.binding,
			&descriptor_offset);

		descriptor_offset += descriptor_size * write_data.descriptor_index;
		void* descriptor_mem = Pointer::Add(a_write_info.allocation.buffer_start, a_write_info.allocation.offset + descriptor_offset);

		s_vulkan_inst->pfn.GetDescriptorEXT(s_vulkan_inst->device, &desc_info, descriptor_size, descriptor_mem);
	}
}

//we won't have that many pipeline layouts, so make a basic one.
static uint64_t PipelineLayoutCreateInfoHash(const VkPipelineLayoutCreateInfo& a_info)
{
	uint64_t hash = static_cast<uint64_t>(a_info.pushConstantRangeCount + a_info.setLayoutCount);

	for (size_t i = 0; i < a_info.pushConstantRangeCount; i++)
	{
		hash *= a_info.pPushConstantRanges[i].size;
	}
	
	for (size_t i = 0; i < a_info.setLayoutCount; i++)
	{
		hash *= reinterpret_cast<uint64_t>(a_info.pSetLayouts[i]);
	}
	return hash;
}

RPipelineLayout Vulkan::CreatePipelineLayout(const RDescriptorLayout* a_descriptor_layouts, const uint32_t a_layout_count, const PushConstantRange* a_constant_ranges, const uint32_t a_constant_range_count)
{
	VkPipelineLayoutCreateInfo create_info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	create_info.setLayoutCount = a_layout_count;
	create_info.pSetLayouts = reinterpret_cast<const VkDescriptorSetLayout*>(a_descriptor_layouts);
	
	if (a_constant_range_count)
	{
		VkPushConstantRange* constant_ranges = BBstackAlloc(a_constant_range_count, VkPushConstantRange);
		for (size_t i = 0; i < a_constant_range_count; i++)
		{
			constant_ranges[i].stageFlags = ShaderStageFlags(a_constant_ranges[i].stages);
			constant_ranges[i].size = a_constant_ranges[i].size;
			constant_ranges[i].offset = a_constant_ranges[i].offset;
		}

		create_info.pushConstantRangeCount = a_constant_range_count;
		create_info.pPushConstantRanges = constant_ranges;

		const uint64_t pipeline_hash = PipelineLayoutCreateInfoHash(create_info);
		if (VkPipelineLayout* layout = s_vulkan_inst->pipeline_layout_cache.find(pipeline_hash))
			return RPipelineLayout(reinterpret_cast<uintptr_t>(*layout));
		else
		{
			VkPipelineLayout pipe_layout;
			VKASSERT(vkCreatePipelineLayout(s_vulkan_inst->device, &create_info, nullptr, &pipe_layout),
				"Failed to create vulkan pipeline layout");

			s_vulkan_inst->pipeline_layout_cache.insert(pipeline_hash, pipe_layout);

			return RPipelineLayout(reinterpret_cast<uintptr_t>(pipe_layout));
		}
	}
	else
	{
		create_info.pushConstantRangeCount = 0;
		create_info.pPushConstantRanges = nullptr;

		const uint64_t pipeline_hash = PipelineLayoutCreateInfoHash(create_info);
		if (VkPipelineLayout* layout = s_vulkan_inst->pipeline_layout_cache.find(pipeline_hash))
			return RPipelineLayout(reinterpret_cast<uintptr_t>(*layout));
		else
		{
			VkPipelineLayout pipe_layout;
			VKASSERT(vkCreatePipelineLayout(s_vulkan_inst->device, &create_info, nullptr, &pipe_layout),
				"Failed to create vulkan pipeline layout");

			s_vulkan_inst->pipeline_layout_cache.insert(pipeline_hash, pipe_layout);

			return RPipelineLayout(reinterpret_cast<uintptr_t>(pipe_layout));
		}
	}
}

void Vulkan::FreePipelineLayout(const RPipelineLayout a_layout)
{
	vkDestroyPipelineLayout(s_vulkan_inst->device, 
		reinterpret_cast<VkPipelineLayout>(a_layout.handle),
		nullptr);
}

RPipeline Vulkan::CreatePipeline(const CreatePipelineInfo& a_info)
{
	VkShaderModule vertex_module;
	VkShaderModuleCreateInfo shader_mod_info{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	shader_mod_info.pCode = reinterpret_cast<const uint32_t*>(a_info.vertex.shader_code);
	shader_mod_info.codeSize = a_info.vertex.shader_code_size;

	VKASSERT(vkCreateShaderModule(s_vulkan_inst->device, &shader_mod_info, nullptr, &vertex_module),
		"Vulkan: Failed to create shadermodule.");

	VkShaderModule fragment_module;
	shader_mod_info.pCode = reinterpret_cast<const uint32_t*>(a_info.fragment.shader_code);
	shader_mod_info.codeSize = a_info.fragment.shader_code_size;

	VKASSERT(vkCreateShaderModule(s_vulkan_inst->device, &shader_mod_info, nullptr, &fragment_module),
		"Vulkan: Failed to create shadermodule.");

	VkPipelineShaderStageCreateInfo pipe_shader_info[2];
	pipe_shader_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipe_shader_info[0].pNext = nullptr;
	pipe_shader_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	pipe_shader_info[0].module = vertex_module;
	pipe_shader_info[0].pName = a_info.vertex.shader_entry;
	pipe_shader_info[0].pSpecializationInfo = nullptr;

	pipe_shader_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipe_shader_info[1].pNext = nullptr;
	pipe_shader_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	pipe_shader_info[1].module = fragment_module;
	pipe_shader_info[1].pName = a_info.fragment.shader_entry;
	pipe_shader_info[1].pSpecializationInfo = nullptr;

	VkPipelineRasterizationStateCreateInfo pipe_raster{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	pipe_raster.depthClampEnable = VK_FALSE;
	pipe_raster.depthBiasEnable = VK_FALSE;
	pipe_raster.rasterizerDiscardEnable = VK_FALSE;
	pipe_raster.polygonMode = VK_POLYGON_MODE_FILL;
	pipe_raster.cullMode = VK_CULL_MODE_BACK_BIT;
	pipe_raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	pipe_raster.lineWidth = 1.0f;
	pipe_raster.depthBiasConstantFactor = 0.0f; // Optional
	pipe_raster.depthBiasClamp = 0.0f; // Optional
	pipe_raster.depthBiasSlopeFactor = 0.0f; // Optional

	VkPipelineColorBlendAttachmentState color_attach{};
	color_attach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_attach.blendEnable = VK_TRUE;
	color_attach.colorBlendOp = VK_BLEND_OP_ADD;
	color_attach.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
	color_attach.dstColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
	color_attach.alphaBlendOp = VK_BLEND_OP_ADD;
	color_attach.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	color_attach.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

	VkPipelineColorBlendStateCreateInfo pipe_color_blend{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	pipe_color_blend.logicOp = VK_LOGIC_OP_CLEAR;
	pipe_color_blend.logicOpEnable = VK_FALSE;
	pipe_color_blend.attachmentCount = 1;
	pipe_color_blend.pAttachments = &color_attach;

	VkPipelineVertexInputStateCreateInfo pipe_vertex_input{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	pipe_vertex_input.vertexBindingDescriptionCount = 0;
	pipe_vertex_input.vertexAttributeDescriptionCount = 0;

	VkPipelineInputAssemblyStateCreateInfo input_assembly{};
	input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly.primitiveRestartEnable = VK_FALSE;

	//Get dynamic state for the viewport and scissor.
	VkDynamicState dynamic_states[2]{ VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT, VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT };
	VkPipelineDynamicStateCreateInfo dynamic_state_info{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamic_state_info.dynamicStateCount = 2;
	dynamic_state_info.pDynamicStates = dynamic_states;

	//Set viewport to nullptr and let the commandbuffer handle it via 
	VkPipelineViewportStateCreateInfo pipe_viewport_state{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	pipe_viewport_state.viewportCount = 0;
	pipe_viewport_state.pViewports = nullptr;
	pipe_viewport_state.scissorCount = 0;
	pipe_viewport_state.pScissors = nullptr;

	VkPipelineMultisampleStateCreateInfo multi_sampling{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	multi_sampling.sampleShadingEnable = VK_FALSE;
	multi_sampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multi_sampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multi_sampling.alphaToOneEnable = VK_FALSE; // Optional
	multi_sampling.minSampleShading = 1.0f; // Optional
	multi_sampling.pSampleMask = nullptr; // Optional

	VkPipelineRenderingCreateInfo pipeline_dynamic_rendering{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO }; //attachment for dynamic rendering.
	pipeline_dynamic_rendering.colorAttachmentCount = 1;
	pipeline_dynamic_rendering.pColorAttachmentFormats = &s_vulkan_swapchain->image_format;
	pipeline_dynamic_rendering.depthAttachmentFormat = DepthFormat(a_info.depth_format);
	pipeline_dynamic_rendering.stencilAttachmentFormat = DepthFormat(a_info.depth_format);
	pipeline_dynamic_rendering.pNext = nullptr;

	VkPipelineDepthStencilStateCreateInfo pipe_stencil{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	pipe_stencil.depthTestEnable = VK_TRUE;
	pipe_stencil.depthWriteEnable = VK_TRUE;
	pipe_stencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	pipe_stencil.back.compareMask = VK_COMPARE_OP_ALWAYS;
	pipe_stencil.depthBoundsTestEnable = VK_FALSE;
	pipe_stencil.minDepthBounds = 0.0f;
	pipe_stencil.maxDepthBounds = 0.0f;
	pipe_stencil.stencilTestEnable = VK_FALSE;

	VkGraphicsPipelineCreateInfo pipeline_info{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	pipeline_info.pStages = pipe_shader_info;
	pipeline_info.stageCount = _countof(pipe_shader_info);
	pipeline_info.pRasterizationState = &pipe_raster;
	pipeline_info.pColorBlendState = &pipe_color_blend;
	pipeline_info.pVertexInputState = &pipe_vertex_input;
	pipeline_info.pInputAssemblyState = &input_assembly;
	pipeline_info.layout = reinterpret_cast<VkPipelineLayout>(a_info.layout.handle);
	pipeline_info.pDynamicState = &dynamic_state_info;
	pipeline_info.pViewportState = &pipe_viewport_state;
	pipeline_info.pMultisampleState = &multi_sampling;
	pipeline_info.pDepthStencilState = &pipe_stencil;
	pipeline_info.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
	pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
	pipeline_info.basePipelineIndex = -1;
	pipeline_info.pNext = &pipeline_dynamic_rendering;

	VkPipeline pipeline;
	VKASSERT(vkCreateGraphicsPipelines(s_vulkan_inst->device,
		VK_NULL_HANDLE,
		1,
		&pipeline_info,
		nullptr,
		&pipeline),
		"failed to create graphics pipeline");

	vkDestroyShaderModule(s_vulkan_inst->device, vertex_module, nullptr);
	vkDestroyShaderModule(s_vulkan_inst->device, fragment_module, nullptr);

	return RPipeline(reinterpret_cast<uintptr_t>(pipeline));
}

void Vulkan::CreateShaderObject(Allocator a_temp_allocator, Slice<ShaderObjectCreateInfo> a_shader_objects, ShaderObject* a_pshader_objects)
{
	VkShaderCreateInfoEXT* shader_create_infos = BBnewArr(a_temp_allocator, a_shader_objects.size(), VkShaderCreateInfoEXT);

	for (size_t i = 0; i < a_shader_objects.size(); i++)
	{
		const ShaderObjectCreateInfo& shad_info = a_shader_objects[i];
		VkShaderCreateInfoEXT& create_inf = shader_create_infos[i];
		create_inf.sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT;
		create_inf.pNext = nullptr;
		create_inf.flags = VK_SHADER_CREATE_LINK_STAGE_BIT_EXT;
		create_inf.stage = static_cast<VkShaderStageFlagBits>(ShaderStageFlags(shad_info.stage));
		create_inf.nextStage = ShaderStageFlagsFromFlags(shad_info.next_stages);
		create_inf.codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT; //for now always SPIR-V
		create_inf.codeSize = shad_info.shader_code_size;
		create_inf.pCode = shad_info.shader_code;
		create_inf.pName = shad_info.shader_entry;
		create_inf.setLayoutCount = shad_info.descriptor_layout_count;
		create_inf.pSetLayouts = reinterpret_cast<VkDescriptorSetLayout*>(shad_info.descriptor_layouts);

		if (shad_info.push_constant_range_count)
		{
			VkPushConstantRange* constant_ranges = BBnewArr(a_temp_allocator, shad_info.push_constant_range_count, VkPushConstantRange);
			for (size_t const_range = 0; const_range < shad_info.push_constant_range_count; const_range++)
			{
				constant_ranges[const_range].stageFlags = ShaderStageFlags(shad_info.push_constant_ranges[const_range].stages);
				constant_ranges[const_range].size = shad_info.push_constant_ranges[const_range].size;
				constant_ranges[const_range].offset = shad_info.push_constant_ranges[const_range].offset;
			}
			create_inf.pPushConstantRanges = constant_ranges;
		}
		else
			create_inf.pPushConstantRanges = nullptr;
		create_inf.pushConstantRangeCount = shad_info.push_constant_range_count;
		create_inf.pSpecializationInfo = nullptr;
	}

	VKASSERT(s_vulkan_inst->pfn.CreateShaderEXT(s_vulkan_inst->device,
		static_cast<uint32_t>(a_shader_objects.size()),
		shader_create_infos,
		nullptr,
		reinterpret_cast<VkShaderEXT*>(a_pshader_objects)),
		"Failed to create shader objects!");
}

void Vulkan::DestroyShaderObject(const ShaderObject a_shader_object)
{
	s_vulkan_inst->pfn.DestroyShaderEXT(s_vulkan_inst->device,
		reinterpret_cast<VkShaderEXT>(a_shader_object.handle),
		nullptr);
}

void* Vulkan::MapBufferMemory(const GPUBuffer a_buffer)
{
	const VulkanBuffer& buffer = s_vulkan_inst->buffers.find(a_buffer);
	void* mapped;
	vmaMapMemory(s_vulkan_inst->vma, buffer.allocation, &mapped);
	return mapped;
}

void Vulkan::UnmapBufferMemory(const GPUBuffer a_buffer)
{
	const VulkanBuffer& buffer = s_vulkan_inst->buffers.find(a_buffer);
	vmaUnmapMemory(s_vulkan_inst->vma, buffer.allocation);
}

void Vulkan::ResetCommandPool(const RCommandPool a_pool)
{
	const VkCommandPool cmd_pool = reinterpret_cast<VkCommandPool>(a_pool.handle);
	vkResetCommandPool(s_vulkan_inst->device, cmd_pool, 0);
}

void Vulkan::StartCommandList(const RCommandList a_list, const char* a_name)
{
	const VkCommandBuffer cmd_list = reinterpret_cast<VkCommandBuffer>(a_list.handle);
	VkCommandBufferBeginInfo cmd_begin_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	VKASSERT(vkBeginCommandBuffer(cmd_list,
		&cmd_begin_info),
		"Vulkan: Failed to begin commandbuffer");

	//jank? bind the single descriptor buffer that we have when starting the commandlist.
	VkDescriptorBufferBindingInfoEXT descriptor_buffer_info { VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT };
	descriptor_buffer_info.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;
	descriptor_buffer_info.address = s_vulkan_inst->pdescriptor_buffer->GPUStartAddress();
	s_vulkan_inst->pfn.CmdBindDescriptorBuffersEXT(cmd_list, 1, &descriptor_buffer_info);

	SetDebugName(a_name, cmd_list, VK_OBJECT_TYPE_COMMAND_BUFFER);
}

void Vulkan::EndCommandList(const RCommandList a_list)
{
	const VkCommandBuffer cmd_list = reinterpret_cast<VkCommandBuffer>(a_list.handle);
	VKASSERT(vkEndCommandBuffer(cmd_list), "Vulkan: Error when trying to end commandbuffer!");
	SetDebugName(nullptr, cmd_list, VK_OBJECT_TYPE_COMMAND_BUFFER);
}

void Vulkan::CopyBuffer(const RCommandList a_list, const RenderCopyBuffer& a_copy_buffer)
{
	const VkCommandBuffer cmd_list = reinterpret_cast<VkCommandBuffer>(a_list.handle);
	const VkBuffer dst_buf = s_vulkan_inst->buffers.find(a_copy_buffer.dst).buffer;
	const VkBuffer src_buf = s_vulkan_inst->buffers.find(a_copy_buffer.src).buffer;

	VkBufferCopy* copy_regions = BBstackAlloc(a_copy_buffer.regions.size(), VkBufferCopy);
	for (size_t i = 0; i < a_copy_buffer.regions.size(); i++)
	{
		const RenderCopyBufferRegion& r_cpy_reg = a_copy_buffer.regions[i];
		VkBufferCopy& cpy_reg = copy_regions[i];

		cpy_reg.size = r_cpy_reg.size;
		cpy_reg.dstOffset = r_cpy_reg.dst_offset;
		cpy_reg.srcOffset = r_cpy_reg.src_offset;
	}
	
	vkCmdCopyBuffer(cmd_list,
		src_buf,
		dst_buf,
		static_cast<uint32_t>(a_copy_buffer.regions.size()),
		copy_regions);
}

void Vulkan::CopyBuffers(const RCommandList a_list, const RenderCopyBuffer* a_copy_buffers, const uint32_t a_copy_buffer_count)
{
	const VkCommandBuffer cmd_list = reinterpret_cast<VkCommandBuffer>(a_list.handle);

	for (size_t i = 0; i < a_copy_buffer_count; i++)
	{
		const RenderCopyBuffer& cpy_buf = a_copy_buffers[i];
		const VulkanBuffer& dst_buf = s_vulkan_inst->buffers.find(cpy_buf.dst);
		const VulkanBuffer& src_buf = s_vulkan_inst->buffers.find(cpy_buf.src);

		VkBufferCopy* copy_regions = BBstackAlloc(cpy_buf.regions.size(), VkBufferCopy);
		for (size_t cpy_reg_index = 0; cpy_reg_index < cpy_buf.regions.size(); cpy_reg_index++)
		{
			const RenderCopyBufferRegion& r_cpy_reg = cpy_buf.regions[cpy_reg_index];
			VkBufferCopy& cpy_reg = copy_regions[cpy_reg_index];

			cpy_reg.size = r_cpy_reg.size;
			cpy_reg.dstOffset = r_cpy_reg.dst_offset;
			cpy_reg.srcOffset = r_cpy_reg.src_offset;
		}

		vkCmdCopyBuffer(cmd_list,
			src_buf.buffer,
			dst_buf.buffer,
			static_cast<uint32_t>(cpy_buf.regions.size()),
			copy_regions);
	}
}

void Vulkan::CopyBufferImage(const RCommandList a_list, const RenderCopyBufferToImageInfo& a_copy_info)
{
	const VkCommandBuffer cmd_list = reinterpret_cast<VkCommandBuffer>(a_list.handle);

	const VulkanBuffer& src_buf = s_vulkan_inst->buffers.find(a_copy_info.src_buffer);
	const VulkanImage& dst_image = s_vulkan_inst->images.find(a_copy_info.dst_image);

	VkBufferImageCopy copy_image;
	copy_image.bufferOffset = a_copy_info.src_offset;
	copy_image.bufferImageHeight = 0;
	copy_image.bufferRowLength = 0;

	copy_image.imageExtent.width = a_copy_info.dst_image_info.size_x;
	copy_image.imageExtent.height = a_copy_info.dst_image_info.size_y;
	copy_image.imageExtent.depth = a_copy_info.dst_image_info.size_z;
	
	copy_image.imageOffset.x = a_copy_info.dst_image_info.offset_x;
	copy_image.imageOffset.y = a_copy_info.dst_image_info.offset_y;
	copy_image.imageOffset.z = a_copy_info.dst_image_info.offset_z;

	copy_image.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy_image.imageSubresource.mipLevel = a_copy_info.dst_image_info.mip_level;
	copy_image.imageSubresource.baseArrayLayer = a_copy_info.dst_image_info.base_array_layer;
	copy_image.imageSubresource.layerCount = a_copy_info.dst_image_info.layer_count;

	vkCmdCopyBufferToImage(cmd_list,
		src_buf.buffer,
		dst_image.image,
		ImageLayout(a_copy_info.dst_image_info.layout),
		1,
		&copy_image);
}

static inline uint32_t QueueTransitionIndex(const QUEUE_TRANSITION a_Transition)
{
	switch (a_Transition)
	{
	case QUEUE_TRANSITION::GRAPHICS:	return s_vulkan_inst->queue_indices.graphics;
	case QUEUE_TRANSITION::TRANSFER:	return s_vulkan_inst->queue_indices.transfer;
	case QUEUE_TRANSITION::COMPUTE:		return s_vulkan_inst->queue_indices.compute;
	default:
		BB_ASSERT(false, "Vulkan: queue transition not supported!");
		return VK_QUEUE_FAMILY_IGNORED;
	}
}

void Vulkan::PipelineBarriers(const RCommandList a_list, const PipelineBarrierInfo& a_BarrierInfo)
{
	VkMemoryBarrier2* global_barriers = BBstackAlloc(a_BarrierInfo.global_info_count, VkMemoryBarrier2);
	VkBufferMemoryBarrier2* buffer_barriers = BBstackAlloc(a_BarrierInfo.buffer_info_count, VkBufferMemoryBarrier2);
	VkImageMemoryBarrier2* image_barriers = BBstackAlloc(a_BarrierInfo.image_info_count, VkImageMemoryBarrier2);

	for (size_t i = 0; i < a_BarrierInfo.global_info_count; i++)
	{
		const PipelineBarrierGlobalInfo& barrier_info = a_BarrierInfo.global_infos[i];

		global_barriers[i].sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
		global_barriers[i].pNext = nullptr;
		global_barriers[i].srcAccessMask = AccessMask(barrier_info.src_mask);
		global_barriers[i].dstAccessMask = AccessMask(barrier_info.dst_mask);
		global_barriers[i].srcStageMask = PipelineStage(barrier_info.src_stage);
		global_barriers[i].dstStageMask = PipelineStage(barrier_info.dst_stage);
	}

	for (size_t i = 0; i < a_BarrierInfo.buffer_info_count; i++)
	{
		const PipelineBarrierBufferInfo& barrier_info = a_BarrierInfo.buffer_infos[i];

		buffer_barriers[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
		buffer_barriers[i].pNext = nullptr;
		buffer_barriers[i].srcAccessMask = AccessMask(barrier_info.src_mask);
		buffer_barriers[i].dstAccessMask = AccessMask(barrier_info.dst_mask);
		buffer_barriers[i].srcStageMask = PipelineStage(barrier_info.src_stage);
		buffer_barriers[i].dstStageMask = PipelineStage(barrier_info.dst_stage);
		//if we do no transition on the source queue. Then set it all to false.
		if (barrier_info.src_queue == QUEUE_TRANSITION::NO_TRANSITION)
		{
			buffer_barriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			buffer_barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		}
		else
		{
			buffer_barriers[i].srcQueueFamilyIndex = QueueTransitionIndex(barrier_info.src_queue);
			buffer_barriers[i].dstQueueFamilyIndex = QueueTransitionIndex(barrier_info.dst_queue);
		}
		buffer_barriers[i].buffer = reinterpret_cast<VulkanBuffer*>(barrier_info.buffer.handle)->buffer;
		buffer_barriers[i].offset = barrier_info.offset;
		buffer_barriers[i].size = barrier_info.size;
	}

	for (size_t i = 0; i < a_BarrierInfo.image_info_count; i++)
	{
		const PipelineBarrierImageInfo& barrier_info = a_BarrierInfo.image_infos[i];

		image_barriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		image_barriers[i].pNext = nullptr;
		image_barriers[i].srcAccessMask = AccessMask(barrier_info.src_mask);
		image_barriers[i].dstAccessMask = AccessMask(barrier_info.dst_mask);
		image_barriers[i].srcStageMask = PipelineStage(barrier_info.src_stage);
		image_barriers[i].dstStageMask = PipelineStage(barrier_info.dst_stage);
		//if we do no transition on the source queue. Then set it all to false.
		if (barrier_info.src_queue == QUEUE_TRANSITION::NO_TRANSITION)
		{
			image_barriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			image_barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		}
		else
		{
			image_barriers[i].srcQueueFamilyIndex = QueueTransitionIndex(barrier_info.src_queue);
			image_barriers[i].dstQueueFamilyIndex = QueueTransitionIndex(barrier_info.dst_queue);
		}
		image_barriers[i].oldLayout = ImageLayout(barrier_info.old_layout);
		image_barriers[i].newLayout = ImageLayout(barrier_info.new_layout);
		image_barriers[i].image =  s_vulkan_inst->images.find(barrier_info.image).image;
		if (barrier_info.new_layout == IMAGE_LAYOUT::DEPTH_STENCIL_ATTACHMENT ||
			barrier_info.old_layout == IMAGE_LAYOUT::DEPTH_STENCIL_ATTACHMENT)
			image_barriers[i].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		else
			image_barriers[i].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		image_barriers[i].subresourceRange.baseMipLevel = barrier_info.base_mip_level;
		image_barriers[i].subresourceRange.levelCount = barrier_info.level_count;
		image_barriers[i].subresourceRange.baseArrayLayer = barrier_info.base_array_layer;
		image_barriers[i].subresourceRange.layerCount = barrier_info.layer_count;
	}

	VkDependencyInfo dependency_info{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	dependency_info.memoryBarrierCount = a_BarrierInfo.global_info_count;
	dependency_info.pMemoryBarriers = global_barriers;
	dependency_info.bufferMemoryBarrierCount = a_BarrierInfo.buffer_info_count;
	dependency_info.pBufferMemoryBarriers = buffer_barriers;
	dependency_info.imageMemoryBarrierCount = a_BarrierInfo.image_info_count;
	dependency_info.pImageMemoryBarriers = image_barriers;

	const VkCommandBuffer cmd_buffer = reinterpret_cast<VkCommandBuffer>(a_list.handle);
	vkCmdPipelineBarrier2(cmd_buffer, &dependency_info);
}

void Vulkan::StartRendering(const RCommandList a_list, const StartRenderingInfo& a_render_info, const uint32_t a_backbuffer_index)
{
	const VkCommandBuffer cmd_buffer = reinterpret_cast<VkCommandBuffer>(a_list.handle);

	//include depth stencil later.
	VkImageMemoryBarrier2 image_barriers[2]{};
	image_barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	image_barriers[0].dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	image_barriers[0].srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
	image_barriers[0].dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	image_barriers[0].oldLayout = ImageLayout(a_render_info.initial_layout);
	image_barriers[0].newLayout = ImageLayout(a_render_info.final_layout);
	image_barriers[0].image = s_vulkan_swapchain->frames[a_backbuffer_index].image;
	image_barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_barriers[0].subresourceRange.baseArrayLayer = 0;
	image_barriers[0].subresourceRange.layerCount = 1;
	image_barriers[0].subresourceRange.baseMipLevel = 0;
	image_barriers[0].subresourceRange.levelCount = 1;

	VkRenderingInfo rendering_info{ VK_STRUCTURE_TYPE_RENDERING_INFO };
	VkRenderingAttachmentInfo depth_attachment;

	//If we handle the depth stencil we do that here. 
	if (a_render_info.depth_buffer.handle != BB_INVALID_HANDLE_64)
	{
		const VulkanDepth& depth_buffer = s_vulkan_inst->depth_images.find(a_render_info.depth_buffer);

		image_barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		image_barriers[1].dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		image_barriers[1].srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
		image_barriers[1].dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
		image_barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image_barriers[1].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		image_barriers[1].image = depth_buffer.image;
		image_barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		image_barriers[1].subresourceRange.baseArrayLayer = 0;
		image_barriers[1].subresourceRange.layerCount = 1;
		image_barriers[1].subresourceRange.baseMipLevel = 0;
		image_barriers[1].subresourceRange.levelCount = 1;

		VkDependencyInfo barrier_info{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
		barrier_info.pImageMemoryBarriers = image_barriers;
		barrier_info.imageMemoryBarrierCount = 2;

		vkCmdPipelineBarrier2(cmd_buffer, &barrier_info);

		depth_attachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
		depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depth_attachment.imageView = depth_buffer.view;
		depth_attachment.clearValue.depthStencil = { 1.0f, 0 };
		rendering_info.pDepthAttachment = &depth_attachment;
		rendering_info.pStencilAttachment = &depth_attachment;


#ifndef USE_G_PIPELINE
		vkCmdSetStencilTestEnable(cmd_buffer, VK_FALSE);

		vkCmdSetDepthBiasEnable(cmd_buffer, VK_TRUE);
		vkCmdSetDepthTestEnable(cmd_buffer, VK_TRUE);
		vkCmdSetDepthWriteEnable(cmd_buffer, VK_TRUE);
		vkCmdSetDepthCompareOp(cmd_buffer, VK_COMPARE_OP_LESS_OR_EQUAL);
		
		vkCmdSetDepthBias(cmd_buffer, 0.f, 0.f, 0.f);
		
#endif //USE_G_PIPELINE
	}
	else
	{
		VkDependencyInfo barrier_info{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
		barrier_info.pImageMemoryBarriers = image_barriers;
		barrier_info.imageMemoryBarrierCount = 1;

		vkCmdPipelineBarrier2(cmd_buffer, &barrier_info);

#ifndef USE_G_PIPELINE
		vkCmdSetStencilTestEnable(cmd_buffer, false);

		vkCmdSetDepthBiasEnable(cmd_buffer, VK_FALSE);
		vkCmdSetDepthTestEnable(cmd_buffer, VK_FALSE);
		vkCmdSetDepthWriteEnable(cmd_buffer, VK_FALSE);
#endif //USE_G_PIPELINE
	}

	VkRenderingAttachmentInfo rendering_attachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
	if (a_render_info.load_color)
		rendering_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	else
		rendering_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

	if (a_render_info.store_color)
		rendering_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	else
		rendering_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	rendering_attachment.imageLayout = ImageLayout(a_render_info.final_layout); //Get the layout after the memory barrier.
	rendering_attachment.imageView = s_vulkan_swapchain->frames[a_backbuffer_index].image_view;
	rendering_attachment.clearValue.color.float32[0] = a_render_info.clear_color_rgba.x;
	rendering_attachment.clearValue.color.float32[1] = a_render_info.clear_color_rgba.y;
	rendering_attachment.clearValue.color.float32[2] = a_render_info.clear_color_rgba.z;
	rendering_attachment.clearValue.color.float32[3] = a_render_info.clear_color_rgba.w;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent.width = a_render_info.viewport_width;
	scissor.extent.height = a_render_info.viewport_height;

	rendering_info.renderArea = scissor;
	rendering_info.layerCount = 1;
	rendering_info.pColorAttachments = &rendering_attachment;
	rendering_info.colorAttachmentCount = 1;

	vkCmdBeginRendering(cmd_buffer, &rendering_info);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(a_render_info.viewport_width);
	viewport.height = static_cast<float>(a_render_info.viewport_height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewportWithCount(cmd_buffer, 1, &viewport);

	vkCmdSetScissorWithCount(cmd_buffer, 1, &scissor);
}

void Vulkan::EndRendering(const RCommandList a_list, const EndRenderingInfo& a_rendering_info, const uint32_t a_backbuffer_index)
{
	const VkCommandBuffer cmd_buffer = reinterpret_cast<VkCommandBuffer>(a_list.handle);
	vkCmdEndRendering(cmd_buffer);

	VkImageMemoryBarrier2 present_barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
	present_barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	present_barrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
	present_barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	present_barrier.oldLayout = ImageLayout(a_rendering_info.initial_layout);
	present_barrier.newLayout = ImageLayout(a_rendering_info.final_layout);
	present_barrier.image = s_vulkan_swapchain->frames[a_backbuffer_index].image;
	present_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	present_barrier.subresourceRange.baseArrayLayer = 0;
	present_barrier.subresourceRange.layerCount = 1;
	present_barrier.subresourceRange.baseMipLevel = 0;
	present_barrier.subresourceRange.levelCount = 1;

	VkDependencyInfo barrier_info{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	barrier_info.pImageMemoryBarriers = &present_barrier;
	barrier_info.imageMemoryBarrierCount = 1;

	vkCmdPipelineBarrier2(cmd_buffer, &barrier_info);
}

void Vulkan::SetScissor(const RCommandList a_list, const ScissorInfo& a_scissor)
{
	const VkCommandBuffer cmd_buffer = reinterpret_cast<VkCommandBuffer>(a_list.handle);

	VkRect2D scissor;
	scissor.offset = { a_scissor.offset.x, a_scissor.offset.y };
	scissor.extent = { a_scissor.extent.x, a_scissor.extent.y };

	vkCmdSetScissorWithCount(cmd_buffer, 1, &scissor);
}

void Vulkan::BindVertexBuffer(const RCommandList a_list, const GPUBuffer a_buffer, const uint64_t a_offset)
{
	const VkCommandBuffer cmd_buffer = reinterpret_cast<VkCommandBuffer>(a_list.handle);

	vkCmdBindVertexBuffers(cmd_buffer,
		0,
		1,
		&s_vulkan_inst->buffers[a_buffer].buffer,
		&a_offset);
}

void Vulkan::BindIndexBuffer(const RCommandList a_list, const GPUBuffer a_buffer, const uint64_t a_offset)
{
	const VkCommandBuffer cmd_buffer = reinterpret_cast<VkCommandBuffer>(a_list.handle);

	vkCmdBindIndexBuffer(cmd_buffer,
		s_vulkan_inst->buffers[a_buffer].buffer,
		a_offset,
		VK_INDEX_TYPE_UINT32);
}

void Vulkan::BindPipeline(const RCommandList a_list, const RPipeline a_pipeline)
{
	const VkCommandBuffer cmd_buffer = reinterpret_cast<VkCommandBuffer>(a_list.handle);
	const VkPipeline pipeline = reinterpret_cast<VkPipeline>(a_pipeline.handle);

	vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

void Vulkan::BindShaders(const RCommandList a_list, const uint32_t a_UNIQUE_SHADER_STAGE_COUNT, const SHADER_STAGE* a_shader_stages, const ShaderObject* a_shader_objects)
{
	const VkCommandBuffer cmd_buffer = reinterpret_cast<VkCommandBuffer>(a_list.handle);

	VkShaderStageFlagBits* shader_stages = BBstackAlloc(a_UNIQUE_SHADER_STAGE_COUNT, VkShaderStageFlagBits);
	for (size_t i = 0; i < a_UNIQUE_SHADER_STAGE_COUNT; i++)
		shader_stages[i] = static_cast<VkShaderStageFlagBits>(ShaderStageFlags(a_shader_stages[i]));

	s_vulkan_inst->pfn.CmdBindShadersEXT(cmd_buffer, a_UNIQUE_SHADER_STAGE_COUNT, shader_stages, reinterpret_cast<const VkShaderEXT*>(a_shader_objects));

	vkCmdSetRasterizerDiscardEnable(cmd_buffer, VK_FALSE);

	vkCmdSetCullMode(cmd_buffer, VK_CULL_MODE_NONE);
	vkCmdSetFrontFace(cmd_buffer, VK_FRONT_FACE_CLOCKWISE);
	vkCmdSetPrimitiveTopology(cmd_buffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	s_vulkan_inst->pfn.CmdSetPolygonModeEXT(cmd_buffer, VK_POLYGON_MODE_FILL);
	s_vulkan_inst->pfn.CmdSetRasterizationSamplesEXT(cmd_buffer, VK_SAMPLE_COUNT_1_BIT);
	const uint32_t mask = UINT32_MAX;
	s_vulkan_inst->pfn.CmdSetSampleMaskEXT(cmd_buffer, VK_SAMPLE_COUNT_1_BIT, &mask);

	{
		VkBool32 color_enable = VK_TRUE;
		s_vulkan_inst->pfn.CmdSetColorBlendEnableEXT(cmd_buffer, 0, 1, &color_enable);
		VkColorComponentFlags color_flags = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		s_vulkan_inst->pfn.CmdSetColorWriteMaskEXT(cmd_buffer, 0, 1, &color_flags);

		VkColorBlendEquationEXT color_blend_equation{};
		color_blend_equation.colorBlendOp = VK_BLEND_OP_ADD;
		color_blend_equation.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		color_blend_equation.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		color_blend_equation.alphaBlendOp = VK_BLEND_OP_ADD;
		color_blend_equation.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend_equation.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		//color thing.
		s_vulkan_inst->pfn.CmdSetColorBlendEquationEXT(cmd_buffer, 0, 1, &color_blend_equation);
	}

	s_vulkan_inst->pfn.CmdSetAlphaToCoverageEnableEXT(cmd_buffer, VK_FALSE);
	//FOR imgui maybe not, but that is because I'm dumb asf
	s_vulkan_inst->pfn.CmdSetVertexInputEXT(cmd_buffer, 0, nullptr, 0, nullptr); 
	vkCmdSetPrimitiveRestartEnable(cmd_buffer, VK_FALSE);
}

void Vulkan::SetDescriptorImmutableSamplers(const RCommandList a_list, const RPipelineLayout a_pipe_layout)
{
	const VkCommandBuffer cmd_buffer = reinterpret_cast<VkCommandBuffer>(a_list.handle);

	s_vulkan_inst->pfn.CmdBindDescriptorBufferEmbeddedSamplersEXT(cmd_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		reinterpret_cast<VkPipelineLayout>(a_pipe_layout.handle),
		SPACE_IMMUTABLE_SAMPLER);
}

void Vulkan::SetDescriptorBufferOffset(const RCommandList a_list, const RPipelineLayout a_pipe_layout, const uint32_t a_first_set, const uint32_t a_set_count, const uint32_t* a_buffer_indices, const size_t* a_offsets)
{
	const VkCommandBuffer cmd_buffer = reinterpret_cast<VkCommandBuffer>(a_list.handle);

	s_vulkan_inst->pfn.CmdSetDescriptorBufferOffsetsEXT(cmd_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		reinterpret_cast<VkPipelineLayout>(a_pipe_layout.handle),
		a_first_set,
		a_set_count,
		a_buffer_indices,
		a_offsets);
}

void Vulkan::SetPushConstants(const RCommandList a_list, const RPipelineLayout a_pipe_layout, const uint32_t a_offset, const uint32_t a_size, const void* a_data)
{
	const VkCommandBuffer cmd_buffer = reinterpret_cast<VkCommandBuffer>(a_list.handle);
	vkCmdPushConstants(cmd_buffer, 
		reinterpret_cast<VkPipelineLayout>(a_pipe_layout.handle), 
		VK_SHADER_STAGE_ALL, 
		a_offset, 
		a_size, 
		a_data);
}

void Vulkan::DrawIndexed(const RCommandList a_list, const uint32_t a_index_count, const uint32_t a_instance_count, const uint32_t a_first_index, const int32_t a_vertex_offset, const uint32_t a_first_instance)
{
	const VkCommandBuffer cmd_buffer = reinterpret_cast<VkCommandBuffer>(a_list.handle);

	vkCmdDrawIndexed(cmd_buffer, a_index_count, a_instance_count, a_first_index, a_vertex_offset, a_first_instance);
}

void Vulkan::ExecuteCommandLists(const RQueue a_queue, const ExecuteCommandsInfo* a_execute_infos, const uint32_t a_execute_info_count)
{
	VkTimelineSemaphoreSubmitInfo* timeline_sem_infos = BBstackAlloc(
		a_execute_info_count,
		VkTimelineSemaphoreSubmitInfo);
	VkSubmitInfo* submiinfos = BBstackAlloc(
		a_execute_info_count,
		VkSubmitInfo);

	for (size_t i = 0; i < a_execute_info_count; i++)
	{
		const ExecuteCommandsInfo& exe_inf = a_execute_infos[i];
		VkTimelineSemaphoreSubmitInfo& cur_sem_inf = timeline_sem_infos[i];
		VkSubmitInfo& cur_sub_inf = submiinfos[i];

		cur_sem_inf.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
		cur_sem_inf.pNext = nullptr;
		cur_sem_inf.pWaitSemaphoreValues = exe_inf.wait_values;
		cur_sem_inf.waitSemaphoreValueCount = exe_inf.wait_count;
		cur_sem_inf.pSignalSemaphoreValues = exe_inf.signal_values;
		cur_sem_inf.signalSemaphoreValueCount = exe_inf.signal_count;

		cur_sub_inf.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		cur_sub_inf.pNext = &timeline_sem_infos[i];
		cur_sub_inf.commandBufferCount = exe_inf.list_count;
		cur_sub_inf.pCommandBuffers = reinterpret_cast<const VkCommandBuffer*>(exe_inf.lists);
		cur_sub_inf.waitSemaphoreCount = exe_inf.wait_count;
		cur_sub_inf.pWaitSemaphores = reinterpret_cast<const VkSemaphore*>(exe_inf.wait_fences);
		cur_sub_inf.signalSemaphoreCount = exe_inf.signal_count;
		cur_sub_inf.pSignalSemaphores = reinterpret_cast<const VkSemaphore*>(exe_inf.signal_fences);
	}

	VkQueue queue = reinterpret_cast<VkQueue>(a_queue.handle);
	VKASSERT(vkQueueSubmit(queue,
		a_execute_info_count,
		submiinfos,
		VK_NULL_HANDLE),
		"Vulkan: failed to submit to queue.");
}

void Vulkan::ExecutePresentCommandList(const RQueue a_queue, const ExecuteCommandsInfo& a_execute_info, const uint32_t a_backbuffer_index)
{
	//TEMP
	constexpr VkPipelineStageFlags WAIT_STAGES[8] = { VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT };

	//handle the window api for vulkan.
	const uint32_t wait_semaphore_count = a_execute_info.wait_count + 1;
	const uint32_t signal_semaphore_count = a_execute_info.signal_count + 1;

	VkSemaphore* wait_semaphores = BBstackAlloc(wait_semaphore_count, VkSemaphore);
	uint64_t* wait_values = BBstackAlloc(signal_semaphore_count, uint64_t);
	VkSemaphore* signal_semaphores = BBstackAlloc(signal_semaphore_count, VkSemaphore);
	uint64_t* signal_values = BBstackAlloc(signal_semaphore_count, uint64_t);

	//MEMCPY wait/signal values.

	Memory::Copy<VkSemaphore>(wait_semaphores, a_execute_info.wait_fences, a_execute_info.wait_count);
	Memory::Copy(wait_values, a_execute_info.wait_values, a_execute_info.wait_count);
	wait_semaphores[a_execute_info.wait_count] = s_vulkan_swapchain->frames[a_backbuffer_index].image_available_semaphore;
	wait_values[a_execute_info.wait_count] = 0;

	Memory::Copy<VkSemaphore>(signal_semaphores, a_execute_info.signal_fences, a_execute_info.signal_count);
	Memory::Copy(signal_values, a_execute_info.signal_values, a_execute_info.signal_count);
	signal_semaphores[a_execute_info.signal_count] = s_vulkan_swapchain->frames[a_backbuffer_index].present_finished_semaphore;
	signal_values[a_execute_info.signal_count] = 0;

	VkTimelineSemaphoreSubmitInfo timeline_sem_info{ VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
	timeline_sem_info.pWaitSemaphoreValues = wait_values;
	timeline_sem_info.waitSemaphoreValueCount = wait_semaphore_count;
	timeline_sem_info.pSignalSemaphoreValues = signal_values;
	timeline_sem_info.signalSemaphoreValueCount = signal_semaphore_count;

	VkSubmitInfo submiinfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submiinfo.pNext = &timeline_sem_info;
	submiinfo.commandBufferCount = a_execute_info.list_count;
	submiinfo.pCommandBuffers = reinterpret_cast<const VkCommandBuffer*>(a_execute_info.lists);
	submiinfo.waitSemaphoreCount = wait_semaphore_count;
	submiinfo.pWaitSemaphores = wait_semaphores;
	submiinfo.signalSemaphoreCount = signal_semaphore_count;
	submiinfo.pSignalSemaphores = signal_semaphores;
	submiinfo.pWaitDstStageMask = WAIT_STAGES;

	VkQueue queue = reinterpret_cast<VkQueue>(a_queue.handle);
	VKASSERT(vkQueueSubmit(queue,
		1,
		&submiinfo,
		VK_NULL_HANDLE),
		"Vulkan: failed to submit to queue.");
}

bool Vulkan::StartFrame(const uint32_t a_backbuffer_index)
{
	uint32_t image_index;
	VKASSERT(vkAcquireNextImageKHR(s_vulkan_inst->device,
		s_vulkan_swapchain->swapchain,
		UINT64_MAX,
		s_vulkan_swapchain->frames[a_backbuffer_index].image_available_semaphore,
		VK_NULL_HANDLE,
		&image_index),
		"Vulkan: failed to get next image.");

	return true;
}

bool Vulkan::EndFrame(const uint32_t a_backbuffer_index)
{
	VkPresentInfoKHR preseninfo{};
	preseninfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	preseninfo.waitSemaphoreCount = 1;
	preseninfo.pWaitSemaphores = &s_vulkan_swapchain->frames[a_backbuffer_index].present_finished_semaphore;
	preseninfo.swapchainCount = 1; //Swapchain will always be 1
	preseninfo.pSwapchains = &s_vulkan_swapchain->swapchain;
	preseninfo.pImageIndices = &a_backbuffer_index; //THIS MAY BE WRONG
	preseninfo.pResults = nullptr;

	VKASSERT(vkQueuePresentKHR(s_vulkan_inst->present_queue, &preseninfo),
		"Vulkan: Failed to queuepresentKHR.");

	return true;
}

RFence Vulkan::CreateFence(const uint64_t a_initial_value, const char* a_name)
{
	VkSemaphoreTypeCreateInfo timeline_semaphore_info{ VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
	timeline_semaphore_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	timeline_semaphore_info.initialValue = a_initial_value;

	VkSemaphoreCreateInfo semaphore_create_info{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	semaphore_create_info.pNext = &timeline_semaphore_info;

	VkSemaphore timeline_semaphore;
	vkCreateSemaphore(s_vulkan_inst->device,
		&semaphore_create_info,
		nullptr,
		&timeline_semaphore);

	SetDebugName(a_name, timeline_semaphore, VK_OBJECT_TYPE_SEMAPHORE);

	return RFence(reinterpret_cast<uintptr_t>(timeline_semaphore));
}

void Vulkan::FreeFence(const RFence a_fence)
{
	vkDestroySemaphore(s_vulkan_inst->device, reinterpret_cast<VkSemaphore>(a_fence.handle), nullptr);
}

void Vulkan::WaitFence(const RFence a_fence, const uint64_t a_fence_value)
{
	VkSemaphoreWaitInfo wait_info{ VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
	wait_info.semaphoreCount = 1;
	wait_info.pSemaphores = reinterpret_cast<const VkSemaphore*>(&a_fence);
	wait_info.pValues = &a_fence_value;

	vkWaitSemaphores(s_vulkan_inst->device, &wait_info, 1000000000);
}

void Vulkan::WaitFences(const RFence* a_fences, const uint64_t* a_fence_values, const uint32_t a_fence_count)
{
	VkSemaphoreWaitInfo wait_info{ VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
	wait_info.semaphoreCount = a_fence_count;
	wait_info.pSemaphores = reinterpret_cast<const VkSemaphore*>(a_fences);
	wait_info.pValues = a_fence_values;

	vkWaitSemaphores(s_vulkan_inst->device, &wait_info, 1000000000);
}

uint64_t Vulkan::GetCurrentFenceValue(const RFence a_fence)
{
	uint64_t value;
	vkGetSemaphoreCounterValue(s_vulkan_inst->device,
		reinterpret_cast<const VkSemaphore>(a_fence.handle),
		&value);
	return value;
}

RQueue Vulkan::GetQueue(const QUEUE_TYPE a_queue_type, const char* a_name)
{
	uint32_t queue_index;
	switch (a_queue_type)
	{
	case QUEUE_TYPE::GRAPHICS:
		queue_index = s_vulkan_inst->queue_indices.graphics;
		break;
	case QUEUE_TYPE::TRANSFER:
		queue_index = s_vulkan_inst->queue_indices.transfer;
		break;
	case QUEUE_TYPE::COMPUTE:
		queue_index = s_vulkan_inst->queue_indices.compute;
		break;
	default:
		BB_ASSERT(false, "Vulkan: Trying to get a device queue that you didn't setup yet.");
		queue_index = s_vulkan_inst->queue_indices.graphics;
		break;
	}
	VkQueue queue;
	vkGetDeviceQueue(s_vulkan_inst->device,
		queue_index,
		0,
		&queue);

	SetDebugName(a_name, queue, VK_OBJECT_TYPE_QUEUE);

	return RQueue(reinterpret_cast<uintptr_t>(queue));
}
#if defined(__GNUC__) || defined(__MINGW32__) || defined(__clang__) || defined(__clang_major__)
//for vulkan I initialize the struct name only. So ignore this warning
BB_PRAGMA(clang diagnostic pop)
#endif
