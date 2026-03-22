#pragma once

#include "Base.h"

#include <vector>

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
        VkBuffer          buffer;
        VmaAllocation     buffer_allocation;
        VmaAllocationInfo buffer_allocation_info;

        u64 vertices_size;
        u64 index_count;
};

struct Model {
        void LoadFromOBJ(const std::string& file_name, const VmaAllocator& allocator);
        void Destroy(const VmaAllocator& allocator);

        std::vector<Mesh> meshes;
};

struct Texture {
        void Load(VkDevice device, VkCommandPool command_pool, VkQueue queue, const std::string& file_name, const VmaAllocator& allocator);
        void Destroy(const VmaAllocator& allocator);

        VkImage       image;
        VkImageView   image_view;
        VkSampler     sampler;
        VmaAllocation allocation;
};