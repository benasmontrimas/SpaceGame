#pragma once


#include "Base.h"
#include "MarchingLookup.h"
#include "Model.h"
#include "Resources.h"

#include "volk/volk.h"

#include <vector>


// ===================== //
// ===== Constants ===== //
// ===================== //

constexpr u32 MINIMUM_CHUNK_DIMS = 16;
constexpr u32 PLANET_MAX_RADIUS  = 50'000;


using NodeID  = u32;
using ChunkID = u32;

constexpr u32 PLANET_SEED = 3'203;

constexpr u32 NO_FREE = u32_max;

constexpr u32 NODE_CHILD_COUNT            = 8;
constexpr u32 CHUNK_RADIUS_GROWTH_PER_LOD = 2;

constexpr u32 NO_CHILDREN = u32_max;
constexpr u32 NO_CHUNK    = u32_max;
constexpr u32 EMPTY_CHUNK = u32_max - 1;
constexpr u32 PARENT_ROOT = u32_max;

constexpr Vec3 PARENT_CENTER_OFFSET[NODE_CHILD_COUNT] = {
        { -1, -1, -1 }, { -1, -1, 1 }, { -1, 1, -1 }, { -1, 1, 1 }, { 1, -1, -1 }, { 1, -1, 1 }, { 1, 1, -1 }, { 1, 1, 1 },
};

constexpr u32   CHUNK_LOD_DISTANCE_COUNT                      = 10;
constexpr float CHUNK_LOD_DISTANCES[CHUNK_LOD_DISTANCE_COUNT] = {
        // 0, 32, 64, 128, 256, 1'024,
        // 1000, 1000, 1000, 1000, 1000, 1000,
        2,     5,     10,    50,   100, 400,
        1'000, 2'000, 3'000, 5'000
        // 1, 1, 1, 100, 100, 100, 100, 1000, 1000, 1000
};


enum class PlanetPipelineID {
        Pass1,
        Pass2,
        Pass3,
        Pass4,
        Pass5,
        Pass6,
};

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
struct Transform;

// == Internal == //

enum class ChunkState : u32;

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

// Used for timeline semaphore values
enum class PlanetProcessingStages : u32 {
        Invalid,
        StageOne,
        AllocateBuffers,
        StageTwo,
        CopyMeshData,
        CreatingBVH,
        Finished,

        Count,
};

// Keeps track of a chunks state.
enum class ChunkState : u32 {
        None,

        Waiting,

        // ===== Generation Stages ===== //

        Processing,

        // ===== Destroy States ===== //

        WaitingOnDestroy,

        // ===== Idle States ===== //

        Initialised,
};

// The planet chunks
struct PlanetChunk {
        ChunkState state;
        Vec3       position;

        union {
                u32 lod;
                // ===== Points to next free slot if destroyed ===== //
                u32 next_free_index;
        };

        ModelID model_id;
        bool    is_empty = false;

        // bool IsEmpty() const {
        //         if (state == ChunkState::Processing) return false;
        //         return model_id == u32_max;
        // }

        bool IsRenderReady() const {
                if (model_id == u32_max) return false;
                return state == ChunkState::Initialised and !is_empty;
        }

        // ===== Experimental ===== //

        // Need some way to quickly access edge vertices so we can line up with lod differences.
        float* index_offsets;
};

// Node in octree structure to keep track of chunk indices.
struct PlanetChunkNode {
        // ===== Members ===== //

        NodeID  first_child_index;
        ChunkID chunk_index;

        // ===== Functions ===== //

        bool IsEmpty() const {
                return chunk_index == EMPTY_CHUNK;
        }

        bool HasChildren() const {
                return first_child_index != NO_CHILDREN;
        }

        bool HasChunk() const {
                return chunk_index != NO_CHUNK;
        }
};

// Octree structure to handle planet LOD, and keep track of planet chunks.
struct PlanetChunkTree {
        Planet*                      planet;
        AABB                         root;
        std::vector<NodeID>          parent_indices;
        std::vector<PlanetChunkNode> nodes;
        NodeID                       next_free_index;

        // ===== Function ===== //

        void Init(Planet* _planet);
        void Update();
        void Shutdown();

        void   Traverse(NodeID node_index, AABB bounds, u32 lod, bool waited_on);
        void   TraverseChildren(NodeID node_index, AABB bounds, u32 lod, bool waited_on);
        void   Split(NodeID node_index, AABB bounds, u32 lod, bool waited_on);
        void   HandleLeaf(NodeID node_index, AABB bounds, u32 lod, bool waited_on);
        NodeID AddChildren(NodeID node_index);
        void   RemoveChildren(NodeID node_index);
        void   CreateNodesChunk(NodeID node_index, AABB bounds, u32 lod);
        void   DestroyNodesChunk(NodeID node_index);

        bool AreAllChildrenInitialised(NodeID node_index);

        // Use to set empty to false so that we re-check the node
        void DirtyNode(NodeID node_index);

        NodeID GetParentIndex(NodeID node_index) {
                NodeID parent_index = parent_indices[node_index / 8];
                return parent_index;
        }

        float RayIntersect(NodeID node_index, AABB bounds, Ray ray, float ray_length);
};

// Struct passed to GPU for generating chunks.
struct PlanetCreationInfo {
        VkDeviceAddress edge_case_lookup;
        VkDeviceAddress triangle_lookup;

        VkDeviceAddress edges;
        VkDeviceAddress cell_sums;
        VkDeviceAddress normals;

        VkDeviceAddress indices;
        VkDeviceAddress vertices;

        u32 chunk_cells;
        u32 chunk_dims;

        Vec3 position;

        u32 seed;
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

        u64         semaphore_start_value;
        VkSemaphore semaphore;

        GPUBuffer edge_buffer;
        GPUBuffer cell_sums_buffer;
        GPUBuffer normal_buffer;

        u32 resource_index;

        u8*           mesh_data;
        TransferJobID copy_id;
};

struct Planet {
        // ===== Constants ===== //

        static constexpr u32 THREAD_GROUP_X        = 64;
        static constexpr u32 THREAD_GROUP_Y        = 1;
        static constexpr u32 THREAD_GROUP_Z        = 1;
        static constexpr u32 COMMAND_BUFFER_COUNT  = 20;
        static constexpr u32 GENERATION_PASS_COUNT = 6;
        static constexpr u32 CHUNK_CELLS           = 127;
        static constexpr u32 CHUNK_EDGES           = 128;

        static constexpr const char* ENTRY_POINT_NAMES[GENERATION_PASS_COUNT] = {
                "Pass1", "Pass2", "Pass3", "Pass4", "Pass5", "Pass6",
        };

        static constexpr u64 EDGE_BUFFER_SIZE     = CHUNK_CELLS * CHUNK_EDGES * CHUNK_EDGES * sizeof(u32);
        static constexpr u64 CELL_SUM_BUFFER_SIZE = CHUNK_CELLS * CHUNK_CELLS * sizeof(EdgeSumInfo);

        // ===== Members ===== //

        Vec3 position;

        GameContext* game_context;

        VkCommandPool   command_pool;
        VkCommandBuffer command_buffers[COMMAND_BUFFER_COUNT];
        VkSemaphore     semaphores[COMMAND_BUFFER_COUNT];

        std::vector<u32> free_command_buffers;
        VkPipelineLayout pipeline_layout;
        VkPipeline       pipelines[GENERATION_PASS_COUNT];

        GPUBuffer edge_lookup_buffer;
        GPUBuffer triangle_lookup_buffer;

        std::vector<PlanetChunk> chunks;
        u32                      next_free_chunk;

        std::vector<PlanetChunkProgress> chunks_in_progress;
        std::vector<ChunkID>             chunks_to_render;
        std::vector<ChunkID>             chunks_to_destroy;
        ChunkID                          destroy_count;

        PlanetChunkTree tree;

        Transform* target;

        Texture ground_texture;
        Texture ground_normal_texture;
        Texture ground_disp_texture;

        Material planet_material;

        // ===== Functions ===== //

        void Init(GameContext* _game_context, Transform* _target);
        void Shutdown();
        void Update();

        void    ProcessChunkIfAvailable(PlanetChunkProgress& chunk_progress);
        void    UpdateStates();
        ChunkID GetCommandBufferIndex();
        void    ReleaseCommandBufferIndex(ChunkID index);

        // TODO: I have changed these here -> Return EMPTY_NODE if empty! Oh wait we wont know yet!
        ChunkID GenerateChunk(AABB bounds, ChunkID lod);
        void    DestroyChunk(ChunkID chunk_index);

        void FreeChunkInProgress(u32 index);

        void RecordStageOne(PlanetChunkProgress& chunk_progress);
        void AllocateBuffers(PlanetChunkProgress& chunk_progress);
        void RecordStageTwo(PlanetChunkProgress& chunk_progress);

        float CheckIntersection(Ray ray, float distance);
};
