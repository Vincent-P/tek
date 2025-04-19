#include <vulkan/vulkan.h>
#if defined(_WIN32)
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#elif defined(__linux__)
#include <vulkan/vulkan_wayland.h>
#endif

#include "vulkan.h"
#include "offalloc.h"
#include "./offalloc.c"

// -- Device
#define FRAME_COUNT 2

#define MAIN_MEMORY_SIZE (128 << 20)
#define RT_MEMORY_SIZE (64 << 20)
#define MEMORY_ALIGNMENT (64 << 10)
#define MAX_BACKBUFFER_COUNT 5
#define VK_BUFFER_CAPACITY 64
#define VK_PROGRAM_CAPACITY 64
#define VK_RT_CAPACITY 16
#define VK_TEXTURE_CAPACITY 16

typedef struct VulkanGraphicsProgram
{
	VkPipeline pipeline;
} VulkanGraphicsProgram;

typedef struct VulkanBuffer
{
	oa_allocation_t allocation;
	VkBuffer buffer;
	void* mapped;
	uint64_t gpu_address;
	uint32_t size;
	VkBufferCreateFlags flags;
	VkBufferUsageFlagBits  usage;
} VulkanBuffer;

typedef struct VulkanRenderTarget
{
	oa_allocation_t allocation;
	VkImage image;
	VkImageView image_view;
	VkFormat format;
	uint32_t memory_offset;
	uint32_t width;
	uint32_t height;
} VulkanRenderTarget;

typedef struct VulkanTexture
{
	oa_allocation_t allocation;
	VkImage image;
	VkImageView image_view;
	VkFormat format;
	uint32_t memory_offset;
	uint32_t width;
	uint32_t height;
	uint32_t upload_offset; // temp
	uint32_t upload_size; // temp
} VulkanTexture;

struct VulkanDevice
{
	// device
	VkInstance instance;
	VkDevice device;
	VkQueue graphics_queue;
	VkPhysicalDevice physical_device;
	uint32_t graphics_family_idx;
	PFN_vkCreateDebugUtilsMessengerEXT my_vkCreateDebugUtilsMessengerEXT;
	PFN_vkDestroyDebugUtilsMessengerEXT my_vkDestroyDebugUtilsMessengerEXT;
	PFN_vkCmdInsertDebugUtilsLabelEXT my_vkCmdInsertDebugUtilsLabelEXT;
	PFN_vkCmdPushDescriptorSetKHR my_vkCmdPushDescriptorSetKHR;
	// memory
	oa_allocator_t main_memory_allocator;
	oa_allocator_t rt_memory_allocator;
	VkDeviceMemory main_memory;
	VkDeviceMemory rt_memory;
	void* main_memory_mapped;
	uint32_t main_memory_type_index;
	uint32_t rt_type_index;
	// command buffers
	VkCommandPool command_pool[FRAME_COUNT];
	VkCommandBuffer command_buffer[FRAME_COUNT];
	VkFence command_fence[FRAME_COUNT];
	VkSemaphore swapchain_acquire_semaphore[FRAME_COUNT];
	VkSemaphore swapchain_present_semaphore[FRAME_COUNT];
	uint32_t current_frame;
	// swapchain
	VkSwapchainKHR swapchain;
	uint32_t swapchain_image_count;
	int should_recreate_swapchain;
	uint32_t swapchain_width;
	uint32_t swapchain_height;
	VkFormat swapchain_format;
	void *swapchain_last_wnd;
	VkImage swapchain_images[MAX_BACKBUFFER_COUNT];
	// resources
	VulkanGraphicsProgram graphics_psos[VK_PROGRAM_CAPACITY];
	VulkanBuffer buffers[VK_BUFFER_CAPACITY];
	VulkanRenderTarget rts[VK_RT_CAPACITY];
	VulkanTexture textures[VK_TEXTURE_CAPACITY];
	uint32_t upload_buffer; // TEMP
	uint32_t upload_buffer_offset;
	VkSampler default_sampler;
	VkPipelineLayout pipeline_layout;
};

struct VulkanFrame
{
	VkCommandBuffer cmd;
	uint32_t iframe;
};

struct VulkanRenderPass
{
	VulkanFrame *frame;
	VulkanRenderTarget *color_rts[8];
	uint32_t color_rts_length;
	VulkanRenderTarget *depth_rt;
	uint32_t render_width;
	uint32_t render_height;
};


uint32_t vulkan_get_device_size() { return sizeof(VulkanDevice); }

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
						     VkDebugUtilsMessageTypeFlagsEXT message_type,
						     const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
						     void *user_data)
{
	printf("%s\n", pCallbackData->pMessage);
	__debugbreak();
	return VK_FALSE;
}

static void create_swapchain(VulkanDevice *device, void *hwnd);
void new_buffer_internal(VulkanDevice *device, uint32_t handle,uint32_t size, VkBufferCreateFlags flags, VkBufferUsageFlagBits  usage);

void vulkan_create_device(VulkanDevice *device, void *hwnd)
{
	// -- Create instance
	const char *instance_extensions[] = {
		VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(_WIN32)
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(__linux__)
		VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
#endif
 		VK_EXT_DEBUG_UTILS_EXTENSION_NAME
	};
	const char * instance_layers[] = {"VK_LAYER_KHRONOS_validation"};
	VkApplicationInfo app_info  	= {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO};
	app_info.pApplicationName   	= "tek";
	app_info.applicationVersion 	= VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName		= "cengine";
	app_info.engineVersion	  	= VK_MAKE_VERSION(1, 0, 0);
	app_info.apiVersion		= VK_API_VERSION_1_4;
	VkInstanceCreateInfo create_info	= {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
	create_info.pApplicationInfo		= &app_info;
	create_info.enabledLayerCount	   	= ARRAY_LENGTH(instance_layers);
	create_info.ppEnabledLayerNames		= instance_layers;
	create_info.enabledExtensionCount	= ARRAY_LENGTH(instance_extensions);
	create_info.ppEnabledExtensionNames	= instance_extensions;
	VkResult res = vkCreateInstance(&create_info, NULL, &device->instance);
	assert(res == VK_SUCCESS);
	device->my_vkCreateDebugUtilsMessengerEXT  = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(device->instance, "vkCreateDebugUtilsMessengerEXT");
	device->my_vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(device->instance, "vkDestroyDebugUtilsMessengerEXT");
	device->my_vkCmdInsertDebugUtilsLabelEXT = (PFN_vkCmdInsertDebugUtilsLabelEXT)vkGetInstanceProcAddr(device->instance, "vkCmdInsertDebugUtilsLabelEXT");

	if (device->my_vkCreateDebugUtilsMessengerEXT){
		VkDebugUtilsMessengerCreateInfoEXT ci	= {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
		ci.flags				= 0;
		ci.messageSeverity			= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
		ci.messageSeverity 			|= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		ci.messageType	 			= VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
		ci.messageType	 			|= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		ci.pfnUserCallback 			= debug_callback;

		VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
		res = device->my_vkCreateDebugUtilsMessengerEXT(device->instance, &ci, NULL, &messenger);
		assert(res == VK_SUCCESS);
	}

	
	/// --- Pick physical device
	VkPhysicalDevice physical_devices[16] = {0};
	uint32_t physical_devices_count = 16;
	vkEnumeratePhysicalDevices(device->instance, &physical_devices_count, physical_devices);
	device->physical_device = physical_devices[0];
	VkPhysicalDeviceVulkan14Features vulkan14_features = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES};
	vulkan14_features.maintenance5 = 1;
	VkPhysicalDeviceVulkan13Features vulkan13_features = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
	vulkan13_features.dynamicRendering = 1;
	VkPhysicalDeviceVulkan12Features vulkan12_features = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
	VkPhysicalDeviceFeatures2 physical_device_features = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
	vulkan13_features.pNext = &vulkan14_features;
	vulkan12_features.pNext = &vulkan13_features;
	physical_device_features.pNext = &vulkan12_features;
	vkGetPhysicalDeviceFeatures2(device->physical_device, &physical_device_features);

	VkQueueFamilyProperties queue_families[16];
	uint32_t queue_families_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device->physical_device, &queue_families_count, NULL);
	assert(queue_families_count < 16);
	vkGetPhysicalDeviceQueueFamilyProperties(device->physical_device, &queue_families_count, queue_families);
	float priority = 0.0;
	VkDeviceQueueCreateInfo queue_info	= {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
	queue_info.queueFamilyIndex		= 0;
	queue_info.queueCount			= 1;
	queue_info.pQueuePriorities		= &priority;
	device->graphics_family_idx		= 0;
	for (uint32_t i = 0; i < queue_families_count; i++) {
		if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT){
			device->graphics_family_idx = i;
		}
	}
	queue_info.queueFamilyIndex = device->graphics_family_idx;
	printf("queue family index: %u\n", queue_info.queueFamilyIndex);
	const char *device_extensions[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
	};
	VkDeviceCreateInfo dci		= {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
	dci.pNext			= &physical_device_features;
	dci.flags			= 0;
	dci.queueCreateInfoCount	= 1;
	dci.pQueueCreateInfos		= &queue_info;
	dci.enabledLayerCount		= 0;
	dci.ppEnabledLayerNames		= NULL;
	dci.enabledExtensionCount	= ARRAY_LENGTH(device_extensions);
	dci.ppEnabledExtensionNames	= device_extensions;
	dci.pEnabledFeatures		= NULL;
	res = vkCreateDevice(device->physical_device, &dci, NULL, &device->device);
	assert(res == VK_SUCCESS);
	vkGetDeviceQueue(device->device, device->graphics_family_idx, 0, &device->graphics_queue);
	
	device->my_vkCmdPushDescriptorSetKHR = (PFN_vkCmdPushDescriptorSetKHR)vkGetDeviceProcAddr(device->device, "vkCmdPushDescriptorSetKHR");
	assert(device->my_vkCmdPushDescriptorSetKHR != NULL);
	
	// -- Create command buffers
	for (uint32_t iframe = 0; iframe < FRAME_COUNT; ++iframe) {
		VkCommandPoolCreateInfo command_pool_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
		command_pool_info.flags = 0;
		command_pool_info.queueFamilyIndex = device->graphics_family_idx;
		res = vkCreateCommandPool(device->device, &command_pool_info, NULL, &device->command_pool[iframe]);
		assert(res == VK_SUCCESS);
		VkCommandBufferAllocateInfo cmd_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
		cmd_info.commandPool = device->command_pool[iframe];
		cmd_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmd_info.commandBufferCount = 1;
		res = vkAllocateCommandBuffers(device->device, &cmd_info, &device->command_buffer[iframe]);
		assert(res == VK_SUCCESS);
		VkFenceCreateInfo fence_info = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
		fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		res = vkCreateFence(device->device, &fence_info, NULL, &device->command_fence[iframe]);
		assert(res == VK_SUCCESS);
		VkSemaphoreCreateInfo semaphore_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		res = vkCreateSemaphore(device->device, &semaphore_info, NULL, &device->swapchain_acquire_semaphore[iframe]);
		assert(res == VK_SUCCESS);		
		res = vkCreateSemaphore(device->device, &semaphore_info, NULL, &device->swapchain_present_semaphore[iframe]);
		assert(res == VK_SUCCESS);
	}

	
	// -- Prepare device memory
	VkPhysicalDeviceMemoryProperties mem_props = {0};
	vkGetPhysicalDeviceMemoryProperties(device->physical_device, &mem_props);
	uint32_t main_memory_type_index = -1;
	uint32_t rt_type_index = -1;
	uint32_t MEMORY_MASK = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
		VkMemoryPropertyFlags flags = mem_props.memoryTypes[i].propertyFlags;
		printf("memoryType[%u] = %u (heap %u)\n", i, flags, mem_props.memoryTypes[i].heapIndex);
		// Look for GPU memory that is accessible from the CPU
		if (flags == MEMORY_MASK) {
			if (main_memory_type_index == -1) {
				main_memory_type_index = i;
				printf("main memory = memoryType[%u]\n", i);
				break;
			}
		}
		// Look for GPU memory that is accessible from the CPU
		if (flags == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT && rt_type_index == -1) {
			printf("rt memory = memoryType[%u]\n", i);
			rt_type_index = i;
		}
	}
	device->main_memory_type_index = main_memory_type_index;
	device->rt_type_index = rt_type_index;
	VkMemoryAllocateFlagsInfo mem_flags = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
	mem_flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
	VkMemoryAllocateInfo mem_info = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	mem_info.pNext = &mem_flags;
	mem_info.allocationSize = MAIN_MEMORY_SIZE;
	mem_info.memoryTypeIndex = main_memory_type_index;
	res = vkAllocateMemory(device->device, &mem_info, NULL, &device->main_memory);
	assert(res == VK_SUCCESS);
	res = vkMapMemory(device->device, device->main_memory, 0, mem_info.allocationSize, 0, &device->main_memory_mapped);
	assert(res == VK_SUCCESS);
	mem_info.pNext = NULL;
	mem_info.allocationSize = RT_MEMORY_SIZE;
	mem_info.memoryTypeIndex = rt_type_index;
	res = vkAllocateMemory(device->device, &mem_info, NULL, &device->rt_memory);
	assert(res == VK_SUCCESS);
	res = oa_create(&device->main_memory_allocator, MAIN_MEMORY_SIZE / MEMORY_ALIGNMENT, 128 * 1024);
	assert(res == 0);
	res = oa_create(&device->rt_memory_allocator, RT_MEMORY_SIZE / MEMORY_ALIGNMENT, VK_RT_CAPACITY);
	assert(res == 0);

	// -- Resources
	device->upload_buffer = VK_BUFFER_CAPACITY - 1;
	new_buffer_internal(device, device->upload_buffer, (8 << 20), 0, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	device->upload_buffer_offset = 0;

	VkSamplerCreateInfo sampler_info = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
	res = vkCreateSampler(device->device, &sampler_info, NULL, &device->default_sampler);
	assert(res == VK_SUCCESS);
	
	// -- Prepare descriptor layout
	VkDescriptorSetLayoutBinding binding = {0};
	binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding.descriptorCount = 1;
	binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	VkDescriptorSetLayoutCreateInfo desc_layout_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	desc_layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
	desc_layout_info.bindingCount = 1;
	desc_layout_info.pBindings = &binding;
	VkDescriptorSetLayout desc_layout = VK_NULL_HANDLE;
	res = vkCreateDescriptorSetLayout(device->device, &desc_layout_info, NULL, &desc_layout);
	assert(res == VK_SUCCESS);
	
	VkPushConstantRange push_constants_ranges[] = {
		// {stage, offset, size}
		{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 256},
	};
	VkPipelineLayoutCreateInfo pipeline_layout_info = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	pipeline_layout_info.setLayoutCount =1;
	pipeline_layout_info.pSetLayouts = &desc_layout;
	pipeline_layout_info.pushConstantRangeCount = ARRAY_LENGTH(push_constants_ranges);
	pipeline_layout_info.pPushConstantRanges = push_constants_ranges;
	res = vkCreatePipelineLayout(device->device, &pipeline_layout_info, NULL, &device->pipeline_layout);
	assert(res == VK_SUCCESS);

	device->swapchain_last_wnd = hwnd;
	create_swapchain(device, hwnd);
}

static void create_swapchain(VulkanDevice *device, void *hwnd)
{
	vkDeviceWaitIdle(device->device);
	
	VkSwapchainKHR old_swapchain = device->swapchain;
	
	uint32_t width = 256;
	uint32_t height = 256;
	VkSurfaceKHR surface = NULL;

#if defined(_WIN32)
	
	RECT client_rect = {0};
	BOOL win32_res = GetClientRect(hwnd, &client_rect);
	assert(win32_res);
	width = client_rect.right - client_rect.left;
	height = client_rect.bottom - client_rect.top;
	VkWin32SurfaceCreateInfoKHR createInfo = {.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
	createInfo.pNext = NULL;
	createInfo.hinstance = GetModuleHandle(NULL);
	createInfo.hwnd = hwnd;
	VkResult res = vkCreateWin32SurfaceKHR(device->instance, &createInfo, NULL, &surface);
	assert(res == VK_SUCCESS);

#elif defined(__linux__)

	width = g_wnd.w;
	height = g_wnd.h;
	
	struct VkWaylandSurfaceCreateInfoKHR create_info = {.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR};
	create_info.display = g_display;
	create_info.surface = g_surface;
	VkResult res = vkCreateWaylandSurfaceKHR(device->instance, &create_info, NULL, &surface);
	assert(res == VK_SUCCESS);
	
#endif

	printf("Creating a %ux%u swapchain\n", width, height);
	
	// Get the list of VkFormats that are supported:
	uint32_t formatCount;
	res = vkGetPhysicalDeviceSurfaceFormatsKHR(device->physical_device, surface, &formatCount, NULL);
	assert(res == VK_SUCCESS);
	VkSurfaceFormatKHR *surfFormats = (VkSurfaceFormatKHR *)malloc(formatCount * sizeof(VkSurfaceFormatKHR));
	res = vkGetPhysicalDeviceSurfaceFormatsKHR(device->physical_device, surface, &formatCount, surfFormats);
	assert(res == VK_SUCCESS);
	VkFormat format;
	// If the format list includes just one entry of VK_FORMAT_UNDEFINED,
	// the surface has no preferred format.  Otherwise, at least one
	// supported format will be returned.
	if (formatCount == 1 && surfFormats[0].format == VK_FORMAT_UNDEFINED) {
		format = VK_FORMAT_B8G8R8A8_UNORM;
	} else {
		assert(formatCount >= 1);
		format = surfFormats[0].format;
	}
	free(surfFormats);
	device->swapchain_format = format;
	VkSurfaceCapabilitiesKHR surfCapabilities = {0};
	res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->physical_device, surface, &surfCapabilities);
	assert(res == VK_SUCCESS);
	VkExtent2D swapchainExtent = {0};
	// width and height are either both 0xFFFFFFFF, or both not 0xFFFFFFFF.
	if (surfCapabilities.currentExtent.width == 0xFFFFFFFF) {
		// If the surface size is undefined, the size is set to
		// the size of the images requested.
		swapchainExtent.width = width;
		swapchainExtent.height = height;
		if (swapchainExtent.width < surfCapabilities.minImageExtent.width) {
			swapchainExtent.width = surfCapabilities.minImageExtent.width;
		} else if (swapchainExtent.width > surfCapabilities.maxImageExtent.width) {
			swapchainExtent.width = surfCapabilities.maxImageExtent.width;
		}

		if (swapchainExtent.height < surfCapabilities.minImageExtent.height) {
			swapchainExtent.height = surfCapabilities.minImageExtent.height;
		} else if (swapchainExtent.height > surfCapabilities.maxImageExtent.height) {
			swapchainExtent.height = surfCapabilities.maxImageExtent.height;
		}
	} else {
		// If the surface size is defined, the swap chain size must match
		swapchainExtent = surfCapabilities.currentExtent;
		width = swapchainExtent.width;
		height = swapchainExtent.height;
	}
	// The FIFO present mode is guaranteed by the spec to be supported
	VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	uint32_t images_count = surfCapabilities.minImageCount;
	if (images_count < 3 && surfCapabilities.maxImageCount >= 3) {
		images_count = 3;
	}
	assert(images_count < MAX_BACKBUFFER_COUNT);
		
	VkSurfaceTransformFlagBitsKHR preTransform = surfCapabilities.currentTransform;
	if (surfCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
		preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	// Find a supported composite alpha mode - one of these is guaranteed to be set
	VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	VkCompositeAlphaFlagBitsKHR compositeAlphaFlags[4] = {
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
	};
	for (uint32_t i = 0; i < sizeof(compositeAlphaFlags) / sizeof(compositeAlphaFlags[0]); i++) {
		if (surfCapabilities.supportedCompositeAlpha & compositeAlphaFlags[i]) {
			compositeAlpha = compositeAlphaFlags[i];
			break;
		}
	}
	device->swapchain_width = width;
	device->swapchain_height = height;

	VkSwapchainCreateInfoKHR swapchain_ci = {.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
	swapchain_ci.surface = surface;
	swapchain_ci.minImageCount = images_count;
	swapchain_ci.imageFormat = format;
	swapchain_ci.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	swapchain_ci.imageExtent.width = swapchainExtent.width;
	swapchain_ci.imageExtent.height = swapchainExtent.height;
	swapchain_ci.imageArrayLayers = 1;
	swapchain_ci.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchain_ci.queueFamilyIndexCount = 0;
	swapchain_ci.pQueueFamilyIndices = NULL;
	swapchain_ci.preTransform = preTransform;
	swapchain_ci.compositeAlpha = compositeAlpha;
	swapchain_ci.presentMode = swapchainPresentMode;
	swapchain_ci.clipped = 1;

	if (old_swapchain != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(device->device, old_swapchain, NULL);
	}

	res = vkCreateSwapchainKHR(device->device, &swapchain_ci, NULL, &device->swapchain);
	assert(res == VK_SUCCESS);


	res = vkGetSwapchainImagesKHR(device->device, device->swapchain, &device->swapchain_image_count, NULL);
	assert(res == VK_SUCCESS);
	res = vkGetSwapchainImagesKHR(device->device, device->swapchain, &device->swapchain_image_count, device->swapchain_images);
	assert(res == VK_SUCCESS);

#if defined(__linux__)
	wl_surface_commit(g_surface);
#endif
}


// -- Graphics Programs
void new_graphics_program(VulkanDevice *device, uint32_t handle, MaterialAsset material_asset)
{
	struct VulkanGraphicsPsoSpec spec = {0};
	spec.topology = VULKAN_TOPOLOGY_TRIANGLE_LIST;
	new_graphics_program_ex(device, handle, material_asset, spec);
}

void new_graphics_program_ex(VulkanDevice *device, uint32_t handle, MaterialAsset material_asset, struct VulkanGraphicsPsoSpec spec)
{
	assert(material_asset.render_pass_id < ARRAY_LENGTH(RENDER_PASSES));
	struct RenderPass renderpass = RENDER_PASSES[material_asset.render_pass_id];
	
	VkShaderModuleCreateInfo vshader_module = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
	vshader_module.codeSize = material_asset.vertex_shader_bytecode.size;
	vshader_module.pCode = material_asset.vertex_shader_bytecode.data;
	VkShaderModuleCreateInfo pshader_module = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
	pshader_module.codeSize = material_asset.pixel_shader_bytecode.size;
	pshader_module.pCode = material_asset.pixel_shader_bytecode.data;
	VkPipelineShaderStageCreateInfo ShaderStages[2] = {0};
	ShaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	ShaderStages[0].pNext = &vshader_module;
	ShaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	ShaderStages[0].pName = "main";
	ShaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	ShaderStages[1].pNext = &pshader_module;
	ShaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	ShaderStages[1].pName = "main";
	
	VkPipelineVertexInputStateCreateInfo VertexInputState = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
	
	VkPipelineInputAssemblyStateCreateInfo InputAssemblyState = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
	InputAssemblyState.topology = (VkPrimitiveTopology)spec.topology;
	
	VkPipelineTessellationStateCreateInfo TessellationState = {.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO};

	VkPipelineViewportStateCreateInfo ViewportState = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
	ViewportState.viewportCount = 1;
	ViewportState.scissorCount = 1;
	
	VkPipelineRasterizationStateCreateInfo RasterizationState = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
	RasterizationState.polygonMode = (VkPolygonMode)spec.fillmode;
	RasterizationState.cullMode = VK_CULL_MODE_NONE;
	RasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;
	
	VkPipelineMultisampleStateCreateInfo MultisampleState = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
	MultisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo DepthStencilState = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};

	VkPipelineColorBlendAttachmentState AttachmentBlendStates[8] = {0};
	for (uint32_t iattachment = 0; iattachment < renderpass.color_formats_length; ++iattachment) {
		AttachmentBlendStates[iattachment].blendEnable = VK_TRUE;
 		AttachmentBlendStates[iattachment].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		AttachmentBlendStates[iattachment].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		AttachmentBlendStates[iattachment].colorBlendOp = VK_BLEND_OP_ADD;
 		AttachmentBlendStates[iattachment].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		AttachmentBlendStates[iattachment].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		AttachmentBlendStates[iattachment].alphaBlendOp = VK_BLEND_OP_ADD;
		AttachmentBlendStates[iattachment].colorWriteMask = 0xf;
	}
	
	VkPipelineColorBlendStateCreateInfo ColorBlendState = {.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
	ColorBlendState.attachmentCount = renderpass.color_formats_length;
	ColorBlendState.pAttachments = AttachmentBlendStates;
	
	VkDynamicState dynamic_states[] = {
		// viewport
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	VkPipelineDynamicStateCreateInfo DynamicState = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
	DynamicState.dynamicStateCount = ARRAY_LENGTH(dynamic_states);
	DynamicState.pDynamicStates = dynamic_states;

	VkPipelineRenderingCreateInfoKHR RenderingState = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR};
	RenderingState.colorAttachmentCount = renderpass.color_formats_length;
	RenderingState.pColorAttachmentFormats = (const VkFormat*)renderpass.color_formats;
	RenderingState.depthAttachmentFormat = (VkFormat)renderpass.depth_format;

	VkGraphicsPipelineCreateInfo pipeline_info = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
	pipeline_info.pNext = &RenderingState;
	pipeline_info.stageCount = ARRAY_LENGTH(ShaderStages);
	pipeline_info.pStages = ShaderStages;
	pipeline_info.pVertexInputState = &VertexInputState;
	pipeline_info.pInputAssemblyState = &InputAssemblyState;
	pipeline_info.pTessellationState = &TessellationState;
	pipeline_info.pViewportState = &ViewportState;
	pipeline_info.pRasterizationState = &RasterizationState;
	pipeline_info.pMultisampleState = &MultisampleState;
	pipeline_info.pDepthStencilState = &DepthStencilState;
	pipeline_info.pColorBlendState = &ColorBlendState;
	pipeline_info.pDynamicState = &DynamicState;
	pipeline_info.layout = device->pipeline_layout;
	pipeline_info.renderPass = VK_NULL_HANDLE;

	VkPipeline pipeline = VK_NULL_HANDLE;
	VkResult res = vkCreateGraphicsPipelines(device->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline);
	assert(res == VK_SUCCESS);

	device->graphics_psos[handle].pipeline = pipeline;
}


// -- Buffers
void new_buffer_internal(VulkanDevice *device, uint32_t handle,uint32_t size, VkBufferCreateFlags flags, VkBufferUsageFlagBits  usage)
{	
	VkBufferCreateInfo buffer_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	buffer_info.flags = flags;
	buffer_info.size = size;
	buffer_info.usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR;
	VkBuffer buffer = VK_NULL_HANDLE;
	VkResult res = vkCreateBuffer(device->device, &buffer_info, NULL, &buffer);
	assert(res == VK_SUCCESS);
	
	VkDeviceBufferMemoryRequirements buf_requirements = {.sType = VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS};
	buf_requirements.pCreateInfo = &buffer_info;
	VkMemoryRequirements2 mem_requirements = {.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2};
	vkGetDeviceBufferMemoryRequirements(device->device, &buf_requirements, &mem_requirements);
	printf("memoryTypesBits = %u 0x%x\n", mem_requirements.memoryRequirements.memoryTypeBits, mem_requirements.memoryRequirements.memoryTypeBits);
	VkDeviceMemory memory = device->main_memory;
	void *memory_mapped = device->main_memory_mapped;
	bool main_memory_support = ((mem_requirements.memoryRequirements.memoryTypeBits >> device->main_memory_type_index) & 1) != 0;
	assert(main_memory_support);
	assert(mem_requirements.memoryRequirements.alignment <= MEMORY_ALIGNMENT);
	oa_allocation_t allocation = {0};
	uint32_t rounded_up_size = (size + MEMORY_ALIGNMENT - 1) / MEMORY_ALIGNMENT;
	int alloc_res = oa_allocate(&device->main_memory_allocator, rounded_up_size, &allocation);
	assert(alloc_res == 0);
	uint32_t real_offset = allocation.offset * MEMORY_ALIGNMENT;
	res = vkBindBufferMemory(device->device, buffer, memory, real_offset);
	assert(res == VK_SUCCESS);

	// Get GPU address
	VkBufferDeviceAddressInfoKHR address_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR};
	address_info.buffer = buffer;
	uint64_t gpu_address  = vkGetBufferDeviceAddress(device->device, &address_info);
	// Get CPU address
	void *mapped = (char*)memory_mapped + real_offset;
	
	printf("cpu: %p | gpu: %p | size: %u\n", mapped, (void*)gpu_address, size);
	
	assert(device->buffers[handle].buffer == VK_NULL_HANDLE);
	
	device->buffers[handle].allocation = allocation;
	device->buffers[handle].buffer = buffer;
	device->buffers[handle].mapped = mapped;
	device->buffers[handle].gpu_address = gpu_address;
	device->buffers[handle].size = size;
	device->buffers[handle].flags = flags;
	device->buffers[handle].usage = usage;
}

void new_index_buffer(VulkanDevice *device, uint32_t handle, uint32_t size)
{
	new_buffer_internal(device, handle, size, 0, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
}

void new_storage_buffer(VulkanDevice *device, uint32_t handle, uint32_t size)
{
	new_buffer_internal(device, handle, size, 0, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
}

void* buffer_get_mapped_pointer(VulkanDevice *device, uint32_t handle)
{
	assert(handle < ARRAY_LENGTH(device->buffers));
	assert(device->buffers[handle].mapped != NULL);
	return device->buffers[handle].mapped;
}

uint64_t buffer_get_gpu_address(VulkanDevice *device, uint32_t handle)
{
	assert(handle < ARRAY_LENGTH(device->buffers));
	assert(device->buffers[handle].gpu_address != 0);
	return device->buffers[handle].gpu_address;
}

uint32_t buffer_get_size(VulkanDevice *device, uint32_t handle)
{
	assert(handle < ARRAY_LENGTH(device->buffers));
	assert(device->buffers[handle].size != 0);
	return device->buffers[handle].size;
}


// -- Render Targets
void new_render_target(VulkanDevice *device, uint32_t handle, uint32_t width, uint32_t height, int format)
{
	bool is_depth = format == PG_FORMAT_D32_SFLOAT;

	VkImageCreateInfo image_info = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	image_info.flags = 0;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.format = format;
	image_info.extent.width = width;
	image_info.extent.height = height;
	image_info.extent.depth = 1;
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
	if (is_depth) {
		image_info.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	} else {
		image_info.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		image_info.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT; // image may be copied to swapchain
	}
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImage image = VK_NULL_HANDLE;
	VkResult res = vkCreateImage(device->device, &image_info, NULL, &image);
	assert(res == VK_SUCCESS);
	
	VkDeviceImageMemoryRequirements image_requirements = {.sType = VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS};
	image_requirements.pCreateInfo = &image_info;
	VkMemoryRequirements2 mem_requirements = {.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2};
	vkGetDeviceImageMemoryRequirements(device->device, &image_requirements, &mem_requirements);
	assert(((mem_requirements.memoryRequirements.memoryTypeBits >> device->rt_type_index) & 1) != 0); // check that our memory supports this render_target
	assert(mem_requirements.memoryRequirements.alignment <= MEMORY_ALIGNMENT);
	oa_allocation_t allocation = {0};
	uint32_t rounded_up_size = (mem_requirements.memoryRequirements.size + MEMORY_ALIGNMENT - 1) / MEMORY_ALIGNMENT;
	int alloc_res = oa_allocate(&device->rt_memory_allocator, rounded_up_size, &allocation);
	assert(alloc_res == 0);
	uint32_t real_offset = allocation.offset * MEMORY_ALIGNMENT;
	res = vkBindImageMemory(device->device, image, device->rt_memory, real_offset);
	assert(res == VK_SUCCESS);

	// create image view
	VkImageViewCreateInfo view_info = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
	view_info.image = image;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_info.format = format;
	view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	if (is_depth) {
		view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	} else {
		view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}
	view_info.subresourceRange.levelCount = 1;
	view_info.subresourceRange.layerCount = 1;
	VkImageView image_view = VK_NULL_HANDLE;
	res = vkCreateImageView(device->device, &view_info, NULL, &image_view);

	device->rts[handle].allocation = allocation;
	device->rts[handle].image = image;
	device->rts[handle].image_view = image_view;
	device->rts[handle].format = format;
	device->rts[handle].width = width;
	device->rts[handle].height = height;
}

void resize_render_target(VulkanDevice *device, uint32_t handle, uint32_t width, uint32_t height)
{
	assert(handle < ARRAY_LENGTH(device->rts));
	assert(device->rts[handle].image_view != VK_NULL_HANDLE);

	oa_free(&device->rt_memory_allocator, &device->rts[handle].allocation);
	// TODO: free VkImage and VkImageView

	new_render_target(device, handle, width, height, device->rts[handle].format);
}

void new_texture(VulkanDevice *device, uint32_t handle, uint32_t width, uint32_t height, int format, void *data, uint32_t size)
{
	VkImageCreateInfo image_info = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	image_info.flags = 0;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.format = format;
	image_info.extent.width = width;
	image_info.extent.height = height;
	image_info.extent.depth = 1;
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImage image = VK_NULL_HANDLE;
	VkResult res = vkCreateImage(device->device, &image_info, NULL, &image);
	assert(res == VK_SUCCESS);
	
	VkDeviceImageMemoryRequirements image_requirements = {.sType = VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS};
	image_requirements.pCreateInfo = &image_info;
	VkMemoryRequirements2 mem_requirements = {.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2};
	vkGetDeviceImageMemoryRequirements(device->device, &image_requirements, &mem_requirements);
	assert(((mem_requirements.memoryRequirements.memoryTypeBits >> device->main_memory_type_index) & 1) != 0); // check that our memory supports this render_target
	assert(mem_requirements.memoryRequirements.alignment <= MEMORY_ALIGNMENT);
	oa_allocation_t allocation = {0};
	uint32_t rounded_up_size = (mem_requirements.memoryRequirements.size + MEMORY_ALIGNMENT - 1) / MEMORY_ALIGNMENT;
	int alloc_res = oa_allocate(&device->main_memory_allocator, rounded_up_size, &allocation);
	assert(alloc_res == 0);
	uint32_t real_offset = allocation.offset * MEMORY_ALIGNMENT;
	res = vkBindImageMemory(device->device, image, device->main_memory, real_offset);
	assert(res == VK_SUCCESS);

	// create image view
	VkImageViewCreateInfo view_info = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
	view_info.image = image;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_info.format = format;
	view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_info.subresourceRange.levelCount = 1;
	view_info.subresourceRange.layerCount = 1;
	VkImageView image_view = VK_NULL_HANDLE;
	res = vkCreateImageView(device->device, &view_info, NULL, &image_view);

	device->textures[handle].allocation = allocation;
	device->textures[handle].image = image;
	device->textures[handle].image_view = image_view;
	device->textures[handle].format = format;
	device->textures[handle].width = width;
	device->textures[handle].height = height;

	// temp: prepare upload
	device->textures[handle].upload_offset = device->upload_buffer_offset;
	device->textures[handle].upload_size = size;
	assert(device->upload_buffer_offset + size <= device->buffers[device->upload_buffer].size);
	memcpy(device->buffers[device->upload_buffer].mapped + device->upload_buffer_offset,
	       data,
	       size);
	device->upload_buffer_offset += size;
}

// -- Rendering
static void set_image_layout(VkCommandBuffer cmd, VkImage image, VkImageLayout old_image_layout, VkImageLayout new_image_layout,
			     VkAccessFlagBits srcAccessMask, VkPipelineStageFlags src_stages,
			     VkPipelineStageFlags dst_stages)
{
	VkPipelineStageFlags DEPTH_TEST_MASK = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	bool is_depth = ((src_stages & DEPTH_TEST_MASK) != 0) || ((dst_stages & DEPTH_TEST_MASK) != 0);
	
	VkImageMemoryBarrier2 image_barrier = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
	image_barrier.srcStageMask = src_stages;
	image_barrier.srcAccessMask = srcAccessMask;
	image_barrier.dstStageMask = dst_stages;
	image_barrier.oldLayout = old_image_layout;
	image_barrier.newLayout = new_image_layout;
	image_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	image_barrier.image = image;
	image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	if (is_depth) {
		image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	}
	image_barrier.subresourceRange.levelCount = 1;
	image_barrier.subresourceRange.layerCount = 1;

	switch (new_image_layout) {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		image_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		break;

        case VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL:
		if (is_depth) {
			image_barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		}
		else {
			image_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
		}
		break;
		
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		image_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		image_barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		break;

        case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL:
		image_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		image_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		break;

        default:
		image_barrier.dstAccessMask = VK_ACCESS_NONE;
		break;
	}

	VkDependencyInfo dep_info = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
	dep_info.imageMemoryBarrierCount = 1;
	dep_info.pImageMemoryBarriers = &image_barrier;
	vkCmdPipelineBarrier2(cmd, &dep_info);
}

void begin_frame(VulkanDevice *device, VulkanFrame *frame, uint32_t *out_swapchain_w, uint32_t *out_swapchain_h)
{
	frame->iframe = device->current_frame % FRAME_COUNT;
	
	VkResult res = VK_SUCCESS;
	// -- wait for command buffer to finish
	res = vkWaitForFences(device->device, 1, &device->command_fence[frame->iframe], VK_TRUE, -1);
	assert(res == VK_SUCCESS);
	res = vkResetFences(device->device, 1, &device->command_fence[frame->iframe]);
	assert(res == VK_SUCCESS);

	// -- recreate swapchain if needed
#if defined(__linux__)
	if (g_wnd.ready_to_resize != 0) {
		g_wnd.ready_to_resize = 0;
		device->should_recreate_swapchain = 1;
	}

#endif
	if (device->should_recreate_swapchain) {
		device->should_recreate_swapchain = 0;
		create_swapchain(device, device->swapchain_last_wnd);
	}

	// -- reset command buffers
	res = vkResetCommandPool(device->device, device->command_pool[frame->iframe], 0);
	assert(res == VK_SUCCESS);

	// -- begin
	frame->cmd = device->command_buffer[frame->iframe];
	VkCommandBufferBeginInfo begin_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	res = vkBeginCommandBuffer(frame->cmd, &begin_info);
	assert(res == VK_SUCCESS);

	if (out_swapchain_w != NULL) {
		*out_swapchain_w = device->swapchain_width;
	}
	if (out_swapchain_h != NULL) {
		*out_swapchain_h = device->swapchain_height;
	}

	// temp: process uploads
	for (uint32_t itexture = 0; itexture < VK_TEXTURE_CAPACITY; ++itexture) {
		if (device->textures[itexture].upload_size > 0) {
			set_image_layout(frame->cmd, device->textures[itexture].image,
				 VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				 VK_ACCESS_NONE, VK_PIPELINE_STAGE_NONE,
				 VK_PIPELINE_STAGE_TRANSFER_BIT);

			VkBufferImageCopy region = {0};
			region.bufferOffset = device->textures[itexture].upload_offset;
			region.bufferRowLength = device->textures[itexture].width;
			region.bufferImageHeight = device->textures[itexture].height;
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.layerCount = 1;
			region.imageExtent.width = device->textures[itexture].width;
			region.imageExtent.height = device->textures[itexture].height;
			region.imageExtent.depth = 1;
			vkCmdCopyBufferToImage(frame->cmd,
					       device->buffers[device->upload_buffer].buffer,
					       device->textures[itexture].image,
					       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					       1,
					       &region);
			
			set_image_layout(frame->cmd, device->textures[itexture].image,
				 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
				 VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				 VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
		
			device->textures[itexture].upload_offset = 0;
			device->textures[itexture].upload_size = 0;
		}
	}
}

void end_frame(VulkanDevice *device, VulkanFrame *frame, uint32_t output_rt_handle)
{
	// -- get swapchain image
	uint32_t ibackbuffer = 0;
	VkResult res = vkAcquireNextImageKHR(device->device,
				    device->swapchain,
				    -1,
				    device->swapchain_acquire_semaphore[frame->iframe],
				    VK_NULL_HANDLE,
				    &ibackbuffer);
	assert(res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR);
	if (res == VK_SUBOPTIMAL_KHR)
		device->should_recreate_swapchain = 1;

	// -- prepare present
	if (output_rt_handle < ARRAY_LENGTH(device->rts) && device->rts[output_rt_handle].image != VK_NULL_HANDLE) {
		VulkanRenderTarget *output_rt = device->rts + output_rt_handle;
		
		set_image_layout(frame->cmd, output_rt->image,
				 VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				 VK_ACCESS_NONE, VK_PIPELINE_STAGE_NONE,
				 VK_PIPELINE_STAGE_TRANSFER_BIT);

		set_image_layout(frame->cmd, device->swapchain_images[ibackbuffer],
				 VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				 VK_ACCESS_NONE, VK_PIPELINE_STAGE_NONE,
				 VK_PIPELINE_STAGE_TRANSFER_BIT);

		VkImageBlit region = {0};
		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.srcSubresource.layerCount = 1;
		region.srcOffsets[1].x = output_rt->width;
		region.srcOffsets[1].y = output_rt->height;
		region.srcOffsets[1].z = 1;
		region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.dstSubresource.layerCount = 1;	
		region.dstOffsets[1].x = device->swapchain_width;
		region.dstOffsets[1].y = device->swapchain_height;
		region.dstOffsets[1].z = 1;
		vkCmdBlitImage(frame->cmd, output_rt->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			       device->swapchain_images[ibackbuffer], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			       1, &region,
			       VK_FILTER_NEAREST);
	
		set_image_layout(frame->cmd, device->swapchain_images[ibackbuffer],
				 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				 VK_ACCESS_TRANSFER_WRITE_BIT , VK_PIPELINE_STAGE_TRANSFER_BIT,
				 VK_PIPELINE_STAGE_NONE);
	} else {
		set_image_layout(frame->cmd, device->swapchain_images[ibackbuffer],
				 VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				 VK_ACCESS_NONE , VK_PIPELINE_STAGE_NONE,
				 VK_PIPELINE_STAGE_NONE);
	}

	// -- end
	res = vkEndCommandBuffer(frame->cmd);
	assert(res == VK_SUCCESS);

	// -- submit
	VkSemaphoreSubmitInfo wait_semaphore_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
	wait_semaphore_info.semaphore = device->swapchain_acquire_semaphore[frame->iframe];
	wait_semaphore_info.stageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
	VkCommandBufferSubmitInfo command_buffer_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
	command_buffer_info.commandBuffer = frame->cmd;
	VkSemaphoreSubmitInfo signal_semaphore_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
	signal_semaphore_info.semaphore = device->swapchain_present_semaphore[frame->iframe];
	signal_semaphore_info.stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkSubmitInfo2 submit_info = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
	submit_info.waitSemaphoreInfoCount = 1;
	submit_info.pWaitSemaphoreInfos = &wait_semaphore_info;
	submit_info.commandBufferInfoCount = 1;
	submit_info.pCommandBufferInfos = &command_buffer_info;
	submit_info.signalSemaphoreInfoCount = 1;
	submit_info.pSignalSemaphoreInfos = &signal_semaphore_info;
	res = vkQueueSubmit2(device->graphics_queue, 1, &submit_info, device->command_fence[frame->iframe]);
	assert(res == VK_SUCCESS);

	// -- present
	VkPresentInfoKHR present_info = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
	present_info.pNext = NULL;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &device->swapchain;
	present_info.pImageIndices = &ibackbuffer;
	present_info.pWaitSemaphores = &device->swapchain_present_semaphore[frame->iframe];
	present_info.waitSemaphoreCount = 1;
	present_info.pResults = NULL;
	res = vkQueuePresentKHR(device->graphics_queue, &present_info);
	assert(res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR);
	if (res == VK_SUBOPTIMAL_KHR)
		device->should_recreate_swapchain = 1;
}

void begin_render_pass(VulkanDevice *device, VulkanFrame *frame, VulkanRenderPass *pass, struct VulkanBeginPassInfo pass_info)
{
	VkRenderingAttachmentInfoKHR color_infos[8] = {0};
	VkRenderingAttachmentInfoKHR depth_info = {0};
	uint32_t render_width = 8 << 10;
	uint32_t render_height = 8 << 10;

	for (uint32_t icolor = 0; icolor < pass_info.color_rts_length; ++icolor) {
		assert(pass_info.color_rts[icolor] < ARRAY_LENGTH(device->rts));
		assert(device->rts[icolor].image_view != VK_NULL_HANDLE);
		VulkanRenderTarget *rt = device->rts + pass_info.color_rts[icolor];
		pass->color_rts[icolor] = rt;

		if (rt->width < render_width) {
			render_width = rt->width;
		}
		if (rt->height < render_height) {
			render_height = rt->height;
		}

		set_image_layout(frame->cmd, rt->image,
				 VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
				 VK_ACCESS_NONE, VK_PIPELINE_STAGE_NONE,
				 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		
		color_infos[icolor] = (VkRenderingAttachmentInfoKHR){
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
			.imageView = rt->image_view,
			.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
			.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = {0},
		};
	}
	pass->color_rts_length = pass_info.color_rts_length;
	
	bool has_depth = pass_info.depth_rt < ARRAY_LENGTH(device->rts) && device->rts[pass_info.depth_rt].image_view != VK_NULL_HANDLE;
	has_depth = has_depth && device->rts[pass_info.depth_rt].format == PG_FORMAT_D32_SFLOAT;
	if (has_depth) {
		VulkanRenderTarget *rt = device->rts + pass_info.depth_rt;
		pass->depth_rt = rt;
		
		if (rt->width < render_width) {
			render_width = rt->width;
		}
		if (rt->height < render_height) {
			render_height = rt->height;
		}

		set_image_layout(frame->cmd, rt->image,
				 VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
				 VK_ACCESS_NONE, VK_PIPELINE_STAGE_NONE,
				 VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);

		depth_info = (VkRenderingAttachmentInfoKHR){
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
			.imageView = rt->image_view,
			.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
			.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = {0},
		};
	}
	
	VkRenderingInfoKHR render_info  = {.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR};
	render_info.renderArea.extent.width = render_width;
	render_info.renderArea.extent.height = render_height;
	render_info.layerCount = 1;
	render_info.colorAttachmentCount = pass_info.color_rts_length;
	render_info.pColorAttachments = color_infos;
	render_info.pDepthAttachment = has_depth ? &depth_info : NULL;
	vkCmdBeginRendering(frame->cmd, &render_info);

	VkViewport viewport = {.width = render_width, .height = render_height, .maxDepth = 1.0f};
	vkCmdSetViewport(frame->cmd, 0, 1, &viewport);

	VkRect2D scissor = {.extent = {.width = render_width, .height = render_height}};
	vkCmdSetScissor(frame->cmd, 0, 1, &scissor);

	pass->frame = frame;
	pass->render_width = render_width;
	pass->render_height = render_height;
}

void end_render_pass(VulkanDevice *device, VulkanRenderPass *pass)
{
	VulkanFrame *frame = pass->frame;
	
	vkCmdEndRendering(frame->cmd);
	for (uint32_t icolor = 0; icolor < pass->color_rts_length; ++icolor) {
		set_image_layout(frame->cmd, pass->color_rts[icolor]->image,
				 VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
				 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				 VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);
	}
	if (pass->depth_rt != NULL) {
		set_image_layout(frame->cmd, pass->depth_rt->image,
				 VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
				 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				 VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				 VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);
	}
}

void vulkan_clear(VulkanDevice *device, VulkanRenderPass *pass, union VulkanClearColor const* colors, uint32_t colors_length, float depth)
{
	VulkanFrame *frame = pass->frame;

	VkClearAttachment attachments[8] = {0};
	VkClearRect rects[8] = {0};
	
	uint32_t iattachment = 0;
	for (; iattachment < colors_length; ++iattachment) {
		assert(iattachment < ARRAY_LENGTH(attachments));
		attachments[iattachment].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		attachments[iattachment].colorAttachment = iattachment;
		attachments[iattachment].clearValue.color.uint32[0] = colors[iattachment].u32[0];
		attachments[iattachment].clearValue.color.uint32[1] = colors[iattachment].u32[1];
		attachments[iattachment].clearValue.color.uint32[2] = colors[iattachment].u32[2];
		attachments[iattachment].clearValue.color.uint32[3] = colors[iattachment].u32[3];
		rects[iattachment].rect.extent.width = pass->render_width;
		rects[iattachment].rect.extent.height = pass->render_height;
		rects[iattachment].layerCount = 1;
	}
	if (pass->depth_rt != NULL) {
		assert(iattachment < ARRAY_LENGTH(attachments));
		attachments[iattachment].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		attachments[iattachment].clearValue.depthStencil.depth = depth;
		rects[iattachment].rect.extent.width = pass->render_width;
		rects[iattachment].rect.extent.height = pass->render_height;
		rects[iattachment].layerCount = 1;
	}
	
	vkCmdClearAttachments(frame->cmd,
			      iattachment,
			      attachments,
			      iattachment,
			      rects);
}

void vulkan_set_scissor(VulkanDevice *device, VulkanRenderPass *pass, struct VulkanScissor scissor)
{
	VulkanFrame *frame = pass->frame;
	VkRect2D s = {0};
	s.offset.x = scissor.x;
	s.offset.y = scissor.y;
	s.extent.width = scissor.w;
	s.extent.height = scissor.h;
	vkCmdSetScissor(frame->cmd, 0, 1, &s);

}

void vulkan_push_constants(VulkanDevice *device, VulkanRenderPass *pass, void *data, uint32_t size)
{
	VulkanFrame *frame = pass->frame;
	vkCmdPushConstants(frame->cmd,
			   device->pipeline_layout,
			   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			   0,
			   size,
			   data);
}

void vulkan_bind_graphics_pso(VulkanDevice *device, VulkanRenderPass *pass, uint32_t pso_handle)
{
	VulkanFrame *frame = pass->frame;
	assert(pso_handle < ARRAY_LENGTH(device->graphics_psos));
	assert(device->graphics_psos[pso_handle].pipeline != VK_NULL_HANDLE);
	vkCmdBindPipeline(frame->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, device->graphics_psos[pso_handle].pipeline);
}

void vulkan_bind_index_buffer(VulkanDevice *device, VulkanRenderPass *pass, uint32_t buffer_handle)
{
	VulkanFrame *frame = pass->frame;
	assert(buffer_handle < ARRAY_LENGTH(device->buffers));
	assert(device->buffers[buffer_handle].buffer != VK_NULL_HANDLE);
	vkCmdBindIndexBuffer(frame->cmd, device->buffers[buffer_handle].buffer, 0, VK_INDEX_TYPE_UINT32);
}

void vulkan_draw(VulkanDevice *device, VulkanRenderPass *pass, struct VulkanDraw draw)
{
	VulkanFrame *frame = pass->frame;
	vkCmdDrawIndexed(frame->cmd,
			 draw.index_count,
			 draw.instance_count,
			 draw.first_index,
			 draw.vertex_offset,
			 draw.first_instance);
}

void vulkan_insert_debug_label(VulkanDevice *device, VulkanRenderPass *pass, const char *label)
{
	VulkanFrame *frame = pass->frame;

	VkDebugUtilsLabelEXT label_info = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
	label_info.pLabelName = label;
	device->my_vkCmdInsertDebugUtilsLabelEXT(frame->cmd, &label_info);
}


void vulkan_bind_texture(VulkanDevice *device, VulkanFrame *frame, uint32_t texture_handle, uint32_t slot)
{
	VkDescriptorImageInfo image_info = {0};
	image_info.sampler = device->default_sampler;
	image_info.imageView = device->textures[texture_handle].image_view;
	image_info.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
	
	VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	write.dstArrayElement = slot;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &image_info;

	device->my_vkCmdPushDescriptorSetKHR(frame->cmd,
			       VK_PIPELINE_BIND_POINT_GRAPHICS,
			       device->pipeline_layout,
			       0,
			       1,
			       &write);
}
