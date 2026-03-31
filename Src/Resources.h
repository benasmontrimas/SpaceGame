#pragma once

#include "Base.h"

#include "vulkan/vulkan.h"
#include <vma/vk_mem_alloc.h>

struct GPUBuffer {
        VkBuffer          buffer;
        VmaAllocation     allocation;
        VmaAllocationInfo allocation_info;
        VkDeviceAddress   device_address;

        // ===== Synchronization ====== //

        u32         owning_queue_index;
        VkSemaphore ownership_semaphore;
        // Used by the transfer engine as it never keeps ownership of the buffer, but reusing ownership_semaphore can result in
        // 2 queue families waiting on the same semaphore, and im not sure how ordering works, if owning queue recieves it first
        // the queue transitions would be broken.
        VkSemaphore transfer_semaphore;
};

// NOTE: Not currently used, but incase we want to provide some sort of partial transfer functionality
// we can use this.
struct GPUBufferSlice {
        GPUBuffer gpu_buffer;

        u64 offset;
        u64 size;
};

constexpr VmaAllocationCreateFlags GPUBuffer_GPU_ONLY = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
constexpr VmaAllocationCreateFlags GPUBuffer_STAGING  = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
constexpr VmaAllocationCreateFlags GPUBuffer_READBACK = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;


GPUBuffer CreateGPUBuffer(VmaAllocator allocator, u64 size, VkBufferUsageFlags usage, VmaAllocationCreateFlags allocation_flags,
                          VmaMemoryUsage memory_usage = VMA_MEMORY_USAGE_AUTO);
GPUBuffer CreateStagingBuffer(VmaAllocator allocator, u64 size);
GPUBuffer CreateReadbackBuffer(VmaAllocator allocator, u64 size);

// ===== Buffers2 Interface ===== //

// NOTE: Do we want to pass staging buffers? Makes it easier to reuse.
// Staging buffers are hard to size properly as we dont want to waste too much memory on just staging.
// NOTE: If we want to allow max size texture of 4096x4096 for SDR images we need to allow atleast 68MB for texture staging buffers
// NOTE: For Models, assuming 4 * Vec4 as vertex data, need about 28MB for model with 400,000 Verts and 150,000 Triangles
// NOTE: Would probably want multiple copies in parallel, that said, most wont be the max size, so we can use offsets into the same
// Staging Buffer

// NOTE: VkDeviceAddress can be offset by a u64.

struct BufferOwnershipInfo {
        VkCommandBuffer       command_buffer;
        GPUBuffer             buffer;
        u32                   old_queue_family;
        u32                   new_queue_family;
        VkPipelineStageFlags2 stage_mask;
        VkAccessFlags2        access_mask;
};

void ReadFromGPUBuffer(GPUBuffer src, u32 offset, u32 size, u8* dst, GPUBuffer staging_buffer);
void WriteToGPUBuffer(GPUBuffer dst, u32 offset, u32 size, u8* src, GPUBuffer staging_buffer);

// NOTE: Can simplify if we can check for Inified Memory (UMA) systems, which means we can just write to GPU memory from the cpu.
// // This avoids queue syncronizations and transfers assuming that the copy is performed before any operation using it is submitted.

constexpr u32 max_transfers_in_flight         = 8;
constexpr u64 staging_buffer_block_size       = 4 * KiloByte;
constexpr u64 staging_buffer_block_count      = 65'536;
constexpr u64 staging_buffer_total_size       = staging_buffer_block_size * staging_buffer_block_count;
constexpr u32 staging_buffer_max_free_regions = max_transfers_in_flight + 1;

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

enum class TransferJobType : u32 {
        Copy,
        Read,
        Write,
};

struct CopyTransferJob {
        GPUBuffer src;
        GPUBuffer dst;
        u64       src_offset;
        u64       dst_offset;
        u64       size;

        u32 src_queue_family;
        u32 dst_queue_family;
};

// NOTE: Copy from staging buffer to dst when user checks if job finished!
struct ReadTransferJob {
        GPUBuffer src;
        u64       src_offset;
        u64       size;
        u8*       dst;

        u32 src_queue_family;
};

struct WriteTransferJob {
        u8*       src;
        u64       src_offset;
        u64       src_size;
        GPUBuffer dst;

        u32 dst_queue_family;
}

struct TransferJob {

        TransferJobType type;

        union {
                CopyTransferJob  copy_job;
                ReadTransferJob  read_job;
                WriteTransferJob write_job;
        };

        // ===== Transfer Engine Variables ===== //

        u32          id;
        u32          transfer_index;
        BufferRegion staging_buffer_region;
};

// NOTE: Stalls for now. TODO: Sumbit commands and return fence -> up to user to synchronize data deletion and usage correctly.
// - Return a fence view and call release to return to pool for engine to reuse? that way we dont have to create and destory always.
// User can do this, or automatically does this on return true from check?
// - Can also just create and destroy fences allowing us to queue any number of transfer jobs in a vector and just return a unsignaled
// fence view. This might be best option -> Can we still have some fence pool system, difficult to manage as would need to be dynamic.
// and can result in frame drops if we need to resize vector and create new fences. Although realistically this can be set to a larger
// number than ever needed with very little memory user.
struct TransferEngine {
        void Init(VkDevice device, u32 transfer_queue_family_index);
        void Shutdown();
        void Update(); // If we want to queue things we will need an update function to call so that we can add more jobs to the queue.

        void CopyFromGPUBuffer(GPUBuffer src, u32 offset, u32 size, u8* dst);

        BufferRegion GetStagingBufferRegion(u64 size);
        void         ReleaseStagingBufferRegion(BufferRegion buffer_region);
        void         Check(u32 index);

        VmaAllocator vulkan_allocator;

        VkDevice vulkan_device;

        u32     queue_family_index;
        VkQueue queue;

        GPUBuffer    staging_buffer;
        // Could just make free regions a double linked list, makes it easier to add and remove... its never that big to really matter though.
        u32 free_staging_buffer_region_count;
        BufferRegion free_staging_buffer_regions[staging_buffer_max_free_regions];

        VkCommandPool   command_pool;
        VkCommandBuffer command_buffers[max_transfers_in_flight]; // Probably want multiple
        VkFence         fences[max_transfers_in_flight];
        bool            fence_in_use[max_transfers_in_flight];

        u32 transfer_index{}; // Used to mark index after the last used, as this is most likely to be ready.
};

// NOTE: This wont work... as fence cant point to a location which is guarateed to stay.
// Need to just have some sort of ID which can be passed to TransferEngine to check on progress,
// TransferEngine can then store map of ID to index.
// NOTE: Unless we store it as a non contiguous array, and just keep track of free slots.
// Yeah maybe do this method, and then return index instead of FenceView. means no pointers,
// less space, and transfer does all lookups. Also dont need to create and destroy fences.