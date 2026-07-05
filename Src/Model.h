#pragma once

#include "Base.h"
#include "Resources.h"

#include "vma/vk_mem_alloc.h"
#include "vulkan/vulkan.h"

#include <string>
#include <vector>

#include <ft2build.h>

#include <freetype/freetype.h>
#include <freetype/ftoutln.h>

struct GameContext;

using TextureID = u32;
using ModelID   = u32;

// ===================== //
// ===== MATERIALS ===== //
// ===================== //

enum class MaterialType : u32 {
        None = 0,

        SkyMap = 1,
        Basic  = 2,
        Planet = 3,
        Text   = 4,
        UI     = 5,
        UIImage = 6,

        Count,
};

struct MaterialBase {
        TextureID textures_ids[4];
};

struct MaterialSkyMap {
        TextureID texture_id;
};

struct MaterialPlanet {
        TextureID ground_diffuse_texture_id;
        TextureID ground_normal_texture_id;
};

struct MaterialText {
        TextureID texture_start;
};

struct MaterialUI {};

struct MaterialUIImage {
        TextureID texture_id;
};

struct Material {
        MaterialType type;

        union {
                MaterialBase   base;
                MaterialSkyMap skymap;
                MaterialPlanet planet;
                MaterialText   text;
                MaterialUI     ui;
                MaterialUIImage ui_image;
        };

        void* Data() const {
                return (void*)&base;
        }

        u64 DataSize() const {
                return sizeof(MaterialBase);
        }
};

// ===================== //
// ===== DRAW DATA ===== //
// ===================== //

struct FrameDrawData {
        Mat4 projection_matrix;
        Mat4 view_matrix;
};

struct ModelDrawData {
        Material material;
};

struct InstanceDrawData {
        Mat4 model_matrix;
        Vec4 colour0;
        Vec4 colour1;
        u32  user_value;
        u32  user_value1;
        u32  user_value2;
        u32  user_value3;
};

struct VertexDrawData {
        Vec3 position;
        Vec3 normal;
        Vec2 uv;
};

// ===== Triangle BVH ===== //

struct TriangleBVHNode {
        AABB bounds;

        union {
                u32 child_index;
                u32 index_count;
        };

        u32 index_offset;
};

struct TriangleBVH {
        static constexpr float triangle_intersect_cost = 1.5;
        static constexpr float bounds_intersect_cost   = 1.0;

        static constexpr u32 bin_count_per_dimension = 3;
        static constexpr u32 max_indices_in_node     = 36;
        static constexpr u32 root_node               = 1;

        std::vector<TriangleBVHNode> nodes;

        void Init(VertexDrawData* vertices, u32 vertex_count, u32* indices, u32 index_count);
        void Traverse();

        // ===== For BVH Creation ===== //

        float CalculateCost(float parent_area, u32 index_count, const TriangleBVHNode& left, const TriangleBVHNode& right);
        bool  GetBestSplit(VertexDrawData* vertices, u32* indices, u32 index_count, u32 node_index, Vec3* split_position);
        void  Split(VertexDrawData* vertices, u32* indices, u32 index_count, u32 index_offset, u32 node_index);
};

// ===== MODELS ===== //

// A vertex and index buffer to represent a single mesh
struct Mesh {
        GPUBuffer buffer;
        u64       vertices_size;
        u64       index_count;

        u32* indices;
        VertexDrawData* vertices;
        TriangleBVH bvh;

        float Traverse(u32 node_index, Ray ray, float ray_length);
};

// A single instance of a model.
struct ModelInstance {
        ModelID model_id;

        Transform transform;
        Vec4      colour0;
        Vec4      colour1;
        u32       user_value;
        u32       user_value1;
        u32       user_value2;
        u32       user_value3;
};

// Represents a collection of Meshes.
struct Model {
        // ===== Init/Deinit Functions ===== //

        void Init(GameContext* game_context, VertexDrawData* vertices, u64 vertex_count, u32* indices, u64 index_count, u32 _max_instance_count);
        void Init(GameContext* game_context, u64 vertex_count, u64 index_count, u32 _max_instance_count);
        void LoadFromOBJ(GameContext* game_context, const std::string& file_name, u32 _max_instance_count);

        void Destroy(GameContext* game_context);

        void Acquire(GameContext* game_context, VkCommandBuffer command_buffer, std::vector<VkSemaphore>& wait_semaphores);
        void Draw(GameContext* game_context, VkCommandBuffer command_buffer, VkPipelineLayout pipeline_layout, u32 frame_index);

        // ===== Members ===== //

        Mesh* meshes;
        u32   mesh_count;

        InstanceDrawData* instance_draw_data;
        u32               instance_count;

        GPUBuffer instance_buffer;

        Material material;

        u32 last_frame_rendered;

        u32 max_instance_count;

        bool is_transparent {false};

};

union ModelSlot {
        Model model;
        u32   next_free_index;
};

struct ModelSystem {

        std::vector<ModelSlot> models;
        ModelID                next_free_index;

        GameContext* game_context;

        void Init(GameContext* _game_context);

        ModelID GetNewModelID();

        ModelID LoadModel(const std::string& file_path, u32 max_instance_count, bool is_transparent = false);
        ModelID CreateModel(VertexDrawData* vertices, u64 vertex_count, u32* indices, u64 index_count, u32 _max_instance_count, bool is_transparent = false);
        ModelID CreateModel(u64 vertex_count, u64 index_count, u32 _max_instance_count, bool is_transparent = false);
        ModelID AddModel(const Model& model);

        void          DestroyModel(ModelID id);
        ModelInstance CreateModelInstance(ModelID id) const;

        void Draw(ModelInstance instance);

        void Acquire(VkCommandBuffer command_buffer, std::vector<VkSemaphore>& wait_semaphores);
        void Draw(VkCommandBuffer command_buffer, u32 frame_index);

        Model& operator[](ModelID id) {
                return models[id].model;
        }

        const Model& operator[](ModelID id) const {
                return models[id].model;
        }
};

// ==================== //
// ===== TEXTURES ===== //
// ==================== //

// NOTE: Why does this take a command_pool? make a manager and pass command buffers please
struct Texture {
        void Load(VkDevice device, VkCommandPool command_pool, VkQueue queue, const std::string& file_name, const VmaAllocator& allocator);
        void Destroy(GameContext* game_context);

        VkImage       image;
        VkImageView   image_view;
        VkSampler     sampler;
        VmaAllocation allocation;
};