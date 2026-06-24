#pragma once

#include "Base.h"
#include "Resources.h"

#include "vma/vk_mem_alloc.h"
#include "vulkan/vulkan.h"

#include <string>
#include <vector>

struct GameContext;

enum class MaterialID : u32 {
        None = 0,

        SkyMap = 1,
        Basic = 2,
        Planet = 3,

        Count,
};

struct Vertex {
        Vec3 position;
        Vec3 normal;
        Vec2 uv;
};

// For intermediate data. Hold here for writting to gpu.
struct MeshData {
        std::vector<Vertex> vertices;
        std::vector<u32>    indices;
};

struct Mesh {
        GPUBuffer buffer;

        u64 vertices_size;
        u64 index_count;
};

struct Transform {
        Vec3 position{};
        Vec3 rotation{};
        Vec3 scale{ 1, 1, 1 };
};

struct Model {
        void LoadFromOBJ(GameContext& game_context, const std::string& file_name);
        void Destroy(GameContext& game_context);

        std::vector<Mesh> meshes;
        Transform         transform;

        MaterialID material_id;

        u32 last_frame_rendered;
};

// NOTE: Why does this take a command_pool? make a manager and pass command buffers please
struct Texture {
        void Load(VkDevice device, VkCommandPool command_pool, VkQueue queue, const std::string& file_name, const VmaAllocator& allocator);
        void Destroy(GameContext& game_context);

        VkImage       image;
        VkImageView   image_view;
        VkSampler     sampler;
        VmaAllocation allocation;
};