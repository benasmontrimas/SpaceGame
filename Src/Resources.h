#pragma once

#include "Base.h"

#include "vulkan/vulkan.h"
#include <vma/vk_mem_alloc.h>

struct GPUBuffer {
        VkBuffer          buffer;
        VmaAllocation     allocation;
        VmaAllocationInfo allocation_info;
};

constexpr VmaAllocationCreateFlags GPUBuffer_GPU_ONLY = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
constexpr VmaAllocationCreateFlags GPUBuffer_STAGING  = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
constexpr VmaAllocationCreateFlags GPUBuffer_READBACK = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;


GPUBuffer CreateGPUBuffer(VmaAllocator allocator, u64 size, VkBufferUsageFlags usage, VmaAllocationCreateFlags allocation_flags,
                          VmaMemoryUsage memory_usage = VMA_MEMORY_USAGE_AUTO);
GPUBuffer CreateStagingBuffer(VmaAllocator allocator, u64 size);
GPUBuffer CreateReadbackBuffer(VmaAllocator allocator, u64 size);


struct CPUBuffer;
struct GPUCPUBuffer;

// ===== Buffers Interface ===== //

/*
Split buffers into their own struct based on their use/memory location.
Do we even really need this...
Maybe better to do as in {Buffers2 Interface}.
*/

// Buffer which is in GPU memory, not intended to be read from the cpu.
struct GPUBufferA {
        void CopyFrom(CPUBuffer src);
        void CopyTo(CPUBuffer dst);
};

// Buffer which is in CPU memory, used as staging buffers
struct CPUBuffer {
        void* GetData();
};

// Buffers which are in GPU memory but accessible from CPU, used for smaller buffers that need to be written and read often.
struct GPUCPUBuffer {
        void Write(u32 offset, u32 size, u8* src);
        void Read(u32 offset, u32 size, u8* dst);
};

void* TransferData(GPUBufferA src,

// ===== Buffers2 Interface ===== //

// NOTE: Do we want to pass staging buffers? Makes it easier to reuse.
// Staging buffers are hard to size properly as we dont want to waste too much memory on just staging.
// NOTE: If we want to allow max size texture of 4096x4096 for SDR images we need to allow atleast 68MB for texture staging buffers
// NOTE: For Models, assuming 4 * Vec4 as vertex data, need about 28MB for model with 400,000 Verts and 150,000 Triangles
// NOTE: Would probably want multiple copies in parallel, that said, most wont be the max size, so we can use offsets into the same
// Staging Buffer

// NOTE: VkDeviceAddress can be offset by a u64.

void ReadFromGPUBuffer(GPUBuffer src, u32 offset, u32 size, u8* dst, GPUBuffer staging_buffer);
void WriteToGPUBuffer(GPUBuffer dst, u32 offset, u32 size, u8* src, GPUBuffer staging_buffer);