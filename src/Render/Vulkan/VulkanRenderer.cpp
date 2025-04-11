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

// enable more options for certain textures and buffers on their usage

#define BB_ENGINE_TOOLS_ENABLED
#ifdef BB_ENGINE_TOOLS_ENABLED
#define BB_EXTENDED_IMAGE_USAGE_FLAGS | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
#else
#define BB_EXTENDED_IMAGE_USAGE_FLAGS
#endif // BB_ENGINE_TOOLS_ENABLED


#ifdef _DEBUG
#define VKASSERT(vk_result, a_msg)  \
	do								\
	{								\
		if (vk_result != VK_SUCCESS)\
			BB_ASSERT(false, a_msg);	\
	} while (0)
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

// reflection from directx12.
constexpr size_t MAX_COLOR_ATTACHMENTS = 8;

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
	VkDebugUtilsMessengerCreateInfoEXT create_info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	create_info.messageSeverity =
		//VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		//VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
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

static bool CheckExtensionSupport(MemoryArena& a_temp_arena, Slice<const char*> a_extensions)
{
	// check extensions if they are available.
	uint32_t extension_count;
	vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
	VkExtensionProperties* extensions = ArenaAllocArr(a_temp_arena, VkExtensionProperties, extension_count);
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

static bool CheckValidationLayerSupport(MemoryArena& a_temp_arena, const Slice<const char*> a_layers)
{
	// check layers if they are available.
	uint32_t layer_count;
	vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
	VkLayerProperties* layers = ArenaAllocArr(a_temp_arena, VkLayerProperties, layer_count);
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
static bool QueueFindGraphicsBit(MemoryArena& a_temp_arena, VkPhysicalDevice a_physical_device)
{
	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(a_physical_device, &queue_family_count, nullptr);
	VkQueueFamilyProperties* queue_families = ArenaAllocArr(a_temp_arena, VkQueueFamilyProperties, queue_family_count);
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

static VkPhysicalDevice FindPhysicalDevice(MemoryArena& a_temp_arena, const VkInstance a_instance)
{
	uint32_t device_count = 0;
	vkEnumeratePhysicalDevices(a_instance, &device_count, nullptr);
	BB_ASSERT(device_count != 0, "Failed to find any GPU's with vulkan support.");
	VkPhysicalDevice* physical_device = ArenaAllocArr(a_temp_arena, VkPhysicalDevice, device_count);
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
			QueueFindGraphicsBit(a_temp_arena, physical_device[i]) &&
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

static VulkanQueuesIndices GetQueueIndices(MemoryArena& a_temp_arena, const VkPhysicalDevice a_phys_device)
{
	VulkanQueuesIndices return_value{};

	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(a_phys_device, &queue_family_count, nullptr);
	VkQueueFamilyProperties* const queue_families = ArenaAllocArr(a_temp_arena, VkQueueFamilyProperties, queue_family_count);
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

static VkDevice CreateLogicalDevice(MemoryArena& a_temp_arena, const VkPhysicalDevice a_phys_device, const VulkanQueuesIndices& a_queue_indices, const BB::Slice<const char*>& a_device_extensions)
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

	VkDeviceQueueCreateInfo* queue_create_infos = ArenaAllocArr(a_temp_arena, VkDeviceQueueCreateInfo, 3);
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

		enum_conv.image_aspects[static_cast<uint32_t>(IMAGE_ASPECT::COLOR)] = VK_IMAGE_ASPECT_COLOR_BIT;
		enum_conv.image_aspects[static_cast<uint32_t>(IMAGE_ASPECT::DEPTH)] = VK_IMAGE_ASPECT_DEPTH_BIT;
		enum_conv.image_aspects[static_cast<uint32_t>(IMAGE_ASPECT::DEPTH_STENCIL)] = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

		// 64 bit images
		enum_conv.image_formats[static_cast<uint32_t>(IMAGE_FORMAT::RGBA16_UNORM)] = VK_FORMAT_R16G16B16A16_UNORM;
		enum_conv.image_formats[static_cast<uint32_t>(IMAGE_FORMAT::RGBA16_SFLOAT)] = VK_FORMAT_R16G16B16A16_SFLOAT;
		// 32 bit images
		enum_conv.image_formats[static_cast<uint32_t>(IMAGE_FORMAT::RGBA8_SRGB)] = VK_FORMAT_R8G8B8A8_SRGB;
		enum_conv.image_formats[static_cast<uint32_t>(IMAGE_FORMAT::RGBA8_UNORM)] = VK_FORMAT_R8G8B8A8_UNORM;
		// 24 bit images
		enum_conv.image_formats[static_cast<uint32_t>(IMAGE_FORMAT::RGB8_SRGB)] = VK_FORMAT_R8G8B8_SRGB;
		// 8 bit images
		enum_conv.image_formats[static_cast<uint32_t>(IMAGE_FORMAT::A8_UNORM)] = VK_FORMAT_R8_UNORM;
		// depth images
		enum_conv.image_formats[static_cast<uint32_t>(IMAGE_FORMAT::D16_UNORM)] = VK_FORMAT_D16_UNORM;
		enum_conv.image_formats[static_cast<uint32_t>(IMAGE_FORMAT::D32_SFLOAT)] = VK_FORMAT_D32_SFLOAT;
		enum_conv.image_formats[static_cast<uint32_t>(IMAGE_FORMAT::D32_SFLOAT_S8_UINT)] = VK_FORMAT_D32_SFLOAT_S8_UINT;
		enum_conv.image_formats[static_cast<uint32_t>(IMAGE_FORMAT::D24_UNORM_S8_UINT)] = VK_FORMAT_D24_UNORM_S8_UINT;

		enum_conv.image_types[static_cast<uint32_t>(IMAGE_TYPE::TYPE_1D)] = VK_IMAGE_TYPE_1D;
		enum_conv.image_types[static_cast<uint32_t>(IMAGE_TYPE::TYPE_2D)] = VK_IMAGE_TYPE_2D;
		enum_conv.image_types[static_cast<uint32_t>(IMAGE_TYPE::TYPE_3D)] = VK_IMAGE_TYPE_3D;

		enum_conv.image_view_types[static_cast<uint32_t>(IMAGE_VIEW_TYPE::TYPE_1D)] = VK_IMAGE_VIEW_TYPE_1D;
		enum_conv.image_view_types[static_cast<uint32_t>(IMAGE_VIEW_TYPE::TYPE_2D)] = VK_IMAGE_VIEW_TYPE_2D;
		enum_conv.image_view_types[static_cast<uint32_t>(IMAGE_VIEW_TYPE::TYPE_3D)] = VK_IMAGE_VIEW_TYPE_3D;
		enum_conv.image_view_types[static_cast<uint32_t>(IMAGE_VIEW_TYPE::TYPE_1D_ARRAY)] = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
		enum_conv.image_view_types[static_cast<uint32_t>(IMAGE_VIEW_TYPE::TYPE_2D_ARRAY)] = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		enum_conv.image_view_types[static_cast<uint32_t>(IMAGE_VIEW_TYPE::CUBE)] = VK_IMAGE_VIEW_TYPE_CUBE;

		enum_conv.sampler_address_modes[static_cast<uint32_t>(SAMPLER_ADDRESS_MODE::REPEAT)] = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		enum_conv.sampler_address_modes[static_cast<uint32_t>(SAMPLER_ADDRESS_MODE::MIRROR)] = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		enum_conv.sampler_address_modes[static_cast<uint32_t>(SAMPLER_ADDRESS_MODE::BORDER)] = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		enum_conv.sampler_address_modes[static_cast<uint32_t>(SAMPLER_ADDRESS_MODE::CLAMP)] = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

		enum_conv.border_colors[static_cast<uint32_t>(SAMPLER_BORDER_COLOR::COLOR_FLOAT_TRANSPARENT_BLACK)] = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
		enum_conv.border_colors[static_cast<uint32_t>(SAMPLER_BORDER_COLOR::COLOR_INT_TRANSPARENT_BLACK)] = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
		enum_conv.border_colors[static_cast<uint32_t>(SAMPLER_BORDER_COLOR::COLOR_FLOAT_OPAQUE_BLACK)] = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
		enum_conv.border_colors[static_cast<uint32_t>(SAMPLER_BORDER_COLOR::COLOR_INT_OPAQUE_BLACK)] = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		enum_conv.border_colors[static_cast<uint32_t>(SAMPLER_BORDER_COLOR::COLOR_FLOAT_OPAQUE_WHITE)] = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		enum_conv.border_colors[static_cast<uint32_t>(SAMPLER_BORDER_COLOR::COLOR_INT_OPAQUE_WHITE)] = VK_BORDER_COLOR_INT_OPAQUE_WHITE;

		enum_conv.image_usages[static_cast<uint32_t>(IMAGE_USAGE::DEPTH)] = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT BB_EXTENDED_IMAGE_USAGE_FLAGS;
		enum_conv.image_usages[static_cast<uint32_t>(IMAGE_USAGE::SHADOW_MAP)] = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT BB_EXTENDED_IMAGE_USAGE_FLAGS;
		enum_conv.image_usages[static_cast<uint32_t>(IMAGE_USAGE::TEXTURE)] = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT BB_EXTENDED_IMAGE_USAGE_FLAGS;
		enum_conv.image_usages[static_cast<uint32_t>(IMAGE_USAGE::SWAPCHAIN_COPY_IMG)] = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT BB_EXTENDED_IMAGE_USAGE_FLAGS;
		enum_conv.image_usages[static_cast<uint32_t>(IMAGE_USAGE::RENDER_TARGET)] = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT BB_EXTENDED_IMAGE_USAGE_FLAGS;
		enum_conv.image_usages[static_cast<uint32_t>(IMAGE_USAGE::COPY_SRC_DST)] =  VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		enum_conv.cull_modes[static_cast<uint32_t>(CULL_MODE::NONE)] = VK_CULL_MODE_NONE;
		enum_conv.cull_modes[static_cast<uint32_t>(CULL_MODE::FRONT)] = VK_CULL_MODE_FRONT_BIT;
		enum_conv.cull_modes[static_cast<uint32_t>(CULL_MODE::BACK)] = VK_CULL_MODE_BACK_BIT;
		enum_conv.cull_modes[static_cast<uint32_t>(CULL_MODE::FRONT_AND_BACK)] = VK_CULL_MODE_FRONT_AND_BACK;

		enum_conv.blend_op[static_cast<uint32_t>(BLEND_OP::ADD)] = VK_BLEND_OP_ADD;
		enum_conv.blend_op[static_cast<uint32_t>(BLEND_OP::SUBTRACT)] = VK_BLEND_OP_SUBTRACT;

		enum_conv.blend_factor[static_cast<uint32_t>(BLEND_MODE::FACTOR_ZERO)] = VK_BLEND_FACTOR_ZERO;
		enum_conv.blend_factor[static_cast<uint32_t>(BLEND_MODE::FACTOR_ONE)] = VK_BLEND_FACTOR_ONE;
		enum_conv.blend_factor[static_cast<uint32_t>(BLEND_MODE::FACTOR_SRC_ALPHA)] = VK_BLEND_FACTOR_SRC_ALPHA;
		enum_conv.blend_factor[static_cast<uint32_t>(BLEND_MODE::FACTOR_ONE_MINUS_SRC_ALPHA)] = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		enum_conv.blend_factor[static_cast<uint32_t>(BLEND_MODE::FACTOR_DST_ALPHA)] = VK_BLEND_FACTOR_DST_ALPHA;
#endif //ENUM_CONVERSATION_BY_ARRAY
	}

	VkInstance instance;
	VkDebugUtilsMessengerEXT debug_msgr;
	VkPhysicalDevice phys_device;
	VkDevice device;
	VkQueue present_queue;
	VmaAllocator vma;

	//jank pointer
	VulkanDescriptorLinearBuffer* pdescriptor_buffer;

	//takes a VkHandle
	StaticOL_HashMap<uintptr_t, VmaAllocation> allocation_map;
	StaticOL_HashMap<uintptr_t, VkPipelineLayout> pipeline_layout_cache;
	
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
		VkImageAspectFlags image_aspects[static_cast<uint32_t>(IMAGE_ASPECT::ENUM_SIZE)];
		VkFormat image_formats[static_cast<uint32_t>(IMAGE_FORMAT::ENUM_SIZE)];
		VkImageType image_types[static_cast<uint32_t>(IMAGE_TYPE::ENUM_SIZE)];
		VkImageViewType image_view_types[static_cast<uint32_t>(IMAGE_VIEW_TYPE::ENUM_SIZE)];
		VkSamplerAddressMode sampler_address_modes[static_cast<uint32_t>(SAMPLER_ADDRESS_MODE::ENUM_SIZE)];
		VkBorderColor border_colors[static_cast<uint32_t>(SAMPLER_BORDER_COLOR::ENUM_SIZE)];
		VkImageUsageFlags image_usages[static_cast<uint32_t>(IMAGE_USAGE::ENUM_SIZE)];
		VkCullModeFlags cull_modes[static_cast<uint32_t>(CULL_MODE::ENUM_SIZE)];
		VkBlendOp blend_op[static_cast<uint32_t>(BLEND_OP::ENUM_SIZE)];
		VkBlendFactor blend_factor[static_cast<uint32_t>(BLEND_MODE::ENUM_SIZE)];
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

		PFN_vkCreateAccelerationStructureKHR CreateAccelerationStructureKHR;
		PFN_vkDestroyAccelerationStructureKHR DestroyAccelerationStructureKHR;
		PFN_vkGetAccelerationStructureBuildSizesKHR GetAccelerationStructureBuildSizesKHR;
		PFN_vkGetAccelerationStructureDeviceAddressKHR GetAccelerationStructureDeviceAddressKHR;
		PFN_vkCmdBuildAccelerationStructuresKHR CmdBuildAccelerationStructuresKHR;
		PFN_vkBuildAccelerationStructuresKHR BuildAccelerationStructuresKHR;
		PFN_vkCmdTraceRaysKHR CmdTraceRaysKHR;
		PFN_vkGetRayTracingShaderGroupHandlesKHR GetRayTracingShaderGroupHandlesKHR;
		PFN_vkCreateRayTracingPipelinesKHR CreateRayTracingPipelinesKHR;
	} pfn;

	bool use_debug;
	bool use_raytracing;
};

struct Vulkan_swapchain
{
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;

	struct swapchain_frame
	{
		VkImage image;
		VkImageView image_view;
		VkSemaphore image_available_semaphore;
		VkSemaphore present_finished_semaphore;
	};
	uint32_t frame_count;
	swapchain_frame* frames;

	//used for recreation
	VkPresentModeKHR optimal_present;
	VkSurfaceFormatKHR optimal_surface_format;
};

static Vulkan_inst* s_vulkan_inst = nullptr;
static Vulkan_swapchain* s_vulkan_swapchain = nullptr;

static inline VkDeviceSize GetAccelerationStructureAddress(const VkDevice a_device, const VkAccelerationStructureKHR a_acc_struct)
{
	VkAccelerationStructureDeviceAddressInfoKHR info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR, nullptr, a_acc_struct };
	return s_vulkan_inst->pfn.GetAccelerationStructureDeviceAddressKHR(a_device, &info);
}

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

static inline VkImageAspectFlags ImageAspect(const IMAGE_ASPECT a_aspects)
{
#ifdef ENUM_CONVERSATION_BY_ARRAY
	return s_vulkan_inst->enum_conv.image_aspects[static_cast<uint32_t>(a_aspects)];
#else
	switch (a_aspects)
	{
	case IMAGE_ASPECT::COLOR:         return VK_IMAGE_ASPECT_COLOR_BIT;
	case IMAGE_ASPECT::DEPTH:         return VK_IMAGE_ASPECT_DEPTH_BIT;
	case IMAGE_ASPECT::DEPTH_STENCIL: return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	default:
		BB_ASSERT(false, "Vulkan: IMAGE_ASPECT failed to convert to a VkImageAspectFlags.");
		return 0;
		break;
	}
#endif // ENUM_CONVERSATION_BY_ARRAY
}

static inline VkImageLayout ImageLayout(const IMAGE_LAYOUT a_image_layout)
{
	switch (a_image_layout)
	{
	case IMAGE_LAYOUT::NONE:        return VK_IMAGE_LAYOUT_UNDEFINED;
	case IMAGE_LAYOUT::RO_GEOMETRY: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	case IMAGE_LAYOUT::RO_FRAGMENT: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	case IMAGE_LAYOUT::RO_COMPUTE:  return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	case IMAGE_LAYOUT::RW_GEOMETRY: return VK_IMAGE_LAYOUT_GENERAL;
	case IMAGE_LAYOUT::RW_FRAGMENT: return VK_IMAGE_LAYOUT_GENERAL;
	case IMAGE_LAYOUT::RW_COMPUTE:  return VK_IMAGE_LAYOUT_GENERAL;
	case IMAGE_LAYOUT::RO_DEPTH:    return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	case IMAGE_LAYOUT::RT_DEPTH:    return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	case IMAGE_LAYOUT::RT_COLOR:    return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	case IMAGE_LAYOUT::COPY_SRC:    return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	case IMAGE_LAYOUT::COPY_DST:    return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	case IMAGE_LAYOUT::PRESENT:     return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	default:
		BB_ASSERT(false, "Vulkan: IMAGE_LAYOUT failed to convert to a VkImageLayout.");
		return VK_IMAGE_LAYOUT_UNDEFINED;
	}
}

static inline VkFormat ImageFormats(const IMAGE_FORMAT a_image_format)
{
#ifdef ENUM_CONVERSATION_BY_ARRAY
	return s_vulkan_inst->enum_conv.image_formats[static_cast<uint32_t>(a_image_format)];
#else
	switch (a_image_format)
	{
	case IMAGE_FORMAT::RGBA16_UNORM:	return VK_FORMAT_R16G16B16A16_UNORM;
	case IMAGE_FORMAT::RGBA16_SFLOAT:	return VK_FORMAT_R16G16B16A16_SFLOAT;
	case IMAGE_FORMAT::RGBA8_SRGB:		return VK_FORMAT_R8G8B8A8_SRGB;
	case IMAGE_FORMAT::RGBA8_UNORM:		return VK_FORMAT_R8G8B8A8_UNORM;
	case IMAGE_FORMAT::RGB8_SRGB:		return VK_FORMAT_R8G8B8_SRGB;
	case IMAGE_FORMAT::A8_UNORM:		return VK_FORMAT_R8_UNORM;
	case IMAGE_FORMAT::D16_UNORM:				return VK_FORMAT_D16_UNORM;
	case IMAGE_FORMAT::D32_SFLOAT:				return VK_FORMAT_D32_SFLOAT;
	case IMAGE_FORMAT::D32_SFLOAT_S8_UINT:		return VK_FORMAT_D32_SFLOAT_S8_UINT;
	case IMAGE_FORMAT::D24_UNORM_S8_UINT:		return VK_FORMAT_D24_UNORM_S8_UINT;
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

static inline VkImageViewType ImageViewTypes(const IMAGE_VIEW_TYPE a_image_types)
{
#ifdef ENUM_CONVERSATION_BY_ARRAY
	return s_vulkan_inst->enum_conv.image_view_types[static_cast<uint32_t>(a_image_types)];
#else
	switch (a_image_types)
	{
	case IMAGE_TYPE::TYPE_1D:		return VK_IMAGE_VIEW_TYPE_1D;
	case IMAGE_TYPE::TYPE_2D:		return VK_IMAGE_VIEW_TYPE_2D;
	case IMAGE_TYPE::TYPE_3D:		return VK_IMAGE_VIEW_TYPE_3D;
	case IMAGE_TYPE::CUBE:			return VK_IMAGE_VIEW_TYPE_CUBE;
	default:
		BB_ASSERT(false, "Vulkan: IMAGE_TYPE failed to convert to a VkImageViewType.");
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

static inline VkBorderColor SamplerBorderColor(const SAMPLER_BORDER_COLOR a_color)
{
#ifdef ENUM_CONVERSATION_BY_ARRAY
	return s_vulkan_inst->enum_conv.border_colors[static_cast<uint32_t>(a_color)];
#else
	switch (a_address_mode)
	{
	case SAMPLER_BORDER_COLOR::COLOR_FLOAT_TRANSPARENT_BLACK:	return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	case SAMPLER_BORDER_COLOR::COLOR_INT_TRANSPARENT_BLACK:		return VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
	case SAMPLER_BORDER_COLOR::COLOR_FLOAT_OPAQUE_BLACK:		return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
	case SAMPLER_BORDER_COLOR::COLOR_INT_OPAQUE_BLACK:			return VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	case SAMPLER_BORDER_COLOR::COLOR_FLOAT_OPAQUE_WHITE:		return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	case SAMPLER_BORDER_COLOR::COLOR_INT_OPAQUE_WHITE:			return VK_BORDER_COLOR_INT_OPAQUE_WHITE;
	default:
		BB_ASSERT(false, "Vulkan: SAMPLER_BORDER_COLOR failed to convert to a VkBorderColor.");
		return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
		break;
	}
#endif //ENUM_CONVERSATION_BY_ARRAY
}

static inline VkImageUsageFlags ImageUsage(const IMAGE_USAGE a_usage)
{
#ifdef ENUM_CONVERSATION_BY_ARRAY
	return s_vulkan_inst->enum_conv.image_usages[static_cast<uint32_t>(a_usage)];
#else
	switch (a_usage)
	{
	case IMAGE_USAGE::DEPTH:			return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT; 
	case IMAGE_USAGE::TEXTURE:			return VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	case IMAGE_USAGE::UPLOAD_SRC_DST	return VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	case IMAGE_USAGE::RENDER_TARGET:	return VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	case IMAGE_USAGE::COPY_SRC_DST:		return VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	default:
		BB_ASSERT(false, "Vulkan: IMAGE_USAGE failed to convert to a VkImageUsageFlags.");
		return VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		break;
	}
#endif //ENUM_CONVERSATION_BY_ARRAY
}

static inline VkBlendOp BlendOp(const BLEND_OP a_blend_op)
{
#ifdef ENUM_CONVERSATION_BY_ARRAY
	return s_vulkan_inst->enum_conv.blend_op[static_cast<uint32_t>(a_blend_op)];
#else
	BB_UNIMPLEMENTED();
#endif //ENUM_CONVERSATION_BY_ARRAY
}

static inline VkBlendFactor BlendFactor(const BLEND_MODE a_blend_mode)
{
#ifdef ENUM_CONVERSATION_BY_ARRAY
	return s_vulkan_inst->enum_conv.blend_factor[static_cast<uint32_t>(a_blend_mode)];
#else
	BB_UNIMPLEMENTED();
#endif //ENUM_CONVERSATION_BY_ARRAY
}

// temp to figure out 
static void AddMemoryToAllocationMap(const uintptr_t a_address, const VmaAllocation a_vma_allocation)
{
	static BBRWLock lock = BBRWLock(0);
	BBRWLockScopeWrite slock(lock);
	s_vulkan_inst->allocation_map.insert(a_address, a_vma_allocation);
}

static VkSampler CreateSampler(const SamplerCreateInfo& a_create_info)
{
	VkSamplerCreateInfo sampler_info{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	sampler_info.addressModeU = SamplerAddressModes(a_create_info.mode_u);
	sampler_info.addressModeV = SamplerAddressModes(a_create_info.mode_v);
	sampler_info.addressModeW = SamplerAddressModes(a_create_info.mode_w);
	switch (a_create_info.filter)
	{
	case SAMPLER_FILTER::NEAREST:
		sampler_info.magFilter = VK_FILTER_NEAREST;
		sampler_info.minFilter = VK_FILTER_NEAREST;
		break;
	case SAMPLER_FILTER::LINEAR:
		sampler_info.magFilter = VK_FILTER_LINEAR;
		sampler_info.minFilter = VK_FILTER_LINEAR;
		break;
	default:
		BB_ASSERT(false, "default hit while it shouldn't");
		break;
	}
	sampler_info.minLod = a_create_info.min_lod;
	sampler_info.maxLod = a_create_info.max_lod;
	sampler_info.mipLodBias = 0;
	if (a_create_info.max_anistoropy > 0)
	{
		sampler_info.anisotropyEnable = VK_TRUE;
		sampler_info.maxAnisotropy = a_create_info.max_anistoropy;
	}
	sampler_info.borderColor = SamplerBorderColor(a_create_info.border_color);
	VkSampler sampler;
	VKASSERT(vkCreateSampler(s_vulkan_inst->device, &sampler_info, nullptr, &sampler),
		"Vulkan: Failed to create image sampler!");
	SetDebugName(a_create_info.name, sampler, VK_OBJECT_TYPE_SAMPLER);

	return sampler;
}

static inline VkDescriptorAddressInfoEXT GetDescriptorAddressInfo(const VkDevice a_device, const GPUBufferView& a_buffer, const VkFormat a_format = VK_FORMAT_UNDEFINED)
{
	VkDescriptorAddressInfoEXT info{ VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT };
	info.range = a_buffer.size;
	info.address = GetBufferDeviceAddress(a_device, reinterpret_cast<VkBuffer>(a_buffer.buffer.handle));
	//offset the address.
	info.address += a_buffer.offset;
	info.format = a_format;
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
	allocation.size = static_cast<uint32_t>(descriptors_size);
	allocation.offset = m_offset;
	allocation.buffer_start = m_start; //Maybe just get this from the descriptor heap? We only have one heap anyway.

	m_offset += allocation.size;

	return allocation;
}

bool Vulkan::InitializeVulkan(MemoryArena& a_arena, const RendererCreateInfo a_create_info)
{
	BB_ASSERT(s_vulkan_inst == nullptr, "trying to initialize vulkan while it's already initialized");
	s_vulkan_inst = ArenaAllocType(a_arena, Vulkan_inst) {};
	s_vulkan_inst->use_debug = a_create_info.debug;
	s_vulkan_inst->use_raytracing = a_create_info.use_raytracing;

	//just enable then all, no fallback layer for now lol.
	const char* instance_extensions[] = {
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
		VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME
	};

	constexpr const char* device_extensions[] = {
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

	constexpr const char* device_raytracing[] = {
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
		VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME
	};

	const char* chosen_extensions[_countof(device_extensions) + _countof(device_raytracing)]{};
	uint32_t extension_count = 0;
	for (size_t i = 0; i < _countof(device_extensions); i++)
	{
		chosen_extensions[i] = device_extensions[i];
		++extension_count;
	}

	if (a_create_info.use_raytracing)
	{
		chosen_extensions[_countof(device_extensions)] = device_raytracing[0];
		chosen_extensions[_countof(device_extensions) + 1] = device_raytracing[1];
		chosen_extensions[_countof(device_extensions) + 2] = device_raytracing[2];
		extension_count += 3;
	}

	MemoryArenaScope(a_arena)
	{
		//Check if the extensions and layers work.
		BB_ASSERT(CheckExtensionSupport(a_arena,
			Slice(instance_extensions, _countof(instance_extensions))),
			"Vulkan: extension(s) not supported.");

		BB_ASSERT(CheckExtensionSupport(a_arena,
			Slice(chosen_extensions, extension_count)),
			"Vulkan: extension(s) not supported.");

		VkApplicationInfo app_info{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
		app_info.pApplicationName = a_create_info.app_name;
		app_info.pEngineName = a_create_info.engine_name;
		app_info.applicationVersion = VK_MAKE_API_VERSION(0, 1, VULKAN_VERSION, 0);
		app_info.engineVersion = VK_MAKE_API_VERSION(0, 1, VULKAN_VERSION, 0);
		app_info.apiVersion = VK_MAKE_API_VERSION(0, 1, VULKAN_VERSION, 0);

		VkInstanceCreateInfo instance_create_info{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
		instance_create_info.pApplicationInfo = &app_info;
		VkDebugUtilsMessengerCreateInfoEXT debug_create_info;
		const char* validation_layer = "VK_LAYER_KHRONOS_validation";
		if (a_create_info.debug)
		{
			BB_WARNING(CheckValidationLayerSupport(a_arena, Slice(&validation_layer, 1)), "Vulkan: Validation layer(s) not available.", WarningType::MEDIUM);
			debug_create_info = CreateDebugCallbackCreateInfo();
			instance_create_info.ppEnabledLayerNames = &validation_layer;
			instance_create_info.enabledLayerCount = 1;
			instance_create_info.pNext = reinterpret_cast<VkDebugUtilsMessengerCreateInfoEXT*>(&debug_create_info);
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

		// create debug messenger
		if (a_create_info.debug)
		{
			PFN_vkCreateDebugUtilsMessengerEXT debug_msgr_create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(s_vulkan_inst->instance, "vkCreateDebugUtilsMessengerEXT"));
			if (debug_msgr_create != nullptr)
				VKASSERT(debug_msgr_create(s_vulkan_inst->instance, &debug_create_info, nullptr, &s_vulkan_inst->debug_msgr), "failed to create debug messenger");
			else
				BB_ASSERT(false, "failed to create debug messenger");
		}
		
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

		if (s_vulkan_inst->use_raytracing)
		{ 
			s_vulkan_inst->pfn.CreateAccelerationStructureKHR = VkGetFuncPtr(s_vulkan_inst->instance, vkCreateAccelerationStructureKHR);
			s_vulkan_inst->pfn.DestroyAccelerationStructureKHR = VkGetFuncPtr(s_vulkan_inst->instance, vkDestroyAccelerationStructureKHR);
			s_vulkan_inst->pfn.GetAccelerationStructureBuildSizesKHR = VkGetFuncPtr(s_vulkan_inst->instance, vkGetAccelerationStructureBuildSizesKHR);
			s_vulkan_inst->pfn.GetAccelerationStructureDeviceAddressKHR = VkGetFuncPtr(s_vulkan_inst->instance, vkGetAccelerationStructureDeviceAddressKHR);
			s_vulkan_inst->pfn.CmdBuildAccelerationStructuresKHR = VkGetFuncPtr(s_vulkan_inst->instance, vkCmdBuildAccelerationStructuresKHR);
			s_vulkan_inst->pfn.BuildAccelerationStructuresKHR = VkGetFuncPtr(s_vulkan_inst->instance, vkBuildAccelerationStructuresKHR);
			s_vulkan_inst->pfn.CmdTraceRaysKHR = VkGetFuncPtr(s_vulkan_inst->instance, vkCmdTraceRaysKHR);
			s_vulkan_inst->pfn.GetRayTracingShaderGroupHandlesKHR = VkGetFuncPtr(s_vulkan_inst->instance, vkGetRayTracingShaderGroupHandlesKHR);
			s_vulkan_inst->pfn.CreateRayTracingPipelinesKHR = VkGetFuncPtr(s_vulkan_inst->instance, vkCreateRayTracingPipelinesKHR);
		}

		{	//device & queues
			s_vulkan_inst->phys_device = FindPhysicalDevice(a_arena, s_vulkan_inst->instance);
			//do some queue stuff.....

			s_vulkan_inst->queue_indices = GetQueueIndices(a_arena, s_vulkan_inst->phys_device);

			s_vulkan_inst->device = CreateLogicalDevice(a_arena,
				s_vulkan_inst->phys_device, 
				s_vulkan_inst->queue_indices,
				Slice(chosen_extensions, extension_count));
		}

		{	//descriptor info & general device properties
			VkPhysicalDeviceTimelineSemaphoreProperties timeline_properties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES };
			VkPhysicalDeviceDescriptorBufferPropertiesEXT desc_info{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT };
			desc_info.pNext = &timeline_properties;
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

	s_vulkan_inst->allocation_map.Init(a_arena, 1024);
	s_vulkan_inst->pdescriptor_buffer = ArenaAllocType(a_arena, VulkanDescriptorLinearBuffer)(
		mbSize * 4,
		VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

	s_vulkan_inst->pipeline_layout_cache.Init(a_arena, 64);

	//Get the present queue.
	vkGetDeviceQueue(s_vulkan_inst->device,
		s_vulkan_inst->queue_indices.present,
		0,
		&s_vulkan_inst->present_queue);

	return true;
}

GPUDeviceInfo Vulkan::GetGPUDeviceInfo(MemoryArena& a_arena)
{
	VkPhysicalDeviceProperties2 properties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
	vkGetPhysicalDeviceProperties2(s_vulkan_inst->phys_device, &properties);
	VkPhysicalDeviceMemoryProperties memory_prop{};
	vkGetPhysicalDeviceMemoryProperties(s_vulkan_inst->phys_device, &memory_prop);

	GPUDeviceInfo device;
	const size_t device_name_size = strnlen(properties.properties.deviceName, VK_MAX_PHYSICAL_DEVICE_NAME_SIZE);
	device.name = ArenaAllocArr(a_arena, char, device_name_size + 1);
	strncpy_s(device.name, device_name_size + 1, properties.properties.deviceName, device_name_size);

	// get the memory heaps
	device.memory_heaps.Init(a_arena, memory_prop.memoryHeapCount);

	for (uint32_t i = 0; i < static_cast<uint32_t>(device.memory_heaps.capacity()); i++)
	{
		GPUDeviceInfo::MemoryHeapInfo heap_info;
		heap_info.heap_num = i;
		heap_info.heap_size = memory_prop.memoryHeaps[i].size;
		heap_info.heap_device_local = memory_prop.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
		device.memory_heaps.emplace_back(heap_info);
	}

	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(s_vulkan_inst->phys_device, &queue_family_count, nullptr);
	device.queue_families.Init(a_arena, queue_family_count);

	MemoryArenaScope(a_arena)
	{
		VkQueueFamilyProperties* const queue_families = ArenaAllocArr(a_arena, VkQueueFamilyProperties, queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(s_vulkan_inst->phys_device, &queue_family_count, queue_families);

		for (uint32_t i = 0; i < queue_family_count; i++)
		{
			GPUDeviceInfo::QueueFamily queue_family;
			queue_family.queue_family_index = i;
			queue_family.queue_count = queue_families[i].queueCount;
			queue_family.support_compute = queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT;
			queue_family.support_graphics = queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
			queue_family.support_transfer = queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT;
			device.queue_families.emplace_back(queue_family);
		}
	}

	return device;
}

static void CreateVkSwapchain(const uint32_t a_width, const uint32_t a_height, uint32_t& a_backbuffer_count)
{
	VkSurfaceCapabilitiesKHR capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s_vulkan_inst->phys_device, s_vulkan_swapchain->surface, &capabilities);

	const VkExtent2D swapchain_extent
	{
		Clamp(a_width,
			capabilities.minImageExtent.width,
			capabilities.maxImageExtent.width),
		Clamp(a_height,
			capabilities.minImageExtent.height,
			capabilities.maxImageExtent.height)
	};

	VkSwapchainCreateInfoKHR swapchain_create_info{};
	swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchain_create_info.surface = s_vulkan_swapchain->surface;
	swapchain_create_info.imageFormat = s_vulkan_swapchain->optimal_surface_format.format;
	swapchain_create_info.imageColorSpace = s_vulkan_swapchain->optimal_surface_format.colorSpace;
	swapchain_create_info.imageExtent = swapchain_extent;
	swapchain_create_info.imageArrayLayers = 1;
	swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	swapchain_create_info.preTransform = capabilities.currentTransform;
	swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchain_create_info.presentMode = s_vulkan_swapchain->optimal_present;
	swapchain_create_info.clipped = VK_TRUE;
	swapchain_create_info.oldSwapchain = s_vulkan_swapchain->swapchain;


	const uint32_t graphics_family = s_vulkan_inst->queue_indices.graphics;
	const uint32_t present_family = s_vulkan_inst->queue_indices.present;
	const uint32_t concurrent_family_indices[]{ graphics_family, present_family };
	if (graphics_family != present_family)
	{
		swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchain_create_info.queueFamilyIndexCount = 2;
		swapchain_create_info.pQueueFamilyIndices = concurrent_family_indices;
	}
	else
	{
		swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchain_create_info.queueFamilyIndexCount = 0;
		swapchain_create_info.pQueueFamilyIndices = nullptr;
	}

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

static void GetSwapchainImages()
{
	VkImage* swapchain_images = BBstackAlloc(s_vulkan_swapchain->frame_count, VkImage);
	vkGetSwapchainImagesKHR(s_vulkan_inst->device,
		s_vulkan_swapchain->swapchain,
		&s_vulkan_swapchain->frame_count,
		swapchain_images);

	VkImageViewCreateInfo image_view_create_info{};
	image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	image_view_create_info.format = s_vulkan_swapchain->optimal_surface_format.format;
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

	const VkSemaphoreCreateInfo sem_info{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

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

bool Vulkan::CreateSwapchain(MemoryArena& a_arena, const WindowHandle a_window_handle, const uint32_t a_width, const uint32_t a_height, uint32_t& a_backbuffer_count)
{
	BB_ASSERT(s_vulkan_inst != nullptr, "trying to create a swapchain while vulkan is not initialized");
	BB_ASSERT(s_vulkan_swapchain == nullptr, "trying to create a swapchain while one exists");
	s_vulkan_swapchain = ArenaAllocType(a_arena, Vulkan_swapchain) {};
	
	MemoryArenaScope(a_arena)
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
			"Failed to create in32 vulkan surface");

		VkSurfaceFormatKHR* formats;
		uint32_t format_count;
		vkGetPhysicalDeviceSurfaceFormatsKHR(s_vulkan_inst->phys_device, s_vulkan_swapchain->surface, &format_count, nullptr);
		formats = ArenaAllocArr(a_arena, VkSurfaceFormatKHR, format_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(s_vulkan_inst->phys_device, s_vulkan_swapchain->surface, &format_count, formats);

		VkPresentModeKHR* present_modes;
		uint32_t present_mode_count;
		vkGetPhysicalDeviceSurfacePresentModesKHR(s_vulkan_inst->phys_device, s_vulkan_swapchain->surface, &present_mode_count, nullptr);
		present_modes = ArenaAllocArr(a_arena, VkPresentModeKHR, present_mode_count);
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

		s_vulkan_swapchain->optimal_present = optimal_present;
		s_vulkan_swapchain->optimal_surface_format = optimal_surface_format;

		CreateVkSwapchain(a_width, a_height, a_backbuffer_count);
	}

	s_vulkan_swapchain->frames = ArenaAllocArr(a_arena, Vulkan_swapchain::swapchain_frame, s_vulkan_swapchain->frame_count);

	GetSwapchainImages();

	return true;
}

bool Vulkan::RecreateSwapchain(const uint32_t a_width, const uint32_t a_height)
{
	for (uint32_t i = 0; i < s_vulkan_swapchain->frame_count; i++)
	{
		vkDestroyImageView(s_vulkan_inst->device, s_vulkan_swapchain->frames[i].image_view, nullptr);
		vkDestroySemaphore(s_vulkan_inst->device, s_vulkan_swapchain->frames[i].image_available_semaphore, nullptr);
		vkDestroySemaphore(s_vulkan_inst->device, s_vulkan_swapchain->frames[i].present_finished_semaphore, nullptr);
	}
	const uint32_t current_back_buffer_count = s_vulkan_swapchain->frame_count;
	uint32_t new_back_buffer_count = s_vulkan_swapchain->frame_count;
	CreateVkSwapchain(a_width, a_height, new_back_buffer_count);
	BB_ASSERT(new_back_buffer_count == current_back_buffer_count, "back buffer amount should not change during resize");

	GetSwapchainImages();
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
	VkBufferCreateInfo buffer_info{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	buffer_info.size = a_create_info.size;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo vma_alloc{};

	switch (a_create_info.type)
	{
	case BUFFER_TYPE::UPLOAD:
		buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		break;
	case BUFFER_TYPE::READBACK:
		buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
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
	case BUFFER_TYPE::RT_ACCELERATION:
		buffer_info.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		break;
	case BUFFER_TYPE::RT_BUILD_ACCELERATION:
		buffer_info.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		break;
	default:
		BB_ASSERT(false, "unknown buffer type");
		break;
	}

	if (a_create_info.host_writable)
	{
		vma_alloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
		vma_alloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	}
	else
		vma_alloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	
	VkBuffer buffer;
	VmaAllocation allocation;
	VKASSERT(vmaCreateBuffer(s_vulkan_inst->vma,
		&buffer_info, &vma_alloc,
		&buffer, &allocation,
		nullptr), "Vulkan::VMA, Failed to allocate memory");

	SetDebugName(a_create_info.name, buffer, VK_OBJECT_TYPE_BUFFER);

	AddMemoryToAllocationMap(reinterpret_cast<uintptr_t>(buffer), allocation);
	return GPUBuffer(reinterpret_cast<uintptr_t>(buffer));
}

void Vulkan::FreeBuffer(const GPUBuffer a_buffer)
{
	const VmaAllocation allocation = *s_vulkan_inst->allocation_map.find(a_buffer.handle);
	vmaDestroyBuffer(s_vulkan_inst->vma, 
		reinterpret_cast<VkBuffer>(a_buffer.handle),
		allocation);

	s_vulkan_inst->allocation_map.erase(a_buffer.handle);
}

GPUAddress Vulkan::GetBufferAddress(const GPUBuffer a_buffer)
{
	return GetBufferDeviceAddress(s_vulkan_inst->device, reinterpret_cast<VkBuffer>(a_buffer.handle));
}

static ConstSlice<VkAccelerationStructureGeometryKHR> CreateGeometryInfo(MemoryArena& a_arena, const ConstSlice<AccelerationStructGeometrySize> a_geometry_sizes, const GPUAddress a_vertex_device_address, const GPUAddress a_index_device_address)
{
	VkAccelerationStructureGeometryKHR* geometry_infos = ArenaAllocArr(a_arena, VkAccelerationStructureGeometryKHR, a_geometry_sizes.size());

	for (size_t i = 0; i < a_geometry_sizes.size(); i++)
	{
		const AccelerationStructGeometrySize& size_info = a_geometry_sizes[i];
		VkAccelerationStructureGeometryKHR& geo = geometry_infos[i];
		geo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		geo.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		geo.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		geo.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
		geo.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		geo.geometry.triangles.vertexData.deviceAddress = a_vertex_device_address + size_info.vertex_offset;
		geo.geometry.triangles.maxVertex = size_info.vertex_count;
		geo.geometry.triangles.vertexStride = size_info.vertex_stride;
		geo.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
		geo.geometry.triangles.indexData.deviceAddress = a_index_device_address + size_info.index_offset;
		geo.geometry.triangles.transformData.deviceAddress = size_info.transform_address;
	}

	return ConstSlice<VkAccelerationStructureGeometryKHR>(geometry_infos, a_geometry_sizes.size());
}

AccelerationStructSizeInfo Vulkan::GetBottomLevelAccelerationStructSizeInfo(MemoryArena& a_temp_arena, const ConstSlice<AccelerationStructGeometrySize> a_geometry_sizes, const ConstSlice<uint32_t> a_primitive_counts, const GPUAddress a_vertex_device_address, const GPUAddress a_index_device_address)
{
	BB_ASSERT(a_geometry_sizes.size() == a_primitive_counts.size(), "geometry and primitive must have the same size");
	const ConstSlice<VkAccelerationStructureGeometryKHR> geometry_infos = CreateGeometryInfo(a_temp_arena, a_geometry_sizes, a_vertex_device_address, a_index_device_address);

	VkAccelerationStructureBuildGeometryInfoKHR geometry_sizes{};
	geometry_sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	geometry_sizes.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	geometry_sizes.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	geometry_sizes.geometryCount = static_cast<uint32_t>(geometry_infos.size());
	geometry_sizes.pGeometries = geometry_infos.data();

	uint32_t primitive_count = 1;
	VkAccelerationStructureBuildSizesInfoKHR size_info_get{};
	size_info_get.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	s_vulkan_inst->pfn.GetAccelerationStructureBuildSizesKHR(
		s_vulkan_inst->device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&geometry_sizes,
		a_primitive_counts.data(),
		&size_info_get);

	AccelerationStructSizeInfo size_info;
	size_info.acceleration_structure_size = static_cast<uint32_t>(size_info_get.accelerationStructureSize);
	size_info.scratch_build_size = static_cast<uint32_t>(size_info_get.buildScratchSize);
	size_info.scratch_update_size = static_cast<uint32_t>(size_info_get.updateScratchSize);
	size_info.primitive_count = primitive_count;
	return size_info;
}

AccelerationStructSizeInfo Vulkan::GetTopLevelAccelerationStructSizeInfo(MemoryArena& a_temp_arena, const uint32_t a_instance_count)
{
	VkAccelerationStructureGeometryKHR* instance_infos = ArenaAllocArr(a_temp_arena, VkAccelerationStructureGeometryKHR, a_instance_count);

	for (size_t i = 0; i < a_instance_count; i++)
	{
		VkAccelerationStructureGeometryKHR& inst = instance_infos[i];
		inst.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		inst.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		inst.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		inst.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
		inst.geometry.instances.arrayOfPointers = VK_FALSE;
		inst.geometry.instances.pNext = nullptr;
	}

	VkAccelerationStructureBuildGeometryInfoKHR geometry_sizes{};
	geometry_sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	geometry_sizes.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	geometry_sizes.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	geometry_sizes.geometryCount = a_instance_count;
	geometry_sizes.pGeometries = instance_infos;

	VkAccelerationStructureBuildSizesInfoKHR size_info_get{};
	uint32_t primitive_count = 1;
	s_vulkan_inst->pfn.GetAccelerationStructureBuildSizesKHR(
		s_vulkan_inst->device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&geometry_sizes,
		&primitive_count,
		&size_info_get);

	AccelerationStructSizeInfo size_info;
	size_info.acceleration_structure_size = static_cast<uint32_t>(size_info_get.accelerationStructureSize);
	size_info.scratch_build_size = static_cast<uint32_t>(size_info_get.buildScratchSize);
	size_info.scratch_update_size = static_cast<uint32_t>(size_info_get.updateScratchSize);
	size_info.primitive_count = primitive_count;
	return size_info;
}

RAccelerationStruct Vulkan::CreateAccelerationStruct(const uint32_t a_acceleration_structure_size, const GPUBuffer a_dst_buffer, const uint64_t a_dst_offset)
{
	VkAccelerationStructureKHR acc;

	VkAccelerationStructureCreateInfoKHR create_acc_struct{};
	create_acc_struct.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	create_acc_struct.buffer = reinterpret_cast<VkBuffer>(a_dst_buffer.handle);
	create_acc_struct.size = a_acceleration_structure_size;
	create_acc_struct.offset = a_dst_offset;
	create_acc_struct.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	VKASSERT(s_vulkan_inst->pfn.CreateAccelerationStructureKHR(s_vulkan_inst->device, &create_acc_struct, nullptr, &acc), 
		"VULKAN: failed to create acceleration structure");

	return RAccelerationStruct(reinterpret_cast<uintptr_t>(acc));
}

GPUAddress Vulkan::GetAccelerationStructureAddress(const RAccelerationStruct a_acc_struct)
{
	return GetAccelerationStructureAddress(s_vulkan_inst->device, reinterpret_cast<VkAccelerationStructureKHR>(a_acc_struct.handle));
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
	image_create_info.tiling = a_create_info.use_optimal_tiling ? VK_IMAGE_TILING_OPTIMAL : VK_IMAGE_TILING_LINEAR;
	image_create_info.format = ImageFormats(a_create_info.format);
	image_create_info.usage = ImageUsage(a_create_info.usage);

	//Will be defined in the first layout transition.
	image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_create_info.flags = a_create_info.is_cube_map ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;

	VmaAllocationCreateInfo alloc_info{};
	alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	alloc_info.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	VkImage image;
	VmaAllocation allocation;
	VKASSERT(vmaCreateImage(s_vulkan_inst->vma, 
		&image_create_info,
		&alloc_info,
		&image,
		&allocation,
		nullptr), 
		"Vulkan: Failed to create image");

	SetDebugName(a_create_info.name, image, VK_OBJECT_TYPE_IMAGE);

	AddMemoryToAllocationMap(reinterpret_cast<uintptr_t>(image), allocation);
	return RImage(reinterpret_cast<uintptr_t>(image));
}

void Vulkan::FreeImage(const RImage a_image)
{
	const VmaAllocation* allocation = s_vulkan_inst->allocation_map.find(a_image.handle);
	if (allocation == nullptr)
	{
		BB_WARNING(false, "Trying to find VmaAllocation but it returns nullptr, possibly leaking an image or trying to delete an image that does not exist!", WarningType::HIGH);
		return;
	}
	vmaDestroyImage(s_vulkan_inst->vma,
		reinterpret_cast<VkImage>(a_image.handle),
		*allocation);
	
	s_vulkan_inst->allocation_map.erase(a_image.handle);
}

const RImageView Vulkan::CreateImageView(const ImageViewCreateInfo& a_create_info)
{
	VkImageViewCreateInfo view_info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	view_info.image = reinterpret_cast<VkImage>(a_create_info.image.handle);
	view_info.viewType = ImageViewTypes(a_create_info.type);
	view_info.format = ImageFormats(a_create_info.format);
	view_info.subresourceRange.aspectMask = ImageAspect(a_create_info.aspects);
	view_info.subresourceRange.baseMipLevel = a_create_info.base_mip_level;
	view_info.subresourceRange.levelCount = a_create_info.mip_levels;
	view_info.subresourceRange.baseArrayLayer = a_create_info.base_array_layer;
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

RDescriptorLayout Vulkan::CreateDescriptorLayout(MemoryArena& a_temp_arena, const ConstSlice<DescriptorBindingInfo> a_bindings)
{
	VkDescriptorSetLayoutBinding* layout_binds = ArenaAllocArr(a_temp_arena, VkDescriptorSetLayoutBinding, a_bindings.size());

	VkDescriptorBindingFlags* bindless_flags = ArenaAllocArr(a_temp_arena, VkDescriptorBindingFlags, a_bindings.size());
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

	VkDescriptorSetLayout set_layout;
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

inline static void DescriptorWrite(const VkDescriptorGetInfoEXT& a_desc_info, const RDescriptorLayout a_layout, const uint32_t a_binding, const uint32_t a_descriptor_size, const uint32_t a_descriptor_index, const uint32_t a_buffer_offset, void* a_buffer_start)
{
	VkDeviceSize descriptor_offset;
	s_vulkan_inst->pfn.GetDescriptorSetLayoutBindingOffsetEXT(s_vulkan_inst->device,
		reinterpret_cast<VkDescriptorSetLayout>(a_layout.handle),
		a_binding,
		&descriptor_offset);

	descriptor_offset += static_cast<VkDeviceSize>(a_descriptor_size) * a_descriptor_index;
	void* descriptor_mem = Pointer::Add(a_buffer_start, a_buffer_offset + descriptor_offset);

	s_vulkan_inst->pfn.GetDescriptorEXT(s_vulkan_inst->device, &a_desc_info, a_descriptor_size, descriptor_mem);
}

void Vulkan::DescriptorWriteUniformBuffer(const DescriptorWriteBufferInfo& a_write_info)
{
	VkDescriptorAddressInfoEXT buffer = GetDescriptorAddressInfo(s_vulkan_inst->device, a_write_info.buffer_view);
	VkDescriptorGetInfoEXT desc_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
	desc_info.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	desc_info.data.pUniformBuffer = &buffer;

	DescriptorWrite(desc_info, a_write_info.descriptor_layout, a_write_info.binding, s_vulkan_inst->descriptor_sizes.uniform_buffer, a_write_info.descriptor_index, a_write_info.allocation.offset, a_write_info.allocation.buffer_start);
}

void Vulkan::DescriptorWriteStorageBuffer(const DescriptorWriteBufferInfo& a_write_info)
{
	VkDescriptorAddressInfoEXT buffer = GetDescriptorAddressInfo(s_vulkan_inst->device, a_write_info.buffer_view);
	VkDescriptorGetInfoEXT desc_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
	desc_info.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	desc_info.data.pStorageBuffer = &buffer;

	DescriptorWrite(desc_info, a_write_info.descriptor_layout, a_write_info.binding, s_vulkan_inst->descriptor_sizes.storage_buffer, a_write_info.descriptor_index, a_write_info.allocation.offset, a_write_info.allocation.buffer_start);
}

void Vulkan::DescriptorWriteImage(const DescriptorWriteImageInfo& a_write_info)
{
	VkDescriptorImageInfo image
	{
		VK_NULL_HANDLE,	// static samplers only
		reinterpret_cast<VkImageView>(a_write_info.view.handle),
		ImageLayout(a_write_info.layout)
	};
	VkDescriptorGetInfoEXT desc_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
	desc_info.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	desc_info.data.pSampledImage = &image;

	DescriptorWrite(desc_info, a_write_info.descriptor_layout, a_write_info.binding, s_vulkan_inst->descriptor_sizes.sampled_image, a_write_info.descriptor_index, a_write_info.allocation.offset, a_write_info.allocation.buffer_start);
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

RPipelineLayout Vulkan::CreatePipelineLayout(const RDescriptorLayout* a_descriptor_layouts, const uint32_t a_layout_count, const PushConstantRange a_constant_range)
{
	VkPipelineLayoutCreateInfo create_info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	create_info.setLayoutCount = a_layout_count;
	create_info.pSetLayouts = reinterpret_cast<const VkDescriptorSetLayout*>(a_descriptor_layouts);
	
	if (a_constant_range.size > 0)
	{
		VkPushConstantRange constant_range;
		constant_range.stageFlags = ShaderStageFlags(a_constant_range.stages);
		constant_range.size = a_constant_range.size;
		constant_range.offset = 0;

		create_info.pushConstantRangeCount = 1;
		create_info.pPushConstantRanges = &constant_range;

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

ShaderObject Vulkan::CreateShaderObject(const ShaderObjectCreateInfo& a_shader_object)
{
	VkShaderCreateInfoEXT shader_create_info{ VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT };
	shader_create_info.flags = 0;
	shader_create_info.stage = static_cast<VkShaderStageFlagBits>(ShaderStageFlags(a_shader_object.stage));
	shader_create_info.nextStage = ShaderStageFlagsFromFlags(a_shader_object.next_stages);
	shader_create_info.codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT; //for now always SPIR-V
	shader_create_info.codeSize = a_shader_object.shader_code_size;
	shader_create_info.pCode = a_shader_object.shader_code;
	shader_create_info.pName = a_shader_object.shader_entry;
	shader_create_info.setLayoutCount = a_shader_object.descriptor_layout_count;
	shader_create_info.pSetLayouts = reinterpret_cast<const VkDescriptorSetLayout*>(a_shader_object.descriptor_layouts.data());
	shader_create_info.pSpecializationInfo = nullptr;

	VkShaderEXT shader_object;
	if (a_shader_object.push_constant_range.size)
	{
		VkPushConstantRange constant_range;
		constant_range.stageFlags = ShaderStageFlags(a_shader_object.push_constant_range.stages);
		constant_range.size = a_shader_object.push_constant_range.size;
		constant_range.offset = 0;
		shader_create_info.pPushConstantRanges = &constant_range;
		shader_create_info.pushConstantRangeCount = 1;

		VKASSERT(s_vulkan_inst->pfn.CreateShaderEXT(s_vulkan_inst->device,
			1,
			&shader_create_info,
			nullptr,
			&shader_object),
			"Failed to create a shader object!");

		return ShaderObject(reinterpret_cast<size_t>(shader_object));
	}
	else
	{
		shader_create_info.pPushConstantRanges = nullptr;
		shader_create_info.pushConstantRangeCount = 0;

		VKASSERT(s_vulkan_inst->pfn.CreateShaderEXT(s_vulkan_inst->device,
			1,
			&shader_create_info,
			nullptr,
			&shader_object),
			"Failed to create a shader object!");

		return ShaderObject(reinterpret_cast<size_t>(shader_object));
	}
}

void Vulkan::CreateShaderObjects(MemoryArena& a_temp_arena, Slice<ShaderObjectCreateInfo> a_shader_objects, ShaderObject* a_pshader_objects, const bool a_link_shaders)
{
	VkShaderCreateInfoEXT* shader_create_infos = ArenaAllocArr(a_temp_arena, VkShaderCreateInfoEXT, a_shader_objects.size());

	for (size_t i = 0; i < a_shader_objects.size(); i++)
	{
		const ShaderObjectCreateInfo& shad_info = a_shader_objects[i];
		VkShaderCreateInfoEXT& create_inf = shader_create_infos[i];
		create_inf.sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT;
		create_inf.pNext = nullptr;
		create_inf.flags = a_link_shaders ? VK_SHADER_CREATE_LINK_STAGE_BIT_EXT : 0;
		create_inf.stage = static_cast<VkShaderStageFlagBits>(ShaderStageFlags(shad_info.stage));
		create_inf.nextStage = ShaderStageFlagsFromFlags(shad_info.next_stages);
		create_inf.codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT; //for now always SPIR-V
		create_inf.codeSize = shad_info.shader_code_size;
		create_inf.pCode = shad_info.shader_code;
		create_inf.pName = shad_info.shader_entry;
		create_inf.setLayoutCount = shad_info.descriptor_layout_count;
		create_inf.pSetLayouts = reinterpret_cast<const VkDescriptorSetLayout*>(shad_info.descriptor_layouts.data());

		if (shad_info.push_constant_range.size)
		{
			VkPushConstantRange* constant_ranges = ArenaAllocType(a_temp_arena, VkPushConstantRange);
			constant_ranges->stageFlags = ShaderStageFlags(shad_info.push_constant_range.stages);
			constant_ranges->size = shad_info.push_constant_range.size;
			constant_ranges->offset = 0;
			create_inf.pPushConstantRanges = constant_ranges;
			create_inf.pushConstantRangeCount = 1;
		}
		else
		{
			create_inf.pPushConstantRanges = nullptr;
			create_inf.pushConstantRangeCount = 0;
		}
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
	const VmaAllocation allocation = *s_vulkan_inst->allocation_map.find(a_buffer.handle);
	void* mapped;
	vmaMapMemory(s_vulkan_inst->vma, allocation, &mapped);
	return mapped;
}

void Vulkan::UnmapBufferMemory(const GPUBuffer a_buffer)
{
	const VmaAllocation allocation = *s_vulkan_inst->allocation_map.find(a_buffer.handle);
	vmaUnmapMemory(s_vulkan_inst->vma, allocation);
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
		reinterpret_cast<VkBuffer>(a_copy_buffer.src.handle),
		reinterpret_cast<VkBuffer>(a_copy_buffer.dst.handle),
		static_cast<uint32_t>(a_copy_buffer.regions.size()),
		copy_regions);
}

void Vulkan::CopyImage(const RCommandList a_list, const CopyImageInfo& a_copy_info)
{
	const VkCommandBuffer cmd_list = reinterpret_cast<VkCommandBuffer>(a_list.handle);

	VkImageCopy image_copy;
	image_copy.extent.width = a_copy_info.extent.x;
	image_copy.extent.height = a_copy_info.extent.y;
	image_copy.extent.depth = a_copy_info.extent.z;

	image_copy.srcOffset.x = a_copy_info.src_offset.x;
	image_copy.srcOffset.y = a_copy_info.src_offset.y;
	image_copy.srcOffset.z = a_copy_info.src_offset.z;
	image_copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_copy.srcSubresource.mipLevel = a_copy_info.src_mip_level;
	image_copy.srcSubresource.baseArrayLayer = a_copy_info.src_base_array_layer;
	image_copy.srcSubresource.layerCount = a_copy_info.src_layer_count;

	image_copy.dstOffset.x = a_copy_info.dst_offset.x;
	image_copy.dstOffset.y = a_copy_info.dst_offset.y;
	image_copy.dstOffset.z = a_copy_info.dst_offset.z;
	image_copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_copy.dstSubresource.mipLevel = a_copy_info.dst_mip_level;
	image_copy.dstSubresource.baseArrayLayer = a_copy_info.dst_base_array_layer;
	image_copy.dstSubresource.layerCount = a_copy_info.dst_layer_count;

	vkCmdCopyImage(cmd_list,
		reinterpret_cast<VkImage>(a_copy_info.src_image.handle), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		reinterpret_cast<VkImage>(a_copy_info.dst_image.handle), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &image_copy);
}

void Vulkan::CopyBufferToImage(const RCommandList a_list, const RenderCopyBufferToImageInfo& a_copy_info)
{
	const VkCommandBuffer cmd_list = reinterpret_cast<VkCommandBuffer>(a_list.handle);

	VkBufferImageCopy copy_image;
	copy_image.bufferOffset = a_copy_info.src_offset;
	copy_image.bufferImageHeight = 0;
	copy_image.bufferRowLength = 0;

	copy_image.imageExtent.width = a_copy_info.dst_image_info.extent.x;
	copy_image.imageExtent.height = a_copy_info.dst_image_info.extent.y;
	copy_image.imageExtent.depth = a_copy_info.dst_image_info.extent.z;
	
	copy_image.imageOffset.x = a_copy_info.dst_image_info.offset.x;
	copy_image.imageOffset.y = a_copy_info.dst_image_info.offset.y;
	copy_image.imageOffset.z = a_copy_info.dst_image_info.offset.z;

	copy_image.imageSubresource.mipLevel = a_copy_info.dst_image_info.mip_level;
	copy_image.imageSubresource.baseArrayLayer = a_copy_info.dst_image_info.base_array_layer;
	copy_image.imageSubresource.layerCount = a_copy_info.dst_image_info.layer_count;
	copy_image.imageSubresource.aspectMask = ImageAspect(a_copy_info.dst_aspects);

	vkCmdCopyBufferToImage(cmd_list,
		reinterpret_cast<VkBuffer>(a_copy_info.src_buffer.handle),
		reinterpret_cast<VkImage>(a_copy_info.dst_image.handle),
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&copy_image);
}
void Vulkan::CopyImageToBuffer(const RCommandList a_list, const RenderCopyImageToBufferInfo& a_copy_info)
{
	const VkCommandBuffer cmd_list = reinterpret_cast<VkCommandBuffer>(a_list.handle);

	VkBufferImageCopy copy_image;
	copy_image.bufferOffset = a_copy_info.dst_offset;
	copy_image.bufferImageHeight = 0;
	copy_image.bufferRowLength = 0;

	copy_image.imageExtent.width = a_copy_info.src_image_info.extent.x;
	copy_image.imageExtent.height = a_copy_info.src_image_info.extent.y;
	copy_image.imageExtent.depth = a_copy_info.src_image_info.extent.z;

	copy_image.imageOffset.x = a_copy_info.src_image_info.offset.x;
	copy_image.imageOffset.y = a_copy_info.src_image_info.offset.y;
	copy_image.imageOffset.z = a_copy_info.src_image_info.offset.z;

	copy_image.imageSubresource.aspectMask = ImageAspect(a_copy_info.src_aspects);
	copy_image.imageSubresource.mipLevel = a_copy_info.src_image_info.mip_level;
	copy_image.imageSubresource.baseArrayLayer = a_copy_info.src_image_info.base_array_layer;
	copy_image.imageSubresource.layerCount = a_copy_info.src_image_info.layer_count;

	vkCmdCopyImageToBuffer(cmd_list,
		reinterpret_cast<VkImage>(a_copy_info.src_image.handle),
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		reinterpret_cast<VkBuffer>(a_copy_info.dst_buffer.handle),
		1,
		&copy_image);
}

void Vulkan::ClearImage(const RCommandList a_list, const ClearImageInfo& a_clear_info)
{
	const VkCommandBuffer cmd_list = reinterpret_cast<VkCommandBuffer>(a_list.handle);

	VkClearColorValue clear_color;
	clear_color.float32[0] = a_clear_info.clear_color.e[0];
	clear_color.float32[1] = a_clear_info.clear_color.e[1];
	clear_color.float32[2] = a_clear_info.clear_color.e[2];
	clear_color.float32[3] = a_clear_info.clear_color.e[3];

	VkImageSubresourceRange subresource;
	subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresource.layerCount = a_clear_info.layer_count;
	subresource.levelCount = a_clear_info.level_count;
	subresource.baseArrayLayer = a_clear_info.base_array_layer;
	subresource.baseMipLevel = a_clear_info.base_mip_level;

	vkCmdClearColorImage(cmd_list, reinterpret_cast<VkImage>(a_clear_info.image.handle), ImageLayout(a_clear_info.layout), &clear_color, 1, &subresource);
}

void Vulkan::ClearDepthImage(const RCommandList a_list, const ClearDepthImageInfo& a_clear_info)
{
	const VkCommandBuffer cmd_list = reinterpret_cast<VkCommandBuffer>(a_list.handle);

	VkClearDepthStencilValue clear_color;
	clear_color.depth = a_clear_info.clear_depth;
	clear_color.stencil = a_clear_info.clear_stencil;

	VkImageSubresourceRange subresource;
	subresource.aspectMask = ImageAspect(a_clear_info.depth_aspects);
	subresource.layerCount = a_clear_info.layer_count;
	subresource.levelCount = a_clear_info.level_count;
	subresource.baseArrayLayer = a_clear_info.base_array_layer;
	subresource.baseMipLevel = a_clear_info.base_mip_level;

	vkCmdClearDepthStencilImage(cmd_list, reinterpret_cast<VkImage>(a_clear_info.image.handle), ImageLayout(a_clear_info.layout), &clear_color, 1, &subresource);
}

void Vulkan::BlitImage(const RCommandList a_list, const BlitImageInfo& a_info)
{
	const VkCommandBuffer cmd_list = reinterpret_cast<VkCommandBuffer>(a_list.handle);

	VkImageBlit image_blit{};
	image_blit.srcOffsets[0].x = a_info.src_offset_p0.x;
	image_blit.srcOffsets[0].y = a_info.src_offset_p0.y;
	image_blit.srcOffsets[0].z = a_info.src_offset_p0.z;
	image_blit.srcOffsets[1].x = a_info.src_offset_p1.x;
	image_blit.srcOffsets[1].y = a_info.src_offset_p1.y;
	image_blit.srcOffsets[1].z = a_info.src_offset_p1.z;
	image_blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_blit.srcSubresource.baseArrayLayer = a_info.src_base_layer;
	image_blit.srcSubresource.layerCount = a_info.src_layer_count;
	image_blit.srcSubresource.mipLevel = a_info.src_mip_level;

	image_blit.dstOffsets[0].x = a_info.dst_offset_p0.x;
	image_blit.dstOffsets[0].y = a_info.dst_offset_p0.y;
	image_blit.dstOffsets[0].z = a_info.dst_offset_p0.z;
	image_blit.dstOffsets[1].x = a_info.dst_offset_p1.x;
	image_blit.dstOffsets[1].y = a_info.dst_offset_p1.y;
	image_blit.dstOffsets[1].z = a_info.dst_offset_p1.z;
	image_blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_blit.dstSubresource.baseArrayLayer = a_info.dst_base_layer;
	image_blit.dstSubresource.layerCount = a_info.dst_layer_count;
	image_blit.dstSubresource.mipLevel = a_info.dst_mip_level;

	vkCmdBlitImage(cmd_list,
		reinterpret_cast<VkImage>(a_info.src_image.handle),
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		reinterpret_cast<VkImage>(a_info.dst_image.handle),
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&image_blit,
		VK_FILTER_NEAREST);
}

void Vulkan::BuildBottomLevelAccelerationStruct(MemoryArena& a_temp_arena, const RCommandList a_list, const BuildAccelerationStructInfo& a_build_info, GPUAddress a_vertex_device_address, GPUAddress a_index_device_address)
{
	BB_ASSERT(a_build_info.geometry_sizes.size() == a_build_info.primitive_counts.size(), "geometry and primitive must have the same size");
	const ConstSlice<VkAccelerationStructureGeometryKHR> geometry_infos = CreateGeometryInfo(a_temp_arena, a_build_info.geometry_sizes, a_vertex_device_address, a_index_device_address);
	const VkCommandBuffer cmd_list = reinterpret_cast<VkCommandBuffer>(a_list.handle);

	VkAccelerationStructureBuildRangeInfoKHR* ranges = ArenaAllocArr(a_temp_arena, VkAccelerationStructureBuildRangeInfoKHR, a_build_info.primitive_counts.size());
	for (size_t i = 0; i < a_build_info.primitive_counts.size(); i++)
	{
		VkAccelerationStructureBuildRangeInfoKHR& range = ranges[i];
		range.primitiveCount = a_build_info.primitive_counts[i];
		range.primitiveOffset = 0;
		range.firstVertex = 0;
		range.transformOffset = 0;
	}

	VkAccelerationStructureBuildGeometryInfoKHR acceleration_build_info{};
	acceleration_build_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	acceleration_build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	acceleration_build_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	acceleration_build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	acceleration_build_info.dstAccelerationStructure = reinterpret_cast<VkAccelerationStructureKHR>(a_build_info.acc_struct.handle);
	acceleration_build_info.geometryCount = static_cast<uint32_t>(geometry_infos.size());
	acceleration_build_info.pGeometries = geometry_infos.data();
	acceleration_build_info.scratchData.deviceAddress = a_build_info.scratch_buffer_address;

	s_vulkan_inst->pfn.CmdBuildAccelerationStructuresKHR(cmd_list, 1, &acceleration_build_info, &ranges);
}


void Vulkan::TopLevelAccelerationStruct(MemoryArena& a_temp_arena, const RCommandList a_list, const BuildTopLevelAccelerationStructInfo& a_build_info)
{
	BB_ASSERT(a_build_info.instances.size() == a_build_info.primitive_counts.size(), "geometry and primitive must have the same size");
	const VkCommandBuffer cmd_list = reinterpret_cast<VkCommandBuffer>(a_list.handle);

	const size_t instance_count = a_build_info.primitive_counts.size();
	VkAccelerationStructureBuildRangeInfoKHR* ranges = ArenaAllocArr(a_temp_arena, VkAccelerationStructureBuildRangeInfoKHR, instance_count);
	VkAccelerationStructureInstanceKHR* instances = ArenaAllocArr(a_temp_arena, VkAccelerationStructureInstanceKHR, instance_count);
	for (size_t i = 0; i < instance_count; i++)
	{
		const BottomLevelAccelerationStructInstance& racl = a_build_info.instances[i];
		VkAccelerationStructureInstanceKHR& wacl = instances[i];
		wacl.transform = {
			racl.transform.r0.x, racl.transform.r0.y, racl.transform.r0.z, racl.transform.r0.w,
			racl.transform.r1.x, racl.transform.r1.y, racl.transform.r1.z, racl.transform.r1.w,
			racl.transform.r2.x, racl.transform.r2.y, racl.transform.r2.z, racl.transform.r2.w };
		wacl.instanceCustomIndex = 0;
		wacl.mask = 0xFF;
		wacl.instanceShaderBindingTableRecordOffset = 0;
		wacl.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		wacl.accelerationStructureReference = racl.acceleration_structure_address;

		VkAccelerationStructureBuildRangeInfoKHR& range = ranges[i];
		range.primitiveCount = a_build_info.primitive_counts[i];
		range.primitiveOffset = 0;
		range.firstVertex = 0;
		range.transformOffset = 0;
	}

	const size_t instance_copy_size = sizeof(BottomLevelAccelerationStructInstance) * instance_count;
	BB_ASSERT(instance_copy_size == a_build_info.mapped_size, "instance copy size is not the same as mapped_size");
	memcpy(a_build_info.acceleration_build_mapped, instances, instance_copy_size);

	VkAccelerationStructureGeometryKHR acceleration_instances{};
	acceleration_instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	acceleration_instances.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	acceleration_instances.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	acceleration_instances.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	acceleration_instances.geometry.instances.arrayOfPointers = VK_FALSE;
	acceleration_instances.geometry.instances.data.deviceAddress = a_build_info.acceleration_build_address;

	VkAccelerationStructureBuildGeometryInfoKHR acceleration_build_info{};
	acceleration_build_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	acceleration_build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	acceleration_build_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	acceleration_build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	acceleration_build_info.dstAccelerationStructure = reinterpret_cast<VkAccelerationStructureKHR>(a_build_info.acc_struct.handle);
	acceleration_build_info.geometryCount = 1;
	acceleration_build_info.pGeometries = &acceleration_instances;
	acceleration_build_info.scratchData.deviceAddress = a_build_info.scratch_buffer_address;

	s_vulkan_inst->pfn.CmdBuildAccelerationStructuresKHR(cmd_list, 1, &acceleration_build_info, &ranges);
}

static void _PipelineBarrierFillStages(const IMAGE_LAYOUT a_usage, VkPipelineStageFlags2& a_stage_flags, VkAccessFlags2& a_access_flags, VkImageLayout& a_image_layout)
{
	switch (a_usage)
	{
	case IMAGE_LAYOUT::NONE:
		a_stage_flags = VK_PIPELINE_STAGE_2_NONE;
		a_access_flags = VK_ACCESS_2_NONE;
		a_image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
		break;
	case IMAGE_LAYOUT::RO_GEOMETRY:
		a_stage_flags = VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
		a_access_flags = VK_ACCESS_2_SHADER_READ_BIT;
		a_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		break;
	case IMAGE_LAYOUT::RO_FRAGMENT:
		a_stage_flags = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		a_access_flags = VK_ACCESS_2_SHADER_READ_BIT;
		a_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		break;
	case IMAGE_LAYOUT::RO_COMPUTE:
		a_stage_flags = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		a_access_flags = VK_ACCESS_2_SHADER_READ_BIT;
		a_access_flags = VK_IMAGE_ASPECT_COLOR_BIT;
		a_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		break;
	case IMAGE_LAYOUT::RW_GEOMETRY:
		a_stage_flags = VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
		a_access_flags = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
		a_image_layout = VK_IMAGE_LAYOUT_GENERAL;
		break;
	case IMAGE_LAYOUT::RW_FRAGMENT:
		a_stage_flags = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		a_access_flags = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
		a_image_layout = VK_IMAGE_LAYOUT_GENERAL;
		break;
	case IMAGE_LAYOUT::RW_COMPUTE:
		a_stage_flags = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		a_access_flags = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
		a_image_layout = VK_IMAGE_LAYOUT_GENERAL;
		break;
	case IMAGE_LAYOUT::RO_DEPTH:
		a_stage_flags = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		a_access_flags = VK_ACCESS_2_SHADER_READ_BIT;
		a_image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		break;
	case IMAGE_LAYOUT::RT_DEPTH:
		a_stage_flags = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
		a_access_flags = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		a_image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		break;
	case IMAGE_LAYOUT::RT_COLOR:
		a_stage_flags = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		a_access_flags = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR;
		a_image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		break;
	case IMAGE_LAYOUT::COPY_SRC:
		a_stage_flags = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		a_access_flags = VK_ACCESS_2_TRANSFER_READ_BIT;
		a_image_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		break;
	case IMAGE_LAYOUT::COPY_DST:
		a_stage_flags = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		a_access_flags = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		a_image_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		break;
	case IMAGE_LAYOUT::PRESENT:
		a_stage_flags = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
		a_access_flags = VK_ACCESS_2_NONE;
		a_image_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		break;
	default:
		BB_ASSERT(false, "default case hit");
		break;
	}
}

void Vulkan::PipelineBarriers(const RCommandList a_list, const PipelineBarrierInfo& a_barriers)
{
	VkMemoryBarrier2* global_barriers = BBstackAlloc(a_barriers.global_barriers.size(), VkMemoryBarrier2);
	VkBufferMemoryBarrier2* buffer_barriers = BBstackAlloc(a_barriers.buffer_barriers.size(), VkBufferMemoryBarrier2);
	VkImageMemoryBarrier2* image_barriers = BBstackAlloc(a_barriers.image_barriers.size(), VkImageMemoryBarrier2);

	for (size_t i = 0; i < a_barriers.global_barriers.size(); i++)
	{
		BB_UNIMPLEMENTED("implement global barriers");
	}

	for (size_t i = 0; i < a_barriers.buffer_barriers.size(); i++)
	{
		BB_UNIMPLEMENTED("implement buffer barriers");
	}

	for (size_t i = 0; i < a_barriers.image_barriers.size(); i++)
	{
		const PipelineBarrierImageInfo& barrier_info = a_barriers.image_barriers[i];
		VkImageMemoryBarrier2& wr_b = image_barriers[i];

		wr_b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		wr_b.pNext = nullptr;
		wr_b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		wr_b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		wr_b.image = reinterpret_cast<VkImage>(barrier_info.image.handle);
		wr_b.subresourceRange.aspectMask = ImageAspect(barrier_info.image_aspect);
		wr_b.subresourceRange.baseMipLevel = barrier_info.base_mip_level;
		wr_b.subresourceRange.levelCount = barrier_info.level_count;
		wr_b.subresourceRange.baseArrayLayer = barrier_info.base_array_layer;
		wr_b.subresourceRange.layerCount = barrier_info.layer_count;

		_PipelineBarrierFillStages(barrier_info.prev, wr_b.srcStageMask, wr_b.srcAccessMask, wr_b.oldLayout);
		_PipelineBarrierFillStages(barrier_info.next, wr_b.dstStageMask, wr_b.dstAccessMask, wr_b.newLayout);
	}

	VkDependencyInfo dependency_info{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	dependency_info.memoryBarrierCount = static_cast<uint32_t>(a_barriers.global_barriers.size());
	dependency_info.pMemoryBarriers = global_barriers;
	dependency_info.bufferMemoryBarrierCount = static_cast<uint32_t>(a_barriers.buffer_barriers.size());
	dependency_info.pBufferMemoryBarriers = buffer_barriers;
	dependency_info.imageMemoryBarrierCount = static_cast<uint32_t>(a_barriers.image_barriers.size());
	dependency_info.pImageMemoryBarriers = image_barriers;

	const VkCommandBuffer cmd_buffer = reinterpret_cast<VkCommandBuffer>(a_list.handle);
	vkCmdPipelineBarrier2(cmd_buffer, &dependency_info);
}

void Vulkan::StartRenderPass(const RCommandList a_list, const StartRenderingInfo& a_render_info)
{
	const VkCommandBuffer cmd_buffer = reinterpret_cast<VkCommandBuffer>(a_list.handle);

	VkRenderingInfo rendering_info{ VK_STRUCTURE_TYPE_RENDERING_INFO };

	VkRenderingAttachmentInfo depth_attachment{};
	if (a_render_info.depth_attachment)
	{
		const RenderingAttachmentDepth& depth_info = *a_render_info.depth_attachment;

		depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depth_attachment.loadOp = depth_info.load_depth ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_attachment.storeOp = depth_info.store_depth ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_NONE;
		depth_attachment.imageLayout = ImageLayout(depth_info.image_layout);
		depth_attachment.imageView = reinterpret_cast<VkImageView>(depth_info.image_view.handle);
		depth_attachment.clearValue.depthStencil = { depth_info.clear_value.depth, depth_info.clear_value.stencil };

		rendering_info.pDepthAttachment = &depth_attachment;

		vkCmdSetStencilTestEnable(cmd_buffer, VK_FALSE);
		vkCmdSetDepthBiasEnable(cmd_buffer, VK_TRUE);
		vkCmdSetDepthTestEnable(cmd_buffer, VK_TRUE);
		vkCmdSetDepthWriteEnable(cmd_buffer, VK_TRUE);
		vkCmdSetDepthCompareOp(cmd_buffer, VK_COMPARE_OP_LESS_OR_EQUAL);
	}
	else
	{
		rendering_info.pDepthAttachment = nullptr;

		vkCmdSetStencilTestEnable(cmd_buffer, false);
		vkCmdSetDepthBiasEnable(cmd_buffer, VK_FALSE);
		vkCmdSetDepthTestEnable(cmd_buffer, VK_FALSE);
		vkCmdSetDepthWriteEnable(cmd_buffer, VK_FALSE);
	}

	FixedArray<VkRenderingAttachmentInfo, MAX_COLOR_ATTACHMENTS> color_attachments{};
	if (a_render_info.color_attachments.size())
	{
		for (size_t i = 0; i < a_render_info.color_attachments.size(); i++)
		{
			const RenderingAttachmentColor& color_info = a_render_info.color_attachments[i];
			VkRenderingAttachmentInfo& color_attach = color_attachments[i];
			color_attach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			color_attach.loadOp = color_info.load_color ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
			color_attach.storeOp = color_info.store_color ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_NONE;
			color_attach.imageLayout = ImageLayout(color_info.image_layout);
			color_attach.imageView = reinterpret_cast<VkImageView>(color_info.image_view.handle);
			color_attach.clearValue.color.float32[0] = color_info.clear_value_rgba.x;
			color_attach.clearValue.color.float32[1] = color_info.clear_value_rgba.y;
			color_attach.clearValue.color.float32[2] = color_info.clear_value_rgba.z;
			color_attach.clearValue.color.float32[3] = color_info.clear_value_rgba.w;
		}
	}

	VkRect2D scissor{};
	scissor.offset.x = a_render_info.render_area_offset.x;
	scissor.offset.y = a_render_info.render_area_offset.y;
	scissor.extent.width = a_render_info.render_area_extent.x;
	scissor.extent.height = a_render_info.render_area_extent.y;

	rendering_info.renderArea = scissor;
	rendering_info.layerCount = 1;
	rendering_info.pColorAttachments = color_attachments.data();
	rendering_info.colorAttachmentCount = static_cast<uint32_t>(a_render_info.color_attachments.size());
	vkCmdBeginRendering(cmd_buffer, &rendering_info);

	// maybe make this it's own function? Think there is no need now
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(a_render_info.render_area_extent.x);
	viewport.height = static_cast<float>(a_render_info.render_area_extent.y);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewportWithCount(cmd_buffer, 1, &viewport);

	vkCmdSetScissorWithCount(cmd_buffer, 1, &scissor);
}

void Vulkan::EndRenderPass(const RCommandList a_list)
{
	const VkCommandBuffer cmd_buffer = reinterpret_cast<VkCommandBuffer>(a_list.handle);
	vkCmdEndRendering(cmd_buffer);
}

void Vulkan::SetScissor(const RCommandList a_list, const ScissorInfo& a_scissor)
{
	const VkCommandBuffer cmd_buffer = reinterpret_cast<VkCommandBuffer>(a_list.handle);

	VkRect2D scissor;
	scissor.offset = { a_scissor.offset.x, a_scissor.offset.y };
	scissor.extent = { a_scissor.extent.x, a_scissor.extent.y };

	vkCmdSetScissorWithCount(cmd_buffer, 1, &scissor);
}

void Vulkan::BindIndexBuffer(const RCommandList a_list, const GPUBuffer a_buffer, const uint64_t a_offset)
{
	const VkCommandBuffer cmd_buffer = reinterpret_cast<VkCommandBuffer>(a_list.handle);

	vkCmdBindIndexBuffer(cmd_buffer,
		reinterpret_cast<VkBuffer>(a_buffer.handle),
		a_offset,
		VK_INDEX_TYPE_UINT32);
}

void Vulkan::BindShaders(const RCommandList a_list, const uint32_t a_shader_stage_count, const SHADER_STAGE* a_shader_stages, const ShaderObject* a_shader_objects)
{
	const VkCommandBuffer cmd_buffer = reinterpret_cast<VkCommandBuffer>(a_list.handle);

	VkShaderStageFlagBits* shader_stages = BBstackAlloc(a_shader_stage_count, VkShaderStageFlagBits);
	for (size_t i = 0; i < a_shader_stage_count; i++)
		shader_stages[i] = static_cast<VkShaderStageFlagBits>(ShaderStageFlags(a_shader_stages[i]));

	s_vulkan_inst->pfn.CmdBindShadersEXT(cmd_buffer, a_shader_stage_count, shader_stages, reinterpret_cast<const VkShaderEXT*>(a_shader_objects));

	vkCmdSetRasterizerDiscardEnable(cmd_buffer, VK_FALSE);

	vkCmdSetPrimitiveTopology(cmd_buffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	s_vulkan_inst->pfn.CmdSetPolygonModeEXT(cmd_buffer, VK_POLYGON_MODE_FILL);
	s_vulkan_inst->pfn.CmdSetRasterizationSamplesEXT(cmd_buffer, VK_SAMPLE_COUNT_1_BIT);
	const uint32_t mask = UINT32_MAX;
	s_vulkan_inst->pfn.CmdSetSampleMaskEXT(cmd_buffer, VK_SAMPLE_COUNT_1_BIT, &mask);

	s_vulkan_inst->pfn.CmdSetAlphaToCoverageEnableEXT(cmd_buffer, VK_FALSE);
	//FOR imgui maybe not, but that is because I'm dumb asf
	s_vulkan_inst->pfn.CmdSetVertexInputEXT(cmd_buffer, 0, nullptr, 0, nullptr); 
	vkCmdSetPrimitiveRestartEnable(cmd_buffer, VK_FALSE);
}

void Vulkan::SetBlendMode(const RCommandList a_list, const uint32_t a_first_attachment, const Slice<ColorBlendState> a_blend_states)
{
	BB_ASSERT(a_blend_states.size() < MAX_COLOR_ATTACHMENTS, "more then MAX_COLOR_ATTACHMENTS of blend states");
	const VkCommandBuffer cmd_buffer = reinterpret_cast<VkCommandBuffer>(a_list.handle);
	const uint32_t blend_state_count = static_cast<uint32_t>(a_blend_states.size());

	FixedArray<VkBool32, MAX_COLOR_ATTACHMENTS> color_enables;
	FixedArray<VkColorComponentFlags, MAX_COLOR_ATTACHMENTS> color_flags;
	FixedArray<VkColorBlendEquationEXT, MAX_COLOR_ATTACHMENTS> blend_eq;

	for (size_t i = 0; i < a_blend_states.size(); i++)
	{
		const ColorBlendState& cbs = a_blend_states[i];
		color_enables[i] = cbs.blend_enable;
		color_flags[i] = cbs.color_flags;

		blend_eq[i].colorBlendOp = BlendOp(cbs.color_blend_op);
		blend_eq[i].srcColorBlendFactor = BlendFactor(cbs.src_blend);
		blend_eq[i].dstColorBlendFactor = BlendFactor(cbs.dst_blend);
		blend_eq[i].alphaBlendOp = BlendOp(cbs.alpha_blend_op);
		blend_eq[i].srcAlphaBlendFactor = BlendFactor(cbs.src_alpha_blend);
		blend_eq[i].dstAlphaBlendFactor = BlendFactor(cbs.dst_alpha_blend);
	}

	s_vulkan_inst->pfn.CmdSetColorBlendEnableEXT(cmd_buffer, a_first_attachment, blend_state_count, color_enables.data());
	s_vulkan_inst->pfn.CmdSetColorWriteMaskEXT(cmd_buffer, a_first_attachment, blend_state_count, color_flags.data());
	s_vulkan_inst->pfn.CmdSetColorBlendEquationEXT(cmd_buffer, a_first_attachment, blend_state_count, blend_eq.data());
}

void Vulkan::SetFrontFace(const RCommandList a_list, const bool a_is_clockwise)
{
	const VkCommandBuffer cmd_buffer = reinterpret_cast<VkCommandBuffer>(a_list.handle);
	vkCmdSetFrontFace(cmd_buffer, static_cast<VkFrontFace>(a_is_clockwise));
}

void Vulkan::SetCullMode(const RCommandList a_list, const CULL_MODE a_cull_mode)
{
	const VkCommandBuffer cmd_buffer = reinterpret_cast<VkCommandBuffer>(a_list.handle);
	vkCmdSetCullMode(cmd_buffer, s_vulkan_inst->enum_conv.cull_modes[static_cast<uint32_t>(a_cull_mode)]);
}

void Vulkan::SetDepthBias(const RCommandList a_list, const float a_bias_constant_factor, const float a_bias_clamp, const float a_bias_slope_factor)
{
	const VkCommandBuffer cmd_buffer = reinterpret_cast<VkCommandBuffer>(a_list.handle);
	vkCmdSetDepthBias(cmd_buffer, a_bias_constant_factor, a_bias_clamp, a_bias_slope_factor);
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

void Vulkan::DrawVertices(const RCommandList a_list, const uint32_t a_vertex_count, const uint32_t a_instance_count, const uint32_t a_first_vertex, const uint32_t a_first_instance)
{
	const VkCommandBuffer cmd_buffer = reinterpret_cast<VkCommandBuffer>(a_list.handle);

	vkCmdDraw(cmd_buffer, a_vertex_count, a_instance_count, a_first_vertex, a_first_instance);
}

void Vulkan::DrawIndexed(const RCommandList a_list, const uint32_t a_index_count, const uint32_t a_instance_count, const uint32_t a_first_index, const int32_t a_vertex_offset, const uint32_t a_first_instance)
{
	const VkCommandBuffer cmd_buffer = reinterpret_cast<VkCommandBuffer>(a_list.handle);

	vkCmdDrawIndexed(cmd_buffer, a_index_count, a_instance_count, a_first_index, a_vertex_offset, a_first_instance);
}

PRESENT_IMAGE_RESULT Vulkan::UploadImageToSwapchain(const RCommandList a_list, const RImage a_src_image, const uint32_t a_array_layer, const int2 a_src_image_size, const int2 a_swapchain_size, const uint32_t a_backbuffer_index)
{
	uint32_t image_index;
	const VkResult result = vkAcquireNextImageKHR(s_vulkan_inst->device,
		s_vulkan_swapchain->swapchain,
		UINT64_MAX,
		s_vulkan_swapchain->frames[a_backbuffer_index].image_available_semaphore,
		VK_NULL_HANDLE,
		&image_index);

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		return PRESENT_IMAGE_RESULT::SWAPCHAIN_OUT_OF_DATE;
	}
	else if (result != VK_SUCCESS)
	{
		BB_ASSERT(false, "Vulkan: failed to get next image.");
	}

	const VkCommandBuffer cmd_buffer = reinterpret_cast<VkCommandBuffer>(a_list.handle);

	constexpr VkImageLayout START_LAYOUT = VK_IMAGE_LAYOUT_UNDEFINED;
	constexpr VkImageLayout TRANSFER_LAYOUT = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	constexpr VkImageLayout END_LAYOUT = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	
	const VkImage swapchain_image = s_vulkan_swapchain->frames[a_backbuffer_index].image;

	{
		VkImageMemoryBarrier2 upload_barrier{};
		upload_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		upload_barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		upload_barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
		upload_barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		upload_barrier.oldLayout = START_LAYOUT;
		upload_barrier.newLayout = TRANSFER_LAYOUT;
		upload_barrier.image = swapchain_image;
		upload_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		upload_barrier.subresourceRange.baseArrayLayer = 0;
		upload_barrier.subresourceRange.layerCount = 1;
		upload_barrier.subresourceRange.baseMipLevel = 0;
		upload_barrier.subresourceRange.levelCount = 1;

		VkDependencyInfo barrier_info{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
		barrier_info.pImageMemoryBarriers = &upload_barrier;
		barrier_info.imageMemoryBarrierCount = 1;

		vkCmdPipelineBarrier2(cmd_buffer, &barrier_info);
	}

	{
		//always single so don't use VkImageBlit2 here.
		VkImageBlit image_blit{};
		image_blit.srcOffsets[1].x = a_src_image_size.x;
		image_blit.srcOffsets[1].y = a_src_image_size.y;
		image_blit.srcOffsets[1].z = 1;

		image_blit.dstOffsets[1].x = a_swapchain_size.x;
		image_blit.dstOffsets[1].y = a_swapchain_size.y;
		image_blit.dstOffsets[1].z = 1;

		image_blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		image_blit.srcSubresource.baseArrayLayer = a_array_layer;
		image_blit.srcSubresource.layerCount = 1;
		image_blit.srcSubresource.mipLevel = 0;

		image_blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		image_blit.dstSubresource.baseArrayLayer = 0;
		image_blit.dstSubresource.layerCount = 1;
		image_blit.dstSubresource.mipLevel = 0;

		vkCmdBlitImage(cmd_buffer,
			reinterpret_cast<VkImage>(a_src_image.handle),
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			swapchain_image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&image_blit,
			VK_FILTER_NEAREST);
	}

	{
		VkImageMemoryBarrier2 present_barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
		present_barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		present_barrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
		present_barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		present_barrier.oldLayout = TRANSFER_LAYOUT;
		present_barrier.newLayout = END_LAYOUT;
		present_barrier.image = swapchain_image;
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

	return PRESENT_IMAGE_RESULT::SUCCESS;
}

void Vulkan::ExecuteCommandLists(const RQueue a_queue, const ExecuteCommandsInfo* a_execute_infos, const uint32_t a_execute_info_count)
{
	VkTimelineSemaphoreSubmitInfo* timeline_sem_infos = BBstackAlloc(
		a_execute_info_count,
		VkTimelineSemaphoreSubmitInfo);
	VkSubmitInfo* submit_infos = BBstackAlloc(
		a_execute_info_count,
		VkSubmitInfo);

	for (size_t i = 0; i < a_execute_info_count; i++)
	{
		const ExecuteCommandsInfo& exe_inf = a_execute_infos[i];
		VkTimelineSemaphoreSubmitInfo& cur_sem_inf = timeline_sem_infos[i];
		VkSubmitInfo& cur_sub_inf = submit_infos[i];

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
		submit_infos,
		VK_NULL_HANDLE),
		"Vulkan: failed to submit to queue.");
}

PRESENT_IMAGE_RESULT Vulkan::ExecutePresentCommandList(const RQueue a_queue, const ExecuteCommandsInfo& a_execute_info, const uint32_t a_backbuffer_index)
{
	// TEMP
	constexpr VkPipelineStageFlags WAIT_STAGES[8] = { VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT };

	// handle the window api for vulkan.
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

	VkSubmitInfo submit_info{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit_info.pNext = &timeline_sem_info;
	submit_info.commandBufferCount = a_execute_info.list_count;
	submit_info.pCommandBuffers = reinterpret_cast<const VkCommandBuffer*>(a_execute_info.lists);
	submit_info.waitSemaphoreCount = wait_semaphore_count;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.signalSemaphoreCount = signal_semaphore_count;
	submit_info.pSignalSemaphores = signal_semaphores;
	submit_info.pWaitDstStageMask = WAIT_STAGES;

	VkQueue queue = reinterpret_cast<VkQueue>(a_queue.handle);
	VKASSERT(vkQueueSubmit(queue,
		1,
		&submit_info,
		VK_NULL_HANDLE),
		"Vulkan: failed to submit to queue.");

	VkPresentInfoKHR present_info{};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &s_vulkan_swapchain->frames[a_backbuffer_index].present_finished_semaphore;
	present_info.swapchainCount = 1; //Swapchain will always be 1
	present_info.pSwapchains = &s_vulkan_swapchain->swapchain;
	present_info.pImageIndices = &a_backbuffer_index; //THIS MAY BE WRONG
	present_info.pResults = nullptr;

	const VkResult result = vkQueuePresentKHR(s_vulkan_inst->present_queue, &present_info);
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		return PRESENT_IMAGE_RESULT::SWAPCHAIN_OUT_OF_DATE;
	}
	else if (result != VK_SUCCESS)
	{
		BB_ASSERT(false, "Vulkan: Failed to queuepresentKHR.");
	}

	return PRESENT_IMAGE_RESULT::SUCCESS;
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

void Vulkan::WaitFence(const RFence a_fence, const GPUFenceValue a_fence_value)
{
	VkSemaphoreWaitInfo wait_info{ VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
	wait_info.semaphoreCount = 1;
	wait_info.pSemaphores = reinterpret_cast<const VkSemaphore*>(&a_fence);
	wait_info.pValues = &a_fence_value;

	vkWaitSemaphores(s_vulkan_inst->device, &wait_info, 1000000000);
}

void Vulkan::WaitFences(const RFence* a_fences, const GPUFenceValue* a_fence_values, const uint32_t a_fence_count)
{
	VkSemaphoreWaitInfo wait_info{ VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
	wait_info.semaphoreCount = a_fence_count;
	wait_info.pSemaphores = reinterpret_cast<const VkSemaphore*>(a_fences);
	wait_info.pValues = a_fence_values;

	vkWaitSemaphores(s_vulkan_inst->device, &wait_info, 1000000000);
}

GPUFenceValue Vulkan::GetCurrentFenceValue(const RFence a_fence)
{
	GPUFenceValue value;
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
