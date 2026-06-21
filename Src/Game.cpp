#include "Game.h"
#include "Base.h"
#include "Planet.h"
#include "Resources.h"

#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

#define VOLK_IMPLEMENTATION
#include "volk/volk.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <array>
#include <print>
#include <vector>

void GameContext::InitComputePipeline() {
        VkResult res{};

        // ===== Create Compute Pipeline Layout ===== //

        VkPushConstantRange push_constant_range{
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .size       = sizeof(PlanetCreationInfo),
        };

        VkPipelineLayoutCreateInfo pipeline_layout_info{
                .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount         = 0,
                .pSetLayouts            = nullptr,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges    = &push_constant_range,
        };

        res = vkCreatePipelineLayout(vulkan_device, &pipeline_layout_info, nullptr, &compute_pipeline_layout);

        if (res != VK_SUCCESS) {
                std::println("Failed creating compute pipeline layout");
                exit(res);
        }

        // Factor out shader loading into a function which handles all this for me.

        // ===== Compute Shaders ===== //
        Slang::ComPtr<slang::IBlob>   diagnostics;
        Slang::ComPtr<slang::IModule> slang_module{ slang_session->loadModuleFromSource("MarchingCubes", "Assets/Shaders/MarchingCubes.slang", nullptr,
                                                                                        diagnostics.writeRef()) };
        // or slang_session->loadModule("compute");

        if (diagnostics) {
                std::println("Slang Error: {}", (const char*)diagnostics->getBufferPointer());
                exit(1);
        }

        Slang::ComPtr<ISlangBlob> spirv;
        slang_module->getTargetCode(0, spirv.writeRef());

        VkShaderModuleCreateInfo shader_module_info{
                .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .codeSize = spirv->getBufferSize(),
                .pCode    = (u32*)spirv->getBufferPointer(),
        };

        VkShaderModule shader_module{};
        res = vkCreateShaderModule(vulkan_device, &shader_module_info, nullptr, &shader_module);

        if (res != VK_SUCCESS) {
                std::println("Failed creating compute shader module");
                exit(res);
        }

        VkPipelineShaderStageCreateInfo shader_stage_info{
                .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = shader_module,
                .pName  = "Pass1",
        };

        // ===== Create Compute Pipeline ===== //

        VkComputePipelineCreateInfo pipeline_info{
                .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                .stage  = shader_stage_info,
                .layout = compute_pipeline_layout,
        };

        vkCreateComputePipelines(vulkan_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &compute_pipeline);

        vkDestroyShaderModule(vulkan_device, shader_module, nullptr);

        // ===== Compute Command Pool ===== //

        VkCommandPoolCreateInfo command_pool_info{
                .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = compute_queue_family,
        };

        res = vkCreateCommandPool(vulkan_device, &command_pool_info, nullptr, &compute_command_pool);

        if (res != VK_SUCCESS) {
                std::println("Failed to create compute command pool");
                exit(res);
        }

        VkCommandBufferAllocateInfo compute_command_buffer_allocate_info{
                .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool        = compute_command_pool,
                .commandBufferCount = 1,
        };

        res = vkAllocateCommandBuffers(vulkan_device, &compute_command_buffer_allocate_info, &compute_command_buffer);

        if (res != VK_SUCCESS) {
                std::println("Failed to allocate compute command buffers");
                exit(res);
        }

        // ===== Sync ===== //

        compute_fences.resize(1);

        VkFenceCreateInfo fence_info{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = 0 };

        res = vkCreateFence(vulkan_device, &fence_info, nullptr, &compute_fences[0]);

        if (res != VK_SUCCESS) {
                std::println("Failed creating fence");
                exit(res);
        }
}

void GameContext::Init() {
        VkResult res{};

        // ===== Initialise Libraries =====

        SDL_Init(SDL_INIT_VIDEO);
        SDL_Vulkan_LoadLibrary(NULL);
        volkInitialize();

        // ===== Vulkan Instance =====

        VkApplicationInfo  app_info{ .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = "Space Game", .apiVersion = VK_API_VERSION_1_3 };
        u32                instance_extension_count{ 0 };
        char const* const* instance_extensions{ SDL_Vulkan_GetInstanceExtensions(&instance_extension_count) };

        std::vector<const char*> instance_layers;

#ifdef DEBUG
        instance_layers.push_back("VK_LAYER_KHRONOS_validation");
        instance_layers.push_back("VK_LAYER_LUNARG_monitor");

        std::println("Using Validation Layers");
#endif

        VkInstanceCreateInfo instance_info{
                .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                .pApplicationInfo        = &app_info,
                .enabledLayerCount       = (u32)instance_layers.size(),
                .ppEnabledLayerNames     = instance_layers.data(),
                .enabledExtensionCount   = instance_extension_count,
                .ppEnabledExtensionNames = instance_extensions,
        };

        res = vkCreateInstance(&instance_info, nullptr, &vulkan_instance);

        if (res != VK_SUCCESS) {
                std::println("Instance Failed Creating: {}", (int)res);
                exit(res);
        }

        // ===== Load Instance Functions =====

        volkLoadInstance(vulkan_instance);

        // ===== Vulkan Physical Device =====

        u32 device_count{ 0 };
        res = vkEnumeratePhysicalDevices(vulkan_instance, &device_count, nullptr);
        if (res != VK_SUCCESS) {
                if (res == VK_INCOMPLETE) {
                        std::println("[WARN] not all devices were returned from vkEnumeratePhysicalDevices");
                } else {
                        std::println("Physical device enumeration failed");
                        exit(res);
                }
        }
        std::vector<VkPhysicalDevice> physical_devices(device_count);
        res = vkEnumeratePhysicalDevices(vulkan_instance, &device_count, physical_devices.data());
        if (res != VK_SUCCESS) {
                if (res == VK_INCOMPLETE) {
                        std::println("[WARN] not all devices were returned from vkEnumeratePhysicalDevices");
                } else {
                        std::println("Physical device enumeration failed");
                        exit(res);
                }
        }

        u32 physical_device_index{ 0 };
        // TODO: Either check for best device, or let user select one.

        VkPhysicalDevice physical_device = physical_devices[physical_device_index];

        VkPhysicalDeviceProperties2 physical_device_properties{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
        vkGetPhysicalDeviceProperties2(physical_device, &physical_device_properties);

        std::println("Selected Physical Device: {}", physical_device_properties.properties.deviceName);

        // NOTE: need to use this to check if we are within limits.
        VkPhysicalDeviceLimits device_limits = physical_device_properties.properties.limits;

        std::println("Limits push constant size: {}", device_limits.maxPushConstantsSize);

        // ===== Vulkan Queues =====

        u32 queue_family_count{ 0 };
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());

        graphics_queue_family = u32_max;

        for (size_t i = 0; i < queue_families.size(); i++) {
                if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                        graphics_queue_family = (u32)i;
                        break;
                }
        }

        if (graphics_queue_family == u32_max) {
                std::println("Failed to find a graphics queue.");
                exit(1);
        }

        if (!SDL_Vulkan_GetPresentationSupport(vulkan_instance, physical_device, graphics_queue_family)) {
                std::println("Graphics queue does not support presentation");
                exit(1);
        }

        compute_queue_family = u32_max;

        for (size_t i = 0; i < queue_families.size(); i++) {
                if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                        compute_queue_family = (u32)i;

                        // We prefer a seperate queue to the graphics.
                        if (compute_queue_family != graphics_queue_family) break;
                }
        }

        if (compute_queue_family == u32_max) {
                std::println("Failed to find a compute queue.");
                exit(1);
        }

        transfer_queue_family = u32_max;

        for (size_t i = 0; i < queue_families.size(); i++) {
                if (queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
                        transfer_queue_family = (u32)i;

                        // We prefer a seperate queue to the graphics and compute.
                        if (transfer_queue_family != graphics_queue_family and transfer_queue_family != compute_queue_family) break;
                }
        }

        if (transfer_queue_family == u32_max) {
                std::println("Failed to find a compute queue.");
                exit(1);
        }

        // ===== Vulkan Logical Device =====

        std::println("Graphics Queue: {}", graphics_queue_family);
        std::println("Compute  Queue: {} ", compute_queue_family);
        std::println("Transfer Queue: {}", transfer_queue_family);

        const float queue_priorities{ 1.0f };

        VkDeviceQueueCreateInfo graphics_queue_info{
                .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = graphics_queue_family,
                .queueCount       = 1,
                .pQueuePriorities = &queue_priorities,
        };

        VkDeviceQueueCreateInfo compute_queue_info{
                .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = compute_queue_family,
                .queueCount       = 1,
                .pQueuePriorities = &queue_priorities,
        };

        VkDeviceQueueCreateInfo transfer_queue_info{
                .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = transfer_queue_family,
                .queueCount       = 1,
                .pQueuePriorities = &queue_priorities,
        };

        VkDeviceQueueCreateInfo queue_infos[3] = {
                graphics_queue_info,
                compute_queue_info,
                transfer_queue_info,
        };

        VkPhysicalDeviceVulkan12Features enabled_vk12_features{
                .sType                                     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
                .descriptorIndexing                        = true,
                .shaderSampledImageArrayNonUniformIndexing = true,
                .descriptorBindingVariableDescriptorCount  = true,
                .runtimeDescriptorArray                    = true,
                .timelineSemaphore                         = true,
                .bufferDeviceAddress                       = true,
        };

        VkPhysicalDeviceVulkan13Features enabled_vk13_features{
                .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
                .pNext            = &enabled_vk12_features,
                .synchronization2 = true,
                .dynamicRendering = true,
        };

        const std::vector<const char*> device_extensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        const VkPhysicalDeviceFeatures enabled_vk10_features{ .samplerAnisotropy = VK_TRUE };

        VkDeviceCreateInfo logical_device_info{
                .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                .pNext                   = &enabled_vk13_features,
                .queueCreateInfoCount    = 3,
                .pQueueCreateInfos       = queue_infos,
                .enabledExtensionCount   = (u32)device_extensions.size(),
                .ppEnabledExtensionNames = device_extensions.data(),
                .pEnabledFeatures        = &enabled_vk10_features,
        };

        res = vkCreateDevice(physical_device, &logical_device_info, nullptr, &vulkan_device);

        if (res != VK_SUCCESS) {
                std::println("Failed creating logical device");
                exit(res);
        }

        vkGetDeviceQueue(vulkan_device, graphics_queue_family, 0, &graphics_queue);
        vkGetDeviceQueue(vulkan_device, compute_queue_family, 0, &compute_queue);

        // ===== VMA Allocator =====

        VmaVulkanFunctions vulkan_functions{
                .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
                .vkGetDeviceProcAddr   = vkGetDeviceProcAddr,
                .vkCreateImage         = vkCreateImage,
        };

        VmaAllocatorCreateInfo allocator_info{
                .flags            = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
                .physicalDevice   = physical_device,
                .device           = vulkan_device,
                .pVulkanFunctions = &vulkan_functions,
                .instance         = vulkan_instance,
        };

        res = vmaCreateAllocator(&allocator_info, &vulkan_allocator);
        if (res != VK_SUCCESS) {
                std::println("Failed creating vma allocator");
                exit(res);
        }

        // ===== Window =====

        window.window = SDL_CreateWindow("Space Game", 1'920, 1'080, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

        if (!window.window) {
                std::println("Window failed to create");
                exit(1);
        }

        SDL_GetWindowSize(window.window, &window.width, &window.height);

        // ===== Vulkan Surface =====

        bool surface_created = SDL_Vulkan_CreateSurface(window.window, vulkan_instance, nullptr, &vulkan_surface);

        if (!surface_created) {
                std::println("Surface creation failed");
                exit(1);
        }

        VkSurfaceCapabilitiesKHR surface_capabilities{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, vulkan_surface, &surface_capabilities);

        // ===== Swapchain =====

        VkExtent2D swapchain_extent{ surface_capabilities.currentExtent };
        if (surface_capabilities.currentExtent.width == 0xFF'FF'FF'FF) { // This is a wayland thing?
                swapchain_extent = { .width = u32(window.width), .height = u32(window.height) };
        }

        const VkFormat           image_format{ VK_FORMAT_B8G8R8A8_SRGB };
        VkSwapchainCreateInfoKHR swapchain_info{
                .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                .surface          = vulkan_surface,
                .minImageCount    = surface_capabilities.minImageCount,
                .imageFormat      = image_format,
                .imageColorSpace  = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
                .imageExtent      = swapchain_extent,
                .imageArrayLayers = 1,
                .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                .preTransform     = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
                .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                .presentMode      = VK_PRESENT_MODE_FIFO_KHR, // TODO: this is guarateed to be available, might want to allow other settings.
        };

        res = vkCreateSwapchainKHR(vulkan_device, &swapchain_info, nullptr, &vulkan_swapchain);

        if (res != VK_SUCCESS) {
                std::println("Failed creating swapchain");
                exit(1);
        }

        // ===== Get Swapchain Images =====

        u32 image_count{ 0 };
        res = vkGetSwapchainImagesKHR(vulkan_device, vulkan_swapchain, &image_count, nullptr);
        if (res != VK_SUCCESS) {
                std::println("Failed getting swapchain images");
                exit(1);
        }
        swapchain_images.resize(image_count);
        res = vkGetSwapchainImagesKHR(vulkan_device, vulkan_swapchain, &image_count, swapchain_images.data());
        if (res != VK_SUCCESS) {
                std::println("Failed getting swapchain images");
                exit(1);
        }

        // ===== Create Swapchain Image Views =====

        swapchain_image_views.resize(image_count);

        for (u32 i = 0; i < image_count; i++) {
                VkImageViewCreateInfo image_view_info{
                        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                        .image    = swapchain_images[i],
                        .viewType = VK_IMAGE_VIEW_TYPE_2D,
                        .format   = image_format,
                        .subresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 },
                };

                res = vkCreateImageView(vulkan_device, &image_view_info, nullptr, &swapchain_image_views[i]);
                if (res != VK_SUCCESS) {
                        std::println("Failed creating swapchain image view");
                        exit(res);
                }
        }

        // ===== Depth Image =====

        std::vector<VkFormat> depth_format_list{ VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT }; // at least one is guaranteed

        VkFormat depth_format{ VK_FORMAT_UNDEFINED };
        for (VkFormat format : depth_format_list) {
                VkFormatProperties2 format_properties{ .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2 };
                vkGetPhysicalDeviceFormatProperties2(physical_device, format, &format_properties);
                if (format_properties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                        depth_format = format;
                        break;
                }
        }

        if (depth_format == VK_FORMAT_UNDEFINED) {
                std::println("Failed finding supported depth format");
                exit(1);
        }

        VkImageCreateInfo depth_image_info{
                .sType     = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = VK_IMAGE_TYPE_2D,
                .format    = depth_format,
                .extent{
                        .width  = (u32)window.width,
                        .height = (u32)window.height,
                        .depth  = 1,
                },
                .mipLevels     = 1,
                .arrayLayers   = 1,
                .samples       = VK_SAMPLE_COUNT_1_BIT,
                .tiling        = VK_IMAGE_TILING_OPTIMAL,
                .usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        VmaAllocationCreateInfo depth_allocation_info{ .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, .usage = VMA_MEMORY_USAGE_AUTO };
        res = vmaCreateImage(vulkan_allocator, &depth_image_info, &depth_allocation_info, &depth_image, &depth_allocation, nullptr);

        if (res != VK_SUCCESS) {
                std::println("Failed depth image allocation");
                exit(res);
        }

        VkImageViewCreateInfo depth_view_info{
                .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image    = depth_image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format   = depth_format,
                .subresourceRange{
                        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                        .levelCount = 1,
                        .layerCount = 1,
                },
        };

        res = vkCreateImageView(vulkan_device, &depth_view_info, nullptr, &depth_image_view);

        if (res != VK_SUCCESS) {
                std::println("Failed creating depth image view");
                exit(res);
        }

        // ===== Shader Data Buffers =====

        // NOTE: Why are these not just in 1 buffer? this way is easier, but also means were doing more allocations?
        for (u32 i = 0; i < max_frames_in_flight; i++) {
                VmaAllocationCreateFlags allocation_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                                            VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

                draw_data[i] = CreateGPUBuffer(vulkan_device, vulkan_allocator, sizeof(ShaderDrawData) * max_draw_count, graphics_queue_family,
                                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, allocation_flags);

                VkBufferDeviceAddressInfo uniform_buffer_address_info{
                        .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                        .buffer = draw_data[i].buffer,
                };

                draw_data[i].device_address = vkGetBufferDeviceAddress(vulkan_device, &uniform_buffer_address_info);
        }

        // ===== Synchronization Objects =====

        VkSemaphoreCreateInfo semaphore_info{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

        VkFenceCreateInfo fence_info{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };

        for (u32 i = 0; i < max_frames_in_flight; i++) {
                res = vkCreateSemaphore(vulkan_device, &semaphore_info, nullptr, &present_semaphores[i]);

                if (res != VK_SUCCESS) {
                        std::println("Failed creating present semephore");
                        exit(res);
                }

                res = vkCreateFence(vulkan_device, &fence_info, nullptr, &fences[i]);

                if (res != VK_SUCCESS) {
                        std::println("Failed creating fence");
                        exit(res);
                }
        }

        render_semaphores.resize(swapchain_images.size());

        for (size_t i = 0; i < render_semaphores.size(); i++) {
                res = vkCreateSemaphore(vulkan_device, &semaphore_info, nullptr, &render_semaphores[i]);
        }

        // ===== Command Pool =====

        VkCommandPoolCreateInfo command_pool_info{
                .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = graphics_queue_family,
        };

        res = vkCreateCommandPool(vulkan_device, &command_pool_info, nullptr, &graphics_command_pool);

        if (res != VK_SUCCESS) {
                std::println("Failed to create command pool");
                exit(res);
        }

        VkCommandBufferAllocateInfo graphics_command_buffer_allocate_info{
                .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool        = graphics_command_pool,
                .commandBufferCount = max_frames_in_flight,
        };

        res = vkAllocateCommandBuffers(vulkan_device, &graphics_command_buffer_allocate_info, &graphics_command_buffers[0]);

        if (res != VK_SUCCESS) {
                std::println("Failed to allocate command buffers");
                exit(res);
        }

        // ===== Descriptor =====

        VkDescriptorBindingFlags descriptor_variable_flag{ VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT };

        VkDescriptorSetLayoutBindingFlagsCreateInfo descriptor_binding_flags{
                .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .bindingCount  = 1,
                .pBindingFlags = &descriptor_variable_flag,
        };

        VkDescriptorSetLayoutBinding descriptor_layout_binding_texture{
                .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 100, // Arbitrary, what is this?
                .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
        };

        VkDescriptorSetLayoutCreateInfo descriptor_layout_texture_info{
                .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext        = &descriptor_binding_flags,
                .bindingCount = 1,
                .pBindings    = &descriptor_layout_binding_texture,
        };

        res = vkCreateDescriptorSetLayout(vulkan_device, &descriptor_layout_texture_info, nullptr, &descriptor_set_layout);

        if (res != VK_SUCCESS) {
                std::println("Failed creating descriptor set layout");
                exit(res);
        }

        VkDescriptorPoolSize pool_size{
                .type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 100, // NOTE: Why?
        };

        VkDescriptorPoolCreateInfo pool_info{
                .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .maxSets       = 1,
                .poolSizeCount = 1,
                .pPoolSizes    = &pool_size,
        };

        res = vkCreateDescriptorPool(vulkan_device, &pool_info, nullptr, &descriptor_pool);

        if (res != VK_SUCCESS) {
                std::println("Failed creating descriptor pool");
                exit(res);
        }

        u32                                                variable_descriptor_count = 100;
        VkDescriptorSetVariableDescriptorCountAllocateInfo variable_descriptor_count_allocation_info{
                .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,
                .descriptorSetCount = 1,
                .pDescriptorCounts  = &variable_descriptor_count,
        };

        VkDescriptorSetAllocateInfo texture_descriptor_set_allocation_info{
                .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .pNext              = &variable_descriptor_count_allocation_info,
                .descriptorPool     = descriptor_pool,
                .descriptorSetCount = 1,
                .pSetLayouts        = &descriptor_set_layout,
        };

        res = vkAllocateDescriptorSets(vulkan_device, &texture_descriptor_set_allocation_info, &descriptor_set);

        if (res != VK_SUCCESS) {
                std::println("Failed allocating descriptor sets");
                exit(res);
        }

        // ===== Pipeline Layout ===== //

        VkPushConstantRange push_constant_range{
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .size       = sizeof(VkDeviceAddress),
        };

        VkPipelineLayoutCreateInfo pipeline_layout_info{
                .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount         = 1,
                .pSetLayouts            = &descriptor_set_layout,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges    = &push_constant_range,
        };

        res = vkCreatePipelineLayout(vulkan_device, &pipeline_layout_info, nullptr, &pipeline_layout);

        if (res != VK_SUCCESS) {
                std::println("Failed creating pipeline layout");
                exit(res);
        }

        // ===== Initialise Slang ===== //
        slang::createGlobalSession(slang_global_session.writeRef());

        // ===== Create Slang Session ===== //

        auto slang_targets{ std::to_array<slang::TargetDesc>({
                {
                        .format  = SLANG_SPIRV,
                        .profile = slang_global_session->findProfile("spirv_1_4"),
                },
        }) };
        auto slang_options{ std::to_array<slang::CompilerOptionEntry>({ {
                slang::CompilerOptionName::EmitSpirvDirectly,
                { slang::CompilerOptionValueKind::Int, 1 },
        } }) };

        const char* shader_search_paths[] = { "Assets/Shaders/" };

        slang::SessionDesc slang_session_desc{
                .targets                  = slang_targets.data(),
                .targetCount              = SlangInt(slang_targets.size()),
                .defaultMatrixLayoutMode  = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
                .searchPaths              = shader_search_paths,
                .searchPathCount          = 1,
                .compilerOptionEntries    = slang_options.data(),
                .compilerOptionEntryCount = u32(slang_options.size()),
        };

        // Slang::ComPtr<slang::ISession> slang_session; // Why would we want to create multiple sessions?
        slang_global_session->createSession(slang_session_desc, slang_session.writeRef());

        // ===== Load Shaders ===== //
        Slang::ComPtr<slang::IBlob>   diagnostics;
        Slang::ComPtr<slang::IModule> slang_module{ slang_session->loadModuleFromSource("base", "Assets/Shaders/shader.slang", nullptr,
                                                                                        diagnostics.writeRef()) };
        // or slang_session->loadModule("compute");

        if (diagnostics) {
                std::println("Slang Error: {}", (const char*)diagnostics->getBufferPointer());
                exit(1);
        }


        Slang::ComPtr<ISlangBlob> spirv;
        slang_module->getTargetCode(0, spirv.writeRef());

        VkShaderModuleCreateInfo shader_module_info{
                .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .codeSize = spirv->getBufferSize(),
                .pCode    = (u32*)spirv->getBufferPointer(),
        };

        VkShaderModule shader_module{};
        res = vkCreateShaderModule(vulkan_device, &shader_module_info, nullptr, &shader_module);

        if (res != VK_SUCCESS) {
                std::println("Failed creating shader module");
                exit(res);
        }

        // ===== Pipeline ===== //

        std::vector<VkPipelineShaderStageCreateInfo> shader_stages{
                { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = shader_module, .pName = "main" },
                { .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                  .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
                  .module = shader_module,
                  .pName  = "main" }
        };

        VkVertexInputBindingDescription vertex_binding{
                .binding   = 0,
                .stride    = sizeof(Vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };

        std::vector<VkVertexInputAttributeDescription> vertex_attributes{
                { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT },
                { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, normal) },
                { .location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, uv) },
        };

        VkPipelineVertexInputStateCreateInfo vertex_input_state{
                .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .vertexBindingDescriptionCount   = 1,
                .pVertexBindingDescriptions      = &vertex_binding,
                .vertexAttributeDescriptionCount = (u32)vertex_attributes.size(),
                .pVertexAttributeDescriptions    = vertex_attributes.data(),
        };

        VkPipelineInputAssemblyStateCreateInfo input_assembly_state{
                .sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        };

        std::vector<VkDynamicState> dynamic_states{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

        VkPipelineDynamicStateCreateInfo dynamic_state{
                .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                .dynamicStateCount = (u32)dynamic_states.size(),
                .pDynamicStates    = dynamic_states.data(),
        };

        VkPipelineViewportStateCreateInfo viewport_state{
                .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                .viewportCount = 1,
                .scissorCount  = 1,
        };

        VkPipelineRasterizationStateCreateInfo rasterization_state{
                .sType     = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .lineWidth = 1.0f,
        };

        VkPipelineMultisampleStateCreateInfo multisample_state{
                .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        };

        VkPipelineDepthStencilStateCreateInfo depth_stencil_state{
                .sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                .depthTestEnable  = VK_TRUE,
                .depthWriteEnable = VK_TRUE,
                .depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL,
        };

        VkPipelineColorBlendAttachmentState blend_attachment{ .colorWriteMask = 0xF };
        VkPipelineColorBlendStateCreateInfo blend_state{
                .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .attachmentCount = 1,
                .pAttachments    = &blend_attachment,
        };

        VkPipelineRenderingCreateInfo rendering_info{
                .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                .colorAttachmentCount    = 1,
                .pColorAttachmentFormats = &image_format,
                .depthAttachmentFormat   = depth_format,
        };

        VkGraphicsPipelineCreateInfo pipeline_info{
                .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .pNext               = &rendering_info,
                .stageCount          = 2,
                .pStages             = shader_stages.data(),
                .pVertexInputState   = &vertex_input_state,
                .pInputAssemblyState = &input_assembly_state,
                .pViewportState      = &viewport_state,
                .pRasterizationState = &rasterization_state,
                .pMultisampleState   = &multisample_state,
                .pDepthStencilState  = &depth_stencil_state,
                .pColorBlendState    = &blend_state,
                .pDynamicState       = &dynamic_state,
                .layout              = pipeline_layout,
        };

        res = vkCreateGraphicsPipelines(vulkan_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline);

        if (res != VK_SUCCESS) {
                std::println("Failed creating graphics pipeline");
                exit(res);
        }

        vkDestroyShaderModule(vulkan_device, shader_module, nullptr);

        InitComputePipeline();
        transfer_engine.Init(vulkan_device, transfer_queue_family, vulkan_allocator);

        // ===== Input ===== //

        SDL_SetWindowRelativeMouseMode(window.window, true);

        input_system.Init(&window);
}

void GameContext::AddTexture(const Texture& texture, u32 index) {
        // ===== Update Descriptor ===== //

        VkDescriptorImageInfo texture_descriptor{};

        texture_descriptor.sampler     = texture.sampler;
        texture_descriptor.imageView   = texture.image_view;
        texture_descriptor.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write_descriptor_set{
                .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet          = descriptor_set,
                .dstBinding      = 0,
                .dstArrayElement = index,
                .descriptorCount = (u32)1,
                .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo      = &texture_descriptor,
        };

        // This actually updates, how expensive is this? Can we do it every texture loaded, or better to queue?
        vkUpdateDescriptorSets(vulkan_device, 1, &write_descriptor_set, 0, nullptr);
}

void GameContext::Shutdown() {
        vkDeviceWaitIdle(vulkan_device);

        // ===== Destroy Models ===== //

        for (u64 i = 0; i < models.size(); i++) {
                Model& model = models[i];
                model.Destroy(*this);
        }

        // ===== Input ===== //

        input_system.Shutdown();

        // ===== Transfer ===== //

        transfer_engine.Shutdown();

        // ===== Compute ===== //

        vkDestroyCommandPool(vulkan_device, compute_command_pool, nullptr);
        vkDestroyFence(vulkan_device, compute_fences[0], nullptr);

        vkDestroyPipelineLayout(vulkan_device, compute_pipeline_layout, nullptr);
        vkDestroyPipeline(vulkan_device, compute_pipeline, nullptr);
        vkDestroyDescriptorSetLayout(vulkan_device, compute_descriptor_set_layout, nullptr);
        vkDestroyDescriptorPool(vulkan_device, compute_descriptor_pool, nullptr);

        // ===== Graphics + Vulkan ===== //

        for (u32 i = 0; i < max_frames_in_flight; i++) {
                vkDestroyFence(vulkan_device, fences[i], nullptr);
                vkDestroySemaphore(vulkan_device, present_semaphores[i], nullptr);
                DestroyGPUBuffer(vulkan_device, draw_data[i], vulkan_allocator);
        }

        for (size_t i = 0; i < render_semaphores.size(); i++) {
                vkDestroySemaphore(vulkan_device, render_semaphores[i], nullptr);
        }

        vkDestroyImageView(vulkan_device, depth_image_view, nullptr);
        vmaDestroyImage(vulkan_allocator, depth_image, depth_allocation);

        for (u32 i = 0; i < swapchain_image_views.size(); i++) {
                vkDestroyImageView(vulkan_device, swapchain_image_views[i], nullptr);
        }

        vkDestroyDescriptorSetLayout(vulkan_device, descriptor_set_layout, nullptr);
        vkDestroyDescriptorPool(vulkan_device, descriptor_pool, nullptr);
        vkDestroyPipelineLayout(vulkan_device, pipeline_layout, nullptr);
        vkDestroyPipeline(vulkan_device, pipeline, nullptr);
        vkDestroyCommandPool(vulkan_device, graphics_command_pool, nullptr);

        vkDestroySwapchainKHR(vulkan_device, vulkan_swapchain, nullptr);
        vkDestroySurfaceKHR(vulkan_instance, vulkan_surface, nullptr);

        // vmaDestroyAllocator(vulkan_allocator);
        vkDestroyDevice(vulkan_device, nullptr);
        vkDestroyInstance(vulkan_instance, nullptr);

        SDL_DestroyWindow(window.window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        SDL_Quit();
}

void GameContext::Render(const Camera& camera) {
        VkResult res{};
        res = vkWaitForFences(vulkan_device, 1, &fences[frame_index], true, u64_max);

        if (res != VK_SUCCESS) {
                std::println("Failed waiting on fence for rendering: {}", (i32)res);
                exit(res);
        }

        res = vkResetFences(vulkan_device, 1, &fences[frame_index]);
        frames_submitted++;

        if (res != VK_SUCCESS) {
                std::println("Failed resetting fences");
                exit(res);
        }

        // TODO: Check result
        res = vkAcquireNextImageKHR(vulkan_device, vulkan_swapchain, u64_max, present_semaphores[frame_index], VK_NULL_HANDLE, &image_index);

        if (res != VK_SUCCESS) {
                if (res != VK_SUBOPTIMAL_KHR) {
                        std::println("Failed acquiring swapchain images");
                        exit(res);
                }

                swapchain_needs_resizing = true;
                std::println("Resize?");
        }


        // ===== Shader Data =====

        VkCommandBuffer command_buffer = graphics_command_buffers[frame_index];

        res = vkResetCommandBuffer(command_buffer, 0);

        if (res != VK_SUCCESS) {
                std::println("Failed resetting command buffer");
                exit(res);
        }

        VkCommandBufferBeginInfo command_buffer_begin_info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        res = vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);

        if (res != VK_SUCCESS) {
                std::println("Failed begining command buffer for rendering");
                exit(res);
        }

        std::array<VkImageMemoryBarrier2, 2> output_barriers{
                VkImageMemoryBarrier2{
                        .sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                        .srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                        .srcAccessMask = 0,
                        .dstStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                        .oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED,
                        .newLayout     = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                        .image         = swapchain_images[image_index],
                        .subresourceRange{
                                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                .levelCount = 1,
                                .layerCount = 1,
                        },
                },
                VkImageMemoryBarrier2{
                        .sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                        .srcStageMask  = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                        .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        .dstStageMask  = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                        .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        .oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED,
                        .newLayout     = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                        .image         = depth_image,
                        .subresourceRange{
                                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
                                .levelCount = 1,
                                .layerCount = 1,
                        },
                },
        };

        VkDependencyInfo barrier_dependency_info{
                .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .imageMemoryBarrierCount = 2,
                .pImageMemoryBarriers    = output_barriers.data(),
        };

        vkCmdPipelineBarrier2(command_buffer, &barrier_dependency_info);


        VkRenderingAttachmentInfo color_attachment_info{
                .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView   = swapchain_image_views[image_index],
                .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue{
                        .color = { .float32{ 0.7f, 0.1f, 0.9f, 1.0f } },
                },
        };

        VkRenderingAttachmentInfo depth_attachment_info{
                .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView   = depth_image_view,
                .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp     = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .clearValue{
                        .depthStencil{
                                .depth   = 1.0f,
                                .stencil = 0,
                        },
                },
        };

        VkRenderingInfo rendering_info{
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .renderArea{
                        .extent{
                                .width  = (u32)window.width,
                                .height = (u32)window.height,
                        },
                },
                .layerCount           = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments    = &color_attachment_info,
                .pDepthAttachment     = &depth_attachment_info,
        };

        vkCmdBeginRendering(command_buffer, &rendering_info);

        VkViewport viewport{
                .width    = (float)window.width,
                .height   = (float)window.height,
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
        };

        vkCmdSetViewport(command_buffer, 0, 1, &viewport);

        VkRect2D scissor{
                .extent{
                        .width  = (u32)window.width,
                        .height = (u32)window.height,
                },
        };

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);

        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);

        // Draw Models
        assert(max_draw_count > models.size());
        for (u32 i = 0; i < models.size(); i++) {

                // All below can be made into model member function
                Model& model = models[i];

                if (model.meshes[0].index_count == 0) continue;

                shader_draw_data.projection = camera.GetProjectionMatrix();
                shader_draw_data.view       = camera.GetViewMatrix();

                // TODO: Rotation
                shader_draw_data.model = glm::translate(Mat4(1.0f), model.transform.position);
                shader_draw_data.model = glm::scale(shader_draw_data.model, model.transform.scale);

                u64 draw_data_offset = sizeof(ShaderDrawData) * i;
                memcpy((u8*)draw_data[frame_index].allocation_info.pMappedData + draw_data_offset, &shader_draw_data, sizeof(ShaderDrawData));

                VkDeviceSize vertex_offset{ 0 };
                vkCmdBindVertexBuffers(command_buffer, 0, 1, &model.meshes[0].buffer.buffer, &vertex_offset);

                vkCmdBindIndexBuffer(command_buffer, model.meshes[0].buffer.buffer, model.meshes[0].vertices_size, VK_INDEX_TYPE_UINT32);

                // I can just store a vector for each frame and clear when its finished, push back, pass pointer, and use other next vector next frame.
                u64 device_address = draw_data[frame_index].device_address + draw_data_offset;
                vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VkDeviceAddress), &device_address);

                vkCmdDrawIndexed(command_buffer, (u32)model.meshes[0].index_count, 1, 0, 0, 0);
        }

        vkCmdEndRendering(command_buffer);

        VkImageMemoryBarrier2 present_barrier{
                .sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstAccessMask = 0,
                .oldLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .image         = swapchain_images[image_index],
                .subresourceRange{
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .levelCount = 1,
                        .layerCount = 1,
                },
        };

        VkDependencyInfo barrier_present_dependency_info{
                .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers    = &present_barrier,
        };

        vkCmdPipelineBarrier2(command_buffer, &barrier_present_dependency_info);
        res = vkEndCommandBuffer(command_buffer);

        if (res != VK_SUCCESS) {
                std::println("Failed ending command buffer");
                exit(res);
        }

        // ===== Submit =====

        VkPipelineStageFlags wait_stages = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submit_info{
                .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .waitSemaphoreCount   = 1,
                .pWaitSemaphores      = &present_semaphores[frame_index],
                .pWaitDstStageMask    = &wait_stages,
                .commandBufferCount   = 1,
                .pCommandBuffers      = &command_buffer,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores    = &render_semaphores[image_index],
        };

        res = vkQueueSubmit(graphics_queue, 1, &submit_info, fences[frame_index]);

        if (res != VK_SUCCESS) {
                std::println("Failed to submit queue: {}", (i32)res);
                exit(res);
        }

        frame_index = (frame_index + 1) % max_frames_in_flight;

        // ===== Present =====

        VkPresentInfoKHR present_info{
                .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores    = &render_semaphores[image_index],
                .swapchainCount     = 1,
                .pSwapchains        = &vulkan_swapchain,
                .pImageIndices      = &image_index,
        };

        res = vkQueuePresentKHR(graphics_queue, &present_info);

        if (res != VK_SUCCESS) {
                if (res == VK_SUBOPTIMAL_KHR) {
                        std::println("Warning: should resize");
                } else {
                        std::println("Failed presenting: {}", (int)res);
                        exit(res);
                }
        }
}

// ===== GAME ===== //

void Game::Init() {
        game_context.Init();

        forward_action = game_context.input_system.RegisterAction("Forward", ActionType::Scalar);
        forward_action->AddKey(KeyCode::Up);
        forward_action->AddKey(KeyCode::W);

        back_action = game_context.input_system.RegisterAction("Backward", ActionType::Scalar);
        back_action->AddKey(KeyCode::Down);
        back_action->AddKey(KeyCode::S);

        right_action = game_context.input_system.RegisterAction("Right", ActionType::Scalar);
        right_action->AddKey(KeyCode::Right);
        right_action->AddKey(KeyCode::D);

        left_action = game_context.input_system.RegisterAction("Left", ActionType::Scalar);
        left_action->AddKey(KeyCode::Left);
        left_action->AddKey(KeyCode::A);


        camera_controller.Init(game_context, camera.game_object);

        // ===== Planet ===== //

        planet.Init(&game_context, &camera.game_object);

        // ===== Mesh Data =====

        // Skysphere
        {
                Model model;
                model.LoadFromOBJ(game_context, "Assets/Models/SkySphere.obj");
                game_context.models.push_back(model);
        }

        // test mesh
        {
                Model model;
                model.LoadFromOBJ(game_context, "Assets/Models/test.obj");
                game_context.models.push_back(model);
        }

        // ===== Textures ===== //

        skymap.Load(game_context.vulkan_device, game_context.graphics_command_pool, game_context.graphics_queue, "Assets/SkyMaps/SpaceLDR.ktx",
                    game_context.vulkan_allocator);
        game_context.AddTexture(skymap, 0);
}

void Game::Shutdown() {
        vkDeviceWaitIdle(game_context.vulkan_device);

        skymap.Destroy(game_context);
        planet.Shutdown();
        camera_controller.Shutdown();

        game_context.Shutdown();
}

void Game::Update(float delta_time) {
        camera.Update(game_context);
        camera_controller.Update(delta_time);

        game_context.models[0].transform.position = camera.game_object.position;

        planet.Update();
}

void Game::Run() {
        u64  last_time{ SDL_GetTicks() };
        bool running = true;

        u64 frame_count = 0;
        float frame_time = 0;

        while (running) {
                // ===== Delta Time ===== //

                float delta_time = float(SDL_GetTicks() - last_time) / 1000.0f;
                last_time        = SDL_GetTicks();

                frame_time += delta_time;
                frame_count++;
                if (frame_time >= 1.0f) {
                        std::println("FPS: {}", frame_count);
                        frame_count = 0;
                        frame_time -= 1.0f;
                }

                // ===== Input ===== //

                running = game_context.input_system.Update();

                // ===== Gameplay Update ===== //

                Update(delta_time);

                // ===== Render ===== //

                game_context.Render(camera);
        }
}
