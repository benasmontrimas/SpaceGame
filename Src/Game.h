#pragma once

#include "vulkan/vulkan.h"
#include <vma/vk_mem_alloc.h>

#include "slang/slang-com-ptr.h"
#include "slang/slang.h"

#include <vector>

#include "Camera.h"
#include "InputSystem.h"
#include "Model.h"
#include "Planet.h"
#include "Resources.h"

struct SDL_Window;

struct Window {
        SDL_Window* window;
        i32         width;
        i32         height;
        bool is_resized;
};

// ====== //

static constexpr u32 max_frames_in_flight{ 2 };
static constexpr u32 max_draw_count{ 10'000 };

// NOTE: Can I split out core stuff into a GameContext struct so that I can pass that around to systems and have gameplay in Game.

struct GameContext {
        void Init();

        void Shutdown();

        void Render(const Camera& camera);
        void Resize();

        // ====== //

        Window window;

        VmaAllocator vulkan_allocator;

        VkInstance       vulkan_instance;
        VkPhysicalDevice physical_device;
        VkDevice         vulkan_device;
        VkSurfaceKHR     vulkan_surface;
        VkSwapchainKHR   vulkan_swapchain;

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

        // ===== Sync ===== //

        std::vector<VkSemaphore> render_semaphores;
        VkSemaphore              present_semaphores[max_frames_in_flight];
        VkFence                  fences[max_frames_in_flight];

        // ===== Render ===== //

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

        // ===== Transfer Pipeline ===== //

        TransferEngine transfer_engine;

        // ===== Render Data ===== //

        u32 frame_index{};
        u32 image_index{};

        u64 frames_submitted{ max_frames_in_flight }; // (frames_submitted - max_frames_in_flight) == last know finished frame

        bool swapchain_needs_resizing{ false };

        GPUBuffer per_frame_draw_buffer;

        // ===== Systems ===== //

        InputSystem input_system;
        TextSystem  text_system;
        ModelSystem model_system;

        // ===== Functions ===== //

        u32 GetLastFinishedFrame() const {
                return u32(frames_submitted - max_frames_in_flight);
        }

        void AddTexture(const Texture& texture, u32 index);
};

struct Game {
        void Init();
        void Shutdown();
        void Update(float delta_time);
        void Run();

        GameContext game_context;

        Planet planet;

        ModelID letter_quad_model;
        ModelID sky_sphere_model;
        ModelID box_model;

        Texture skymap;

        // Model Instances
        ModelInstance sky_sphere;
        ModelInstance test_box;

        // Actions
        Action* forward_action;
        Action* back_action;
        Action* right_action;
        Action* left_action;

        Camera            camera;
        DefaultController camera_controller;
};