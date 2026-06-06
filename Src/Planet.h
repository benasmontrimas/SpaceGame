#pragma once

// TODO: Move all this stuff into a cpp file so that i can pass the Game refernece without needing to include it here

#include "Base.h"
#include "MarchingLookup.h"
#include "Resources.h"

#include "slang/slang-com-ptr.h"
#include "slang/slang.h"
#include "volk/volk.h"
#include "vulkan/vulkan.h"

#include <print>
#include <vector>

// NOTE: ChunkLoading should generate these.
struct PlanetChunk {
        u32 vertex_count;
        u32 index_count;

        GPUBuffer index_buffer;
        GPUBuffer vertex_buffer;

        u32   lod;       // This chunks lod
        uVec4 edge_lods; // The lod of the edges to match with connecting lods.

        // NOTE: We can store this as an array of the all the surrounding edges instead.
        // If we store as the data we used for generation will still need to recalculate a lot.
        float* index_offsets; // Store the offsets for edges, so if we change a bordering lod we need to do less work to find the vertex.
};

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

struct EdgeSumInfo {
        u32 vertex_offset;
        u32 vertex_count;

        u32 triangle_offset;
        u32 triangle_count;
};

// Only care about the stages we need to do work inbetween.
enum class PlanetGenerationStage : u64 {
        None,
        StageOne,   // Pass 1 and 2
        StageTwo,   // Sum offsets
        StageThree, // Pass 3 and 4
        Finished,   // Return Chunk
};

struct PlanetChunkProgress {
        PlanetChunk        chunk;
        PlanetCreationInfo info;

        GPUBuffer edge_buffer;
        GPUBuffer edge_sums_buffer;

        VkSemaphore semaphore;
        VkFence     fence;

        // Use this and this + 1, as we need 2 command buffers.
        int command_buffer_index;

        PlanetGenerationStage GetStage(VkDevice vulkan_device) {
                u64 value;
                vkGetSemaphoreCounterValue(vulkan_device, semaphore, &value);
                return (PlanetGenerationStage)value;
        }
};

struct GameContext;

struct Planet {
        static constexpr u32 thread_group_x       = 8;
        static constexpr u32 thread_group_y       = 8;
        static constexpr u32 thread_group_z       = 1;
        static constexpr u32 command_buffer_count = 10 * 2; // Need 2 per Chunk Gen

        static constexpr u32 pass_count = 4;

        VkDevice       vulkan_device;
        VmaAllocator   vulkan_allocator;
        u32            compute_queue_family;
        VkQueue        compute_queue;
        u32            transfer_queue_family;
        TransferEngine transfer_engine;

        VkCommandPool    command_pool;
        VkCommandBuffer  command_buffers[command_buffer_count];
        std::vector<u32> free_command_buffers;
        VkPipelineLayout pipeline_layout;

        VkPipeline pipelines[pass_count];

        GPUBuffer edge_lookup_buffer;
        GPUBuffer triangle_lookup_buffer;

        std::vector<PlanetChunkProgress> chunks_in_progress;

        void Init(GameContext& game_context);
        void Update();

        int  GetCommandBufferIndex();
        void ReleaseCommandBufferIndex(int index);

        void GenerateChunk(Vec3 position, u32 chunk_cells, u32 chunk_dims);
        void FreeChunkInProgress(PlanetChunkProgress& chunk_progress);

        void          RecordCommands(PlanetChunkProgress& chunk_progress);
        VkSubmitInfo2 RecordStageOne(PlanetChunkProgress& chunk_progress);
        void          StartStageTwo(PlanetChunkProgress& chunk_progress);
        VkSubmitInfo2 RecordStageThree(PlanetChunkProgress& chunk_progress);
};