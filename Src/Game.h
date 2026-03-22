#pragma once

#include "vulkan/vulkan.h"
#include <vma/vk_mem_alloc.h>

#include "slang/slang-com-ptr.h"
#include "slang/slang.h"

#include <vector>

#include "Model.h"

struct SDL_Window;

struct Window {
        SDL_Window* window;
        i32         width;
        i32         height;
};

// Per Model - TODO: Split into VP and M matrices, can pass VP at start of render.
// Can even pass all model matrices with VP? pre calc and send together.
struct ShaderDrawData {
        Mat4 projection;
        Mat4 view;
        Mat4 model;
};

struct UniformBuffer {
        VkBuffer          buffer;
        VmaAllocation     allocation;
        VmaAllocationInfo allocation_info;
        VkDeviceAddress   device_address;
};

struct Game {
        void Init();
        void Shutdown();

        void Run();

        // ====== //

        static constexpr u32 max_frames_in_flight{ 2 };

        // ====== //

        Window window;

        VmaAllocator vulkan_allocator;

        VkInstance     vulkan_instance;
        VkDevice       vulkan_device;
        VkSurfaceKHR   vulkan_surface;
        VkSwapchainKHR vulkan_swapchain;
        VkQueue graphics_queue;

        std::vector<VkImage>     swapchain_images;
        std::vector<VkImageView> swapchain_image_views;

        VkImage       depth_image;
        VkImageView   depth_image_view;
        VmaAllocation depth_allocation;

        UniformBuffer draw_data[max_frames_in_flight];

        std::vector<VkSemaphore> render_semaphores;
        VkSemaphore              present_semaphores[max_frames_in_flight];
        VkFence                  fences[max_frames_in_flight];

        VkCommandPool   graphics_command_pool;
        VkCommandBuffer graphics_command_buffers[max_frames_in_flight];

        VkDescriptorSetLayout descriptor_set_layout;
        VkDescriptorPool      descriptor_pool;
        VkDescriptorSet       descriptor_set;

        Slang::ComPtr<slang::IGlobalSession> slang_global_session;

        VkPipelineLayout pipeline_layout;
        VkPipeline       pipeline;

        // ===== //

        Vec3           camera_position{0, 0, -10.0f};
        ShaderDrawData shader_draw_data;

        u32 frame_index{};
        u32 image_index{};

        bool swapchain_needs_resizing{ false };


        Model model;
};