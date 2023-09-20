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
	VulkanQueuesIndices return_value;

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

	VkPhysicalDeviceDescriptorIndexingFeatures t_IndexingFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES };
	t_IndexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
	t_IndexingFeatures.runtimeDescriptorArray = VK_TRUE;
	t_IndexingFeatures.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
	t_IndexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
	t_IndexingFeatures.pNext = &dynamic_rendering;

	VkPhysicalDeviceBufferDeviceAddressFeatures address_feature{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };
	address_feature.bufferDeviceAddress = VK_TRUE;
	address_feature.pNext = &t_IndexingFeatures;

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

	uint32_t buffer_count = 0;
	uint32_t buffer_max = 32;
	VulkanBuffer* buffers;

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

bool Render::InitializeVulkan(StackAllocator_t& a_stack_allocator, const char* a_app_name, const char* a_engine_name, const bool a_debug)
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

	s_vulkan_inst->buffers = BBnewArr(a_stack_allocator, s_vulkan_inst->buffer_max, VulkanBuffer);

	return true;
}

bool Render::CreateSwapchain(StackAllocator_t& a_stack_allocator, const WindowHandle a_window_handle, const uint32_t a_width, const uint32_t a_height, const uint32_t a_backbuffer_count)
{
	BB_ASSERT(s_vulkan_inst != nullptr, "trying to create a swapchain while vulkan is not initialized");
	BB_ASSERT(s_vulkan_swapchain == nullptr, "trying to create a swapchain while one exists");
	s_vulkan_swapchain = BBnew(a_stack_allocator, Vulkan_swapchain);
	

	BBStackAllocatorScope(a_stack_allocator)
	{
		//Surface
		VkWin32SurfaceCreateInfoKHR surface_create_info{};
		surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surface_create_info.hwnd = reinterpret_cast<HWND>(a_window_handle.ptrHandle);
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

		BB_WARNING(backbuffer_count == a_backbuffer_count, "backbuffer amount changed when creating swapchain", WarningType::HIGH);
		BB_ASSERT(backbuffer_count <= a_backbuffer_count, "too many backbuffers", WarningType::HIGH);

		VKASSERT(vkCreateSwapchainKHR(s_vulkan_inst->device, 
			&swapchain_create_info, 
			nullptr, 
			&s_vulkan_swapchain->swapchain), 
			"Vulkan: Failed to create swapchain.");

		return true;
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

		VkSemaphoreCreateInfo t_SemInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

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
				&t_SemInfo,
				nullptr,
				&s_vulkan_swapchain->frames[i].image_available_semaphore);
			vkCreateSemaphore(s_vulkan_inst->device,
				&t_SemInfo,
				nullptr,
				&s_vulkan_swapchain->frames[i].present_finished_semaphore);
		}
	}
}

const RBuffer Render::CreateBuffer(const BufferCreateInfo& a_create_info)
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
		vma_alloc.usage == VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
		vma_alloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		break;
	case BUFFER_TYPE::STORAGE:
		buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		vma_alloc.usage == VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		break;
	case BUFFER_TYPE::UNIFORM:
		buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		vma_alloc.usage == VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		break;
	case BUFFER_TYPE::VERTEX:
		buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		vma_alloc.usage == VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		break;
	case BUFFER_TYPE::INDEX:
		buffer_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		vma_alloc.usage == VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
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

	const uint32_t buffer_pos = s_vulkan_inst->buffer_count++;
	s_vulkan_inst->buffers[buffer_pos] = buffer;
	return RBuffer(buffer_pos);
}