#pragma once

#include "Base.h"

#include "vulkan/vulkan.h"
#include <vma/vk_mem_alloc.h>

#include <print>

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