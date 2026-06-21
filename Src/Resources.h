#pragma once

#include "Base.h"

#include <vma/vk_mem_alloc.h>

struct GPUBuffer {
        VkBuffer          buffer;
        VmaAllocation     allocation;
        VmaAllocationInfo allocation_info;
        VkDeviceAddress   device_address;

        // ===== Synchronization ====== //

        u32         owning_queue_family;
        // TODO: Free these semaphores -> Create a DestroyGPUBuffer function!
        VkSemaphore ownership_semaphore;
        // Used by the transfer engine as it never keeps ownership of the buffer, but reusing ownership_semaphore can result in
        // 2 queue families waiting on the same semaphore, and im not sure how ordering works, if owning queue recieves it first
        // the queue transitions would be broken.
        VkSemaphore transfer_semaphore;

        void Print();
};

constexpr VmaAllocationCreateFlags GPUBuffer_GPU_ONLY = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
constexpr VmaAllocationCreateFlags GPUBuffer_STAGING  = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

// What is this one used for? I just use the staging buffer.
constexpr VmaAllocationCreateFlags GPUBuffer_READBACK = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

[[nodiscard]]
GPUBuffer CreateGPUBuffer(VkDevice vulkan_device, VmaAllocator allocator, u64 size, u32 owning_queue_family, VkBufferUsageFlags usage,
                          VmaAllocationCreateFlags allocation_flags, VmaMemoryUsage memory_usage = VMA_MEMORY_USAGE_AUTO,
                          VkMemoryPropertyFlags memory_flags = 0);
[[nodiscard]]
GPUBuffer CreateStagingBuffer(VkDevice vulkan_device, VmaAllocator allocator, u64 size, u32 owning_queue_family);
[[nodiscard]]
GPUBuffer CreateReadbackBuffer(VkDevice vulkan_device, VmaAllocator allocator, u64 size, u32 owning_queue_family);

void DestroyGPUBuffer(VkDevice vulkan_device, GPUBuffer& buffer, VmaAllocator allocator);

// ===== Buffers2 Interface ===== //

// NOTE: VkDeviceAddress can be offset by a u64.

struct BufferOwnershipInfo {
        VkCommandBuffer       command_buffer;
        GPUBuffer*            buffer;
        u32                   old_queue_family;
        u32                   new_queue_family;
        VkPipelineStageFlags2 stage_mask;
        VkAccessFlags2        access_mask;
};

void CmdReleaseBufferOwnership(BufferOwnershipInfo& info);
void CmdAcquireBufferOwnership(BufferOwnershipInfo& info);

void ReadFromGPUBuffer(GPUBuffer src, u32 offset, u32 size, u8* dst, GPUBuffer staging_buffer);
void WriteToGPUBuffer(GPUBuffer dst, u32 offset, u32 size, u8* src, GPUBuffer staging_buffer);

// NOTE: Can simplify if we can check for Inified Memory (UMA) systems, which means we can just write to GPU memory from the cpu.
// // This avoids queue syncronizations and transfers assuming that the copy is performed before any operation using it is submitted.

constexpr u32 max_transfers_in_flight         = 8;
constexpr u64 staging_buffer_block_size       = 4 * KiloByte;
constexpr u64 staging_buffer_block_count      = 65'536;
constexpr u64 staging_buffer_total_size       = staging_buffer_block_size * staging_buffer_block_count;
constexpr u32 staging_buffer_max_free_regions = max_transfers_in_flight + 1;
constexpr u32 max_transfer_engine_jobs        = 1'000;
constexpr u32 transfer_queue_end              = max_transfer_engine_jobs;

/*
TODO: For optimal staging buffer with use with images use VkPhysicalDeviceLimits:
- optimalBufferCopyOffsetAlignment
- optimalBufferCopyRowPitchAlignment
- nonCoherentAtomSize (Although im not entirely sure what this means).
*/

struct BufferRegion {
        u32 block_offset;
        u32 block_count;

        u64 GetOffset() const {
                return block_offset * staging_buffer_block_size;
        }
};

struct GPUBufferCopyInfo {
        GPUBuffer* src;
        GPUBuffer* dst;
        u64        src_offset;
        u64        dst_offset;
        u64        size;

        u32 src_queue_family;
        u32 dst_queue_family;
};

// NOTE: Copy from staging buffer to dst when user checks if job finished!
struct GPUBufferReadInfo {
        GPUBuffer* src;
        u64        offset;
        u64        size;
        u8*        dst;

        u32          src_queue_family;
        BufferRegion staging_buffer_region;
};

struct GPUBufferWriteInfo {
        u8*        src;
        u64        offset;
        u64        size;
        GPUBuffer* dst;

        u32          dst_queue_family;
        BufferRegion staging_buffer_region; // This doesnt really make sense here..
};

enum class TransferJobType : u32 {
        Copy,
        Read,
        Write,
};

using TransferJobID = u32;

struct TransferJob {

        TransferJobType type;

        union {
                GPUBufferCopyInfo  copy_job;
                GPUBufferReadInfo  read_job;
                GPUBufferWriteInfo write_job;
        };

        // ===== Next Link ===== //

        TransferJobID next;

        // ===== Transfer Engine Variables ===== //

        TransferJobID id;
        u32           transfer_index;
        BufferRegion  staging_buffer_region;
};

struct TransferEngine {
        void Init(VkDevice device, u32 transfer_queue_family_index, VmaAllocator allocator);
        void Shutdown();
        void Update(); // If we want to queue things we will need an update function to call so that we can add more jobs to the queue.

        void GPUBufferRead(VkCommandBuffer command_buffer, const TransferJob& job);
        void GPUBufferWrite(VkCommandBuffer command_buffer, const TransferJob& job);

        BufferRegion GetStagingBufferRegion(u64 size);
        void         ReleaseStagingBufferRegion(BufferRegion buffer_region);

        TransferJobID PopNextFreeTransferJob();
        TransferJobID PopNextPendingTransferJob();

        void AddPendingJob(TransferJobID id);
        void FreeJob(TransferJobID id);

        TransferJobID QueueGPUBufferRead(GPUBufferReadInfo read_job);
        TransferJobID QueueGPUBufferWrite(GPUBufferWriteInfo write_job);

        // NOTE: Once this returns true, you must not recheck. ID will be reused for other jobs.
        bool Check(TransferJobID id);

        VmaAllocator vulkan_allocator;

        VkDevice vulkan_device;

        u32     transfer_queue_family;
        VkQueue queue;

        GPUBuffer    staging_buffer;
        // Could just make free regions a double linked list, makes it easier to add and remove... its never that big to really matter though.
        u32          free_staging_buffer_region_count;
        BufferRegion free_staging_buffer_regions[staging_buffer_max_free_regions];

        VkCommandPool   command_pool;
        VkCommandBuffer command_buffers[max_transfers_in_flight]; // Probably want multiple
        VkFence         fences[max_transfers_in_flight];
        bool            fence_in_use[max_transfers_in_flight];

        u32 transfer_index{}; // Used to mark index after the last used, as this is most likely to be ready.

        // ===== Transfer Jobs Queue ===== //

        // NOTE: Staging buffer usage can be optimized by splitting jobs up. If a job doesnt fit, start a job that copies what fits
        // and process the rest once space frees up. This means we can get 100% staging buffer use whilst we have atleast 100% the
        // space queued. Issue is this would call lots of small copy commands, so would need to check if performance actually improves.
        TransferJobID first_free;
        TransferJobID first_pending;
        TransferJobID last_pending;
        TransferJob   transfer_jobs[max_transfer_engine_jobs];
};
