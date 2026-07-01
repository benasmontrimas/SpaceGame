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

struct Material {
        MaterialType type;

        union {
                MaterialBase   base;
                MaterialSkyMap skymap;
                MaterialPlanet planet;
                MaterialText   text;
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
        u32  user_value;
};

struct VertexDrawData {
        Vec3 position;
        Vec3 normal;
        Vec2 uv;
};

// ===== MODELS ===== //

// Represents a object transform in 3D.
struct Transform {
        Vec3 position{};
        Quat rotation{};
        Vec3 scale{ 1, 1, 1 };
};

// Represents a object transform in 2D.
struct Transform2D {
        Vec2  position{};
        float rotation{};
        Vec2  scale{ 1, 1 };
};

// A vertex and index buffer to represent a single mesh
struct Mesh {
        GPUBuffer buffer;
        u64       vertices_size;
        u64       index_count;
};

// A single instance of a model.
struct ModelInstance {
        ModelID model_id;

        Transform transform;
        u32       user_value;
};

// Represents a collection of Meshes.
struct Model {
        // ===== Init/Deinit Functions ===== //

        void Init(GameContext* game_context, VertexDrawData* vertices, u64 vertex_count, u32* indices, u64 index_count, u32 _max_instance_count);
        void Init(GameContext* game_context, u64 vertex_count, u64 index_count, u32 _max_instance_count);
        void LoadFromOBJ(GameContext* game_context, const std::string& file_name, u32 _max_instance_count);

        void Destroy(GameContext* game_context);

        // ===== Members ===== //

        Mesh* meshes;
        u32   mesh_count;

        InstanceDrawData* instance_draw_data;
        u32               instance_count;

        GPUBuffer instance_buffer;

        Material material;

        u32 last_frame_rendered;

        u32 max_instance_count;

        void Draw(VkCommandBuffer command_buffer, VkPipelineLayout pipeline_layout, u32 frame_index);
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

        ModelID LoadModel(const std::string& file_path, u32 max_instance_count);
        ModelID CreateModel(VertexDrawData* vertices, u64 vertex_count, u32* indices, u64 index_count, u32 _max_instance_count);
        ModelID CreateModel(u64 vertex_count, u64 index_count, u32 _max_instance_count);
        ModelID AddModel(const Model& model);

        void          DestroyModel(ModelID id);
        ModelInstance CreateModelInstance(ModelID id) const;

        void Draw(ModelInstance instance);
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

// ========================== //
// ===== TEXT RENDERING ===== //
// ========================== //

struct RenderedLetter {
        char        letter;
        Transform2D transform;
        Vec4        colour;
};

struct TextSystem {
        static constexpr u64 max_letter_count  = 100'000;
        static constexpr u64 frame_buffer_size = max_letter_count * sizeof(InstanceDrawData);

        GameContext* game_context;
        ModelID      model_id;

        Texture title_font_atlas;
        Texture default_font_atlas;

        FT_Library ft_library;

        FT_Face title_font;
        FT_Face default_font;

        void Init(GameContext* _game_context);
        void Shutdown();
        void AddText(ModelInstance* model_instance, u32 text_letter_count);
};

struct RenderedText {
        std::string                 text{};
        Transform                   transform;
        std::vector<RenderedLetter> letter_data;
        Vec4                        default_colour{ 1, 1, 1, 1 };

        void SetText(TextSystem* text_system, const std::string& new_text);
        void SetSubText(TextSystem* text_system, const std::string& new_sub_text, u32 start);
        void SetColour(TextSystem* text_system, Vec4 colour);
        void SetColour(TextSystem* text_system, u32 letter_index, Vec4 colour); // NOTE: Count letter index from text, as we dont have spaces and new lines.

        void Draw(TextSystem* text_system);
};