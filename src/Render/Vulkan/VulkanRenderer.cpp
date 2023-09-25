#include "VulkanRenderer.hpp"
#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif //_WIN32
#include <Vulkan/vulkan.h>

#define VMA_STATIC_VULKAN_FUNCTIONS 1
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#define VMA_VULKAN_VERSION 1003000 // Vulkan 1.3
#define VMA_IMPLEMENTATION
//interface library in Cmake does not seem to include this directory. WHY?!
#include "../../../lib/VMA/vk_mem_alloc.h"

#include "Storage/Slotmap.h"

using namespace BB;
using namespace Render;

#ifdef _DEBUG
#define VKASSERT(a_VKResult, a_Msg)\
	if (a_VKResult != VK_SUCCESS)\
		BB_ASSERT(false, a_Msg)
#else
#define VKASSERT(a_VKResult, a_Msg) a_VKResult
#endif //_DEBUG

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

static inline VkDeviceSize GetBufferDeviceAddress(const VkDevice a_device, const VkBuffer a_buffer)
{
	VkBufferDeviceAddressInfoKHR buffer_address_info{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	buffer_address_info.buffer = a_buffer;
	return vkGetBufferDeviceAddress(a_device, &buffer_address_info);
}

class VulkanDescriptorLinearBuffer
{
public:
	VulkanDescriptorLinearBuffer(const size_t a_buffer_size, const VkBufferUsageFlags a_buffer_usage);
	~VulkanDescriptorLinearBuffer();

	const DescriptorAllocation AllocateDescriptor(const RDescriptor a_descriptor);

private:
	VkBuffer m_buffer;
	VmaAllocation m_allocation;
	//using uint32_t since descriptor buffers on some drivers only spend 32-bits virtual address.
	uint32_t m_size;
	uint32_t m_offset;
	void* m_start;
	VkDeviceAddress m_start_address = 0;
};

constexpr int VULKAN_VERSION = 3;

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT a_message_severity,
	VkDebugUtilsMessageTypeFlagsEXT a_message_type,
	const VkDebugUtilsMessengerCallbackDataEXT* a_pcallback_data,
	void* a_puser_data)
{
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
	}
	return VK_FALSE;
}

static VkDebugUtilsMessengerCreateInfoEXT CreateDebugCallbackCreateInfo()
{
	VkDebugUtilsMessengerCreateInfoEXT t_CreateInfo{};
	t_CreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	t_CreateInfo.messageSeverity =
		//VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	t_CreateInfo.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	t_CreateInfo.pfnUserCallback = debugCallback;
	t_CreateInfo.pUserData = nullptr;

	return t_CreateInfo;
}

static bool CheckExtensionSupport(Allocator a_temp_allocator, Slice<const char*> a_extensions)
{
	// check extensions if they are available.
	uint32_t extension_count;
	vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
	VkExtensionProperties* extensions = BBnewArr(a_temp_allocator, extension_count, VkExtensionProperties);
	vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions);

	for (auto t_It = a_extensions.begin(); t_It < a_extensions.end(); t_It++)
	{
		for (size_t i = 0; i < extension_count; i++)
		{

			if (strcmp(*t_It, extensions[i].extensionName) == 0)
				break;

			if (t_It == a_extensions.end())
				return false;
		}
	}
	return true;
}

static inline VkDescriptorType DescriptorBufferType(const RENDER_DESCRIPTOR_TYPE a_Type)
{
	switch (a_Type)
	{
	case RENDER_DESCRIPTOR_TYPE::READONLY_CONSTANT:	return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	case RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER:	return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	case RENDER_DESCRIPTOR_TYPE::READWRITE:			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	case RENDER_DESCRIPTOR_TYPE::IMAGE:				return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	case RENDER_DESCRIPTOR_TYPE::SAMPLER:			return VK_DESCRIPTOR_TYPE_SAMPLER;
	default:
		BB_ASSERT(false, "Vulkan: RENDER_DESCRIPTOR_TYPE failed to convert to a VkDescriptorType.");
		return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		break;
	}
}

static inline VkShaderStageFlagBits ShaderStageBits(const RENDER_SHADER_STAGE a_Stage)
{
	switch (a_Stage)
	{
	case RENDER_SHADER_STAGE::ALL:					return VK_SHADER_STAGE_ALL;
	case RENDER_SHADER_STAGE::VERTEX:				return VK_SHADER_STAGE_VERTEX_BIT;
	case RENDER_SHADER_STAGE::FRAGMENT_PIXEL:		return VK_SHADER_STAGE_FRAGMENT_BIT;
	default:
		BB_ASSERT(false, "Vulkan: RENDER_SHADER_STAGE failed to convert to a VkShaderStageFlagBits.");
		return VK_SHADER_STAGE_ALL;
		break;
	}
}

static inline VkImageLayout ImageLayout(const RENDER_IMAGE_LAYOUT a_ImageLayout)
{
	switch (a_ImageLayout)
	{
	case RENDER_IMAGE_LAYOUT::UNDEFINED:				return VK_IMAGE_LAYOUT_UNDEFINED;
	case RENDER_IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	case RENDER_IMAGE_LAYOUT::DEPTH_STENCIL_ATTACHMENT: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	case RENDER_IMAGE_LAYOUT::GENERAL:					return VK_IMAGE_LAYOUT_GENERAL;
	case RENDER_IMAGE_LAYOUT::TRANSFER_SRC:				return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	case RENDER_IMAGE_LAYOUT::TRANSFER_DST:				return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	case RENDER_IMAGE_LAYOUT::SHADER_READ_ONLY:			return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	case RENDER_IMAGE_LAYOUT::PRESENT:					return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	default:
		BB_ASSERT(false, "Vulkan: RENDER_IMAGE_LAYOUT failed to convert to a VkImageLayout.");
		return VK_IMAGE_LAYOUT_UNDEFINED;
		break;
	}
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

VulkanQueueDeviceInfo FindQueueIndex(VkQueueFamilyProperties* a_queue_properties, uint32_t a_family_property_count, VkQueueFlags a_queue_flags)
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
		VulkanQueueDeviceInfo t_GraphicQueue = FindQueueIndex(queue_families,
			queue_family_count,
			VK_QUEUE_GRAPHICS_BIT);

		return_value.graphics = t_GraphicQueue.index;
		return_value.graphics_count = t_GraphicQueue.queueCount;
		return_value.present = t_GraphicQueue.index;
	}

	{
		VulkanQueueDeviceInfo t_TransferQueue = FindQueueIndex(queue_families,
			queue_family_count,
			VK_QUEUE_TRANSFER_BIT);
		//Check if the queueindex is the same as graphics.
		if (t_TransferQueue.index != return_value.graphics)
		{
			return_value.transfer = t_TransferQueue.index;
			return_value.transfer_count = t_TransferQueue.queueCount;
		}
		else
		{
			return_value.transfer = return_value.graphics;
			return_value.transfer_count = return_value.graphics_count;
		}
	}

	{
		VulkanQueueDeviceInfo t_ComputeQueue = FindQueueIndex(queue_families,
			queue_family_count,
			VK_QUEUE_COMPUTE_BIT);
		//Check if the queueindex is the same as graphics.
		if ((t_ComputeQueue.index != return_value.graphics) &&
			(t_ComputeQueue.index != return_value.compute))
		{
			return_value.compute = t_ComputeQueue.index;
			return_value.compute = t_ComputeQueue.queueCount;
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
	VkDevice return_device;

	VkPhysicalDeviceFeatures device_features{};
	device_features.samplerAnisotropy = VK_TRUE;
	VkPhysicalDeviceTimelineSemaphoreFeatures timeline_sem_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES };
	timeline_sem_features.timelineSemaphore = VK_TRUE;
	timeline_sem_features.pNext = nullptr;

	VkPhysicalDeviceShaderDrawParametersFeatures shader_draw_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES };
	shader_draw_features.pNext = nullptr;
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
	device_create_info.pNext = &sync_features;

	VKASSERT(vkCreateDevice(a_phys_device,
		&device_create_info,
		nullptr,
		&return_device),
		"Failed to create logical device Vulkan.");

	return return_device;
}

struct Vulkan_inst
{
	VkInstance instance;
	VkPhysicalDevice phys_device;
	VkDevice device;
	VkQueue present_queue;
	VmaAllocator vma;

	//jank pointer
	VulkanDescriptorLinearBuffer* pdescriptor_buffer;

	StaticSlotmap<VulkanBuffer> buffers;

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

VulkanDescriptorLinearBuffer::VulkanDescriptorLinearBuffer(const size_t a_buffer_size, const VkBufferUsageFlags a_buffer_usage)
	:	m_size(static_cast<uint32_t>(a_buffer_size))
{
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

const DescriptorAllocation VulkanDescriptorLinearBuffer::AllocateDescriptor(const RDescriptor a_descriptor)
{
	DescriptorAllocation allocation;
	const VkDescriptorSetLayout descriptor_set = reinterpret_cast<VkDescriptorSetLayout>(a_descriptor.handle);
	VkDeviceSize descriptors_size;
	s_vulkan_inst->pfn.GetDescriptorSetLayoutSizeEXT(
		s_vulkan_inst->device,
		descriptor_set,
		&descriptors_size);

	allocation.descriptor = a_descriptor;
	allocation.size = static_cast<uint32_t>(descriptors_size);
	allocation.offset = m_offset;

	m_offset += allocation.size;

	return allocation;
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

#define SetDebugName(a_name, a_object_handle, a_obj_type) SetDebugName_f(a_name, (uint64_t)a_object_handle, a_obj_type)
#else
#define SetDebugName()
#endif _DEBUG

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
		VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME
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

		s_vulkan_inst->pfn.GetDescriptorSetLayoutSizeEXT = (PFN_vkGetDescriptorSetLayoutSizeEXT)vkGetInstanceProcAddr(s_vulkan_inst->instance, "vkGetDescriptorSetLayoutSizeEXT");
		s_vulkan_inst->pfn.GetDescriptorSetLayoutBindingOffsetEXT = (PFN_vkGetDescriptorSetLayoutBindingOffsetEXT)vkGetInstanceProcAddr(s_vulkan_inst->instance, "vkGetDescriptorSetLayoutBindingOffsetEXT");
		s_vulkan_inst->pfn.GetDescriptorEXT = (PFN_vkGetDescriptorEXT)vkGetInstanceProcAddr(s_vulkan_inst->instance, "vkGetDescriptorEXT");
		s_vulkan_inst->pfn.CmdBindDescriptorBuffersEXT = (PFN_vkCmdBindDescriptorBuffersEXT)vkGetInstanceProcAddr(s_vulkan_inst->instance, "vkCmdBindDescriptorBuffersEXT");
		s_vulkan_inst->pfn.CmdBindDescriptorBufferEmbeddedSamplersEXT = (PFN_vkCmdBindDescriptorBufferEmbeddedSamplersEXT)vkGetInstanceProcAddr(s_vulkan_inst->instance, "vkCmdBindDescriptorBufferEmbeddedSamplersEXT");
		s_vulkan_inst->pfn.CmdSetDescriptorBufferOffsetsEXT = (PFN_vkCmdSetDescriptorBufferOffsetsEXT)vkGetInstanceProcAddr(s_vulkan_inst->instance, "vkCmdSetDescriptorBufferOffsetsEXT");
		s_vulkan_inst->pfn.SetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(s_vulkan_inst->instance, "vkSetDebugUtilsObjectNameEXT");

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
	s_vulkan_inst->pdescriptor_buffer = BBnew(a_stack_allocator, VulkanDescriptorLinearBuffer)(
		mbSize * 4,
		VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

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
	s_vulkan_swapchain = BBnew(a_stack_allocator, Vulkan_swapchain);
	
	BBStackAllocatorScope(a_stack_allocator)
	{
		//Surface
		VkWin32SurfaceCreateInfoKHR surface_create_info{};
		surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surface_create_info.hwnd = reinterpret_cast<HWND>(a_window_handle.ptr_handle);
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

void Vulkan::CreateCommandPool(const RENDER_QUEUE_TYPE a_queue_type, const uint32_t a_command_list_count, RCommandPool& a_pool, RCommandList* a_plists)
{
	VkCommandPoolCreateInfo pool_create_info{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	switch (a_queue_type)
	{
	case RENDER_QUEUE_TYPE::GRAPHICS:
		pool_create_info.queueFamilyIndex = s_vulkan_inst->queue_indices.graphics;
		break;
	case RENDER_QUEUE_TYPE::TRANSFER:
		pool_create_info.queueFamilyIndex = s_vulkan_inst->queue_indices.transfer;
		break;
	case RENDER_QUEUE_TYPE::COMPUTE:
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


	a_pool = (uintptr_t)command_pool;
}

void Vulkan::FreeCommandPool(const RCommandPool a_pool)
{
	vkDestroyCommandPool(s_vulkan_inst->device, reinterpret_cast<VkCommandPool>(a_pool.handle), nullptr);
}

const RBuffer Vulkan::CreateBuffer(const BufferCreateInfo& a_create_info)
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
		vma_alloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
		vma_alloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		break;
	case BUFFER_TYPE::STORAGE:
		buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		vma_alloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		break;
	case BUFFER_TYPE::UNIFORM:
		buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		vma_alloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		break;
	case BUFFER_TYPE::VERTEX:
		buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		vma_alloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		break;
	case BUFFER_TYPE::INDEX:
		buffer_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		vma_alloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		break;
	default:
		BB_ASSERT(false, "unknown buffer type");
		break;
	}

	VKASSERT(vmaCreateBuffer(s_vulkan_inst->vma,
		&buffer_info, &vma_alloc,
		&buffer.buffer, &buffer.allocation,
		nullptr), "Vulkan::VMA, Failed to allocate memory");

	SetDebugName(a_create_info.name, buffer.buffer, VK_OBJECT_TYPE_BUFFER);

	return RBuffer(s_vulkan_inst->buffers.insert(buffer).handle);
}

void Vulkan::FreeBuffer(const RBuffer a_buffer)
{
	s_vulkan_inst->buffers.erase(a_buffer.handle);
}

RDescriptor Vulkan::CreateDescriptor(Allocator a_temp_allocator, Slice<DescriptorBindingInfo> a_bindings)
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
		layout_binds[i].stageFlags = ShaderStageBits(binding.shader_stage);

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

	return RDescriptor((uint64_t)set_layout);
}

DescriptorAllocation Vulkan::AllocateDescriptor(const RDescriptor a_descriptor)
{
	return s_vulkan_inst->pdescriptor_buffer->AllocateDescriptor(a_descriptor);
}

void* Vulkan::MapBufferMemory(const RBuffer a_buffer)
{
	const VulkanBuffer& buffer = s_vulkan_inst->buffers.find(a_buffer.handle);
	void* mapped;
	vmaMapMemory(s_vulkan_inst->vma, buffer.allocation, &mapped);
	return mapped;
}

void Vulkan::UnmapBufferMemory(const RBuffer a_buffer)
{
	const VulkanBuffer& buffer = s_vulkan_inst->buffers.find(a_buffer.handle);
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

	SetDebugName(a_name, cmd_list, VK_OBJECT_TYPE_COMMAND_BUFFER);
}

void Vulkan::EndCommandList(const RCommandList a_list)
{
	const VkCommandBuffer cmd_list = reinterpret_cast<VkCommandBuffer>(a_list.handle);
	VKASSERT(vkEndCommandBuffer(cmd_list), "Vulkan: Error when trying to end commandbuffer!");
	SetDebugName(nullptr, cmd_list, VK_OBJECT_TYPE_COMMAND_BUFFER);
}

void Vulkan::StartRendering(const RCommandList a_list, const StartRenderingInfo& a_render_info, const uint32_t a_backbuffer_index)
{
	VkCommandBuffer cmd_buffer = reinterpret_cast<VkCommandBuffer>(a_list.handle);

	//include depth stencil later.
	VkImageMemoryBarrier2 image_barriers[1];
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

	VkDependencyInfo barrier_info{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	barrier_info.pImageMemoryBarriers = image_barriers;
	barrier_info.imageMemoryBarrierCount = 1;

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

	VkRenderingInfo rendering_info{ VK_STRUCTURE_TYPE_RENDERING_INFO };
	rendering_info.renderArea = scissor;
	rendering_info.layerCount = 1;
	rendering_info.pColorAttachments = &rendering_attachment;
	rendering_info.colorAttachmentCount = 1;

	vkCmdBeginRendering(cmd_buffer, &rendering_info);

	VkViewport t_Viewport{};
	t_Viewport.x = 0.0f;
	t_Viewport.y = 0.0f;
	t_Viewport.width = static_cast<float>(a_render_info.viewport_width);
	t_Viewport.height = static_cast<float>(a_render_info.viewport_height);
	t_Viewport.minDepth = 0.0f;
	t_Viewport.maxDepth = 1.0f;
	vkCmdSetViewport(cmd_buffer, 0, 1, &t_Viewport);

	vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);
}

void Vulkan::EndRendering(const RCommandList a_list, const EndRenderingInfo& a_rendering_info, const uint32_t a_backbuffer_index)
{
	VkCommandBuffer cmd_buffer = reinterpret_cast<VkCommandBuffer>(a_list.handle);
	vkCmdEndRendering(cmd_buffer);

	VkImageMemoryBarrier present_barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	present_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	present_barrier.oldLayout = ImageLayout(a_rendering_info.initial_layout);
	present_barrier.newLayout = ImageLayout(a_rendering_info.final_layout);
	present_barrier.image = s_vulkan_swapchain->frames[a_backbuffer_index].image;
	present_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	present_barrier.subresourceRange.baseArrayLayer = 0;
	present_barrier.subresourceRange.layerCount = 1;
	present_barrier.subresourceRange.baseMipLevel = 0;
	present_barrier.subresourceRange.levelCount = 1;

	vkCmdPipelineBarrier(cmd_buffer,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&present_barrier);
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
	wait_semaphores[a_execute_info.wait_count] = s_vulkan_swapchain->frames[a_backbuffer_index].image_available_semaphore;

	Memory::Copy<VkSemaphore>(signal_semaphores, a_execute_info.signal_fences, a_execute_info.signal_count);
	signal_semaphores[a_execute_info.signal_count] = s_vulkan_swapchain->frames[a_backbuffer_index].present_finished_semaphore;

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
	VkPresentInfoKHR present_info{};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &s_vulkan_swapchain->frames[a_backbuffer_index].present_finished_semaphore;
	present_info.swapchainCount = 1; //Swapchain will always be 1
	present_info.pSwapchains = &s_vulkan_swapchain->swapchain;
	present_info.pImageIndices = &a_backbuffer_index; //THIS MAY BE WRONG
	present_info.pResults = nullptr;

	VKASSERT(vkQueuePresentKHR(s_vulkan_inst->present_queue, &present_info),
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

	return RFence((uintptr_t)timeline_semaphore);
}

void Vulkan::FreeFence(const RFence a_fence)
{
	vkDestroySemaphore(s_vulkan_inst->device, reinterpret_cast<VkSemaphore>(a_fence.handle), nullptr);
}

void Vulkan::WaitFences(const RFence* a_fences, const uint64_t* a_fence_values, const uint32_t a_fence_count)
{
	VkSemaphoreWaitInfo t_WaitInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
	t_WaitInfo.semaphoreCount = a_fence_count;
	t_WaitInfo.pSemaphores = reinterpret_cast<const VkSemaphore*>(a_fences);
	t_WaitInfo.pValues = a_fence_values;

	vkWaitSemaphores(s_vulkan_inst->device, &t_WaitInfo, 1000000000);
}

RQueue Vulkan::GetQueue(const RENDER_QUEUE_TYPE a_queue_type, const char* a_name)
{
	uint32_t queue_index;
	switch (a_queue_type)
	{
	case RENDER_QUEUE_TYPE::GRAPHICS:
		queue_index = s_vulkan_inst->queue_indices.graphics;
		break;
	case RENDER_QUEUE_TYPE::TRANSFER:
		queue_index = s_vulkan_inst->queue_indices.transfer;
		break;
	case RENDER_QUEUE_TYPE::COMPUTE:
		queue_index = s_vulkan_inst->queue_indices.compute;
		break;
	default:
		BB_ASSERT(false, "Vulkan: Trying to get a device queue that you didn't setup yet.");
		break;
	}
	VkQueue queue;
	vkGetDeviceQueue(s_vulkan_inst->device,
		queue_index,
		0,
		&queue);

	SetDebugName(a_name, queue, VK_OBJECT_TYPE_QUEUE);

	return RQueue((uint64_t)queue);
}