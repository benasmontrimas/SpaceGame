#pragma once

#include "vulkan/vulkan.h"
#include <vma/vk_mem_alloc.h>

#include "slang/slang-com-ptr.h"
#include "slang/slang.h"

#include <vector>

#include "InputSystem.h"
#include "Model.h"
#include "Planet.h"
#include "Resources.h"
#include "Camera.h"

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

// NOTE: Can I split out core stuff into a GameContext struct so that I can pass that around to systems and have gameplay in Game.

struct GameContext {
        void Init();
        void InitComputePipeline();

        void Shutdown();

        void Render(const Camera& camera);

        // ====== //

        static constexpr u32 max_frames_in_flight{ 2 };

        // ====== //

        Window window;

        VmaAllocator vulkan_allocator;

        VkInstance     vulkan_instance;
        VkDevice       vulkan_device;
        VkSurfaceKHR   vulkan_surface;
        VkSwapchainKHR vulkan_swapchain;

        u32 graphics_queue_family;
        u32 compute_queue_family;
        u32 transfer_queue_family;

        VkQueue graphics_queue;
        VkQueue compute_queue;

        std::vector<VkImage>     swapchain_images;
        std::vector<VkImageView> swapchain_image_views;

        VkImage       depth_image;
        VkImageView   depth_image_view;
        VmaAllocation depth_allocation;

        GPUBuffer draw_data[max_frames_in_flight];

        std::vector<VkSemaphore> render_semaphores;
        VkSemaphore              present_semaphores[max_frames_in_flight];
        VkFence                  fences[max_frames_in_flight];

        VkCommandPool   graphics_command_pool;
        VkCommandBuffer graphics_command_buffers[max_frames_in_flight];

        VkDescriptorSetLayout descriptor_set_layout;
        VkDescriptorPool      descriptor_pool;
        VkDescriptorSet       descriptor_set;

        VkPipelineLayout pipeline_layout;
        VkPipeline       pipeline;

        // ===== Slang ==== //

        Slang::ComPtr<slang::IGlobalSession> slang_global_session;
        Slang::ComPtr<slang::ISession>       slang_session;

        // ===== Compute Pipeline ===== //

        VkPipelineLayout compute_pipeline_layout;
        VkPipeline       compute_pipeline; // Does this need a layout?

        VkDescriptorSetLayout compute_descriptor_set_layout;
        VkDescriptorPool      compute_descriptor_pool;
        VkDescriptorSet       compute_descriptor_set;

        std::vector<VkSemaphore> compute_semaphores;
        std::vector<VkFence>     compute_fences;
        VkCommandPool            compute_command_pool;
        VkCommandBuffer          compute_command_buffer;

        // ===== Transfer Pipeline ===== //

        TransferEngine transfer_engine;

        u32 frame_index{};
        u32 image_index{};

        ShaderDrawData shader_draw_data;

        bool swapchain_needs_resizing{ false };

        // ===== Input ===== //

        InputSystem input_system;

        // ===== REPLACE BELOW ===== //
        Vec3  camera_position{ 0, 0, -10.0f };
        Model model;
};

struct Game {
        void Init();
        void Shutdown();
        void Run();

        GameContext game_context;

        GPUBuffer planet_density_buffer;

        Planet planet;

        // Actions
        Action* forward_action;
        Action* back_action;
        Action* right_action;
        Action* left_action;

        Camera camera;
        DefaultController camera_controller;
};