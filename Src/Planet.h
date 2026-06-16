#pragma once


#include "Base.h"
#include "MarchingLookup.h"
#include "Model.h"
#include "Resources.h"

#include "slang/slang-com-ptr.h"
#include "slang/slang.h"
#include "volk/volk.h"
#include "vulkan/vulkan.h"

#include <print>
#include <vector>


// ===================== //
// ===== Constants ===== //
// ===================== //


constexpr u32 NODE_CHILD_COUNT            = 8;
constexpr u32 NO_CHILDREN                 = u32_max;
constexpr u32 PARENT_ROOT                 = u32_max - 1;
constexpr u32 NO_CHUNK                    = u32_max;
constexpr u32 EMPTY_NODE                  = u32_max - 1;
constexpr u32 CHUNK_RADIUS_GROWTH_PER_LOD = 2;

constexpr Vec3 PARENT_CENTER_OFFSET[NODE_CHILD_COUNT] = {
        { -1, -1, -1 }, { -1, -1, 1 }, { -1, 1, -1 }, { -1, 1, 1 }, { 1, -1, -1 }, { 1, -1, 1 }, { 1, 1, -1 }, { 1, 1, 1 },
};

constexpr u32   CHUNK_LOD_DISTANCE_COUNT                      = 6;
constexpr float CHUNK_LOD_DISTANCES[CHUNK_LOD_DISTANCE_COUNT] = {
        0, 64, 256, 1'024, 8'192, 65'536,
};

constexpr u32 MINIMUM_CHUNK_DIMS = 32;
constexpr u32 PLANET_MAX_RADIUS  = 60'000;

// Calculates the size the root of the chunk tree given a minimum chunk size.
constexpr u32 GetTreeRootDiameter() {
        u32 ans = MINIMUM_CHUNK_DIMS;

        while (ans < PLANET_MAX_RADIUS) {
                ans *= CHUNK_RADIUS_GROWTH_PER_LOD;
        }

        return ans;
}

constexpr u32 GetMaxTreeLevels() {
        u32 ans   = MINIMUM_CHUNK_DIMS;
        u32 count = 0;

        while (ans < PLANET_MAX_RADIUS) {
                ans *= CHUNK_RADIUS_GROWTH_PER_LOD;
                count++;
        }

        return count;
}

constexpr u32 TREE_ROOT_DIAMETER = GetTreeRootDiameter();
constexpr u32 TREE_MAX_LOD       = GetMaxTreeLevels();


// =========================== //
// ===== Forward Declare ===== //
// =========================== //


// == External == //

struct GameContext;
struct GameObject;

// == Internal == //

enum class PlanetGenerationState : u32;

struct PlanetChunk;
struct PlanetChunkNode;
struct PlanetChunkTree;
struct PlanetCreationInfo;
struct EdgeSumInfo;
struct PlanetChunkProgress;
struct Planet;


// =================== //
// ===== Structs ===== //
// =================== //


// Keeps track of a chunks state.
enum class PlanetGenerationState : u32 {
        None,

        // ===== Generation Stages ===== //

        WaitingOnGeneration,
        ProcessingStageOne,   // Pass 1 and 2
        ProcessingStageTwo,   // Sum offsets
        ProcessingStageThree, // Pass 3 and 4
        FinishingProcessing,  // Return Chunk

        // ===== Destroy States ===== //

        WaitingOnDestroy,

        // ===== Idle States ===== //

        Initialised,
        Empty,
};

// The planet chunks
struct PlanetChunk {
        PlanetGenerationState state;
        Vec3                  position;

        union {
                // NOTE: Do we need this?
                u32 lod_level;
                // ===== Points to next free slot if destroyed ===== //
                u32 next_free_index;
        };

        Model model;

        // ===== Experimental ===== //

        // Need some way to quickly access edge vertices so we can line up with lod differences.
        float* index_offsets;
};

// Node in octree structure to keep track of chunk indices.
struct PlanetChunkNode {
        // ===== Properties Masks ===== //

        static constexpr u32 parent_offset_mask = 0b111;
        static constexpr u32 is_empty_mask      = 0b1000;

        // ===== Members ===== //

        u32 first_child_index;
        u32 chunk_index;

        // ===== Functions ===== //

        void SetEmpty(bool is_empty) {
                if (is_empty) chunk_index = EMPTY_NODE;
                else chunk_index = NO_CHUNK;
        }

        bool IsEmpty() const {
                return chunk_index == EMPTY_NODE;
        }

        bool HasChildren() const {
                return (first_child_index != NO_CHILDREN) or IsEmpty();
        }

        bool HasChunk() const {
                return (chunk_index != NO_CHUNK);
        }
};

// Octree structure to handle planet LOD, and keep track of planet chunks.
struct PlanetChunkTree {
        Planet*                      planet;
        AABB                         root;
        std::vector<u32>             parent_indices;
        std::vector<PlanetChunkNode> nodes;
        u32                          next_free_index;

        // ===== Function ===== //

        void Init(Planet* _planet);
        void Update();
        void Shutdown();

        void Traverse(u32 node_index, AABB bounds, u32 lod_level);
        u32  AssignChildren(u32 parent_index);
        void RemoveChildren(u32 parent_index);
        void ResetNode(u32 node_index);
        void DirtyNode(u32 node_index);

        u32 GetParentIndex(u32 node_index) {
                u32 parent_index = parent_indices[node_index / 8];
                return parent_index;
        }
};

// Struct passed to GPU for generating chunks.
struct PlanetCreationInfo {
        u32  seed;
        Vec3 position;
        u32  chunk_cells;
        u32  chunk_dims;

        VkDeviceAddress edge_case_lookup;
        VkDeviceAddress triangle_lookup;

        VkDeviceAddress edges;
        VkDeviceAddress edge_sums;
        VkDeviceAddress triangle_normals;

        VkDeviceAddress indices;
        VkDeviceAddress vertices;
};

// Struct used to store edge sums for chunks.
struct EdgeSumInfo {
        u32 vertex_offset;
        u32 vertex_count;

        u32 triangle_offset;
        u32 triangle_count;
};

// Intermediate data needed to generate chunks
struct PlanetChunkProgress {
        u32                chunk_index;
        PlanetCreationInfo info;

        GPUBuffer edge_buffer;
        GPUBuffer edge_sums_buffer;

        VkSemaphore semaphore;
        VkFence     fence; // Not used?

        int command_buffer_index;
};

struct Planet {
        // ===== Constants ===== //

        static constexpr u32 THREAD_GROUP_X        = 8;
        static constexpr u32 THREAD_GROUP_Y        = 8;
        static constexpr u32 THREAD_GROUP_Z        = 1;
        static constexpr u32 COMMAND_BUFFER_COUNT  = 10;
        static constexpr u32 GENERATION_PASS_COUNT = 4;
        static constexpr u32 CHUNK_CELLS           = 128;

        // ===== Members ===== //

        Vec3 position;

        GameContext* game_context;

        VkCommandPool    command_pool;
        VkCommandBuffer  command_buffers[COMMAND_BUFFER_COUNT];
        std::vector<u32> free_command_buffers;
        VkPipelineLayout pipeline_layout;
        VkPipeline       pipelines[GENERATION_PASS_COUNT];

        GPUBuffer edge_lookup_buffer;
        GPUBuffer triangle_lookup_buffer;

        std::vector<PlanetChunk> chunks;
        u32                      next_free_chunk;

        std::vector<PlanetChunkProgress> chunks_in_progress;
        std::vector<u32>                 chunks_to_render;
        std::vector<u32>                 chunks_to_destroy;
        u32                              destroy_count;

        PlanetChunkTree tree;

        GameObject* target;

        // ===== Functions ===== //

        void Init(GameContext* _game_context, GameObject* _target);
        void Shutdown();
        void Update();

        int  GetCommandBufferIndex();
        void ReleaseCommandBufferIndex(int index);

        // TODO: I have changed these here -> Return EMPTY_NODE if empty! Oh wait we wont know yet!
        u32  GenerateChunk(AABB bounds, u32 lod_level);
        void DestroyChunk(u32 chunk_index);

        void FreeChunkInProgress(PlanetChunkProgress& chunk_progress);

        VkSubmitInfo2 RecordStageOne(PlanetChunkProgress& chunk_progress);
        void          StartStageTwo(PlanetChunkProgress& chunk_progress);
        VkSubmitInfo2 RecordStageThree(PlanetChunkProgress& chunk_progress);
};