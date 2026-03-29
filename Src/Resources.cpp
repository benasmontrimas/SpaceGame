#include "Resources.h"

#include <print>

GPUBuffer CreateGPUBuffer(VmaAllocator allocator, u64 size, VkBufferUsageFlags usage, VmaAllocationCreateFlags allocation_flags,
                          VmaMemoryUsage memory_usage) {
        VkBufferCreateInfo buffer_info{
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size  = size,
                .usage = usage,
        };

        VmaAllocationCreateInfo buffer_allocation_info{
                .flags = allocation_flags,
                .usage = memory_usage,
        };

        GPUBuffer buffer{};

        VkResult res = vmaCreateBuffer(allocator, &buffer_info, &buffer_allocation_info, &buffer.buffer, &buffer.allocation, &buffer.allocation_info);

        if (res != VK_SUCCESS) {
                std::println("Failed creating buffer");
                exit(res);
        }

        return buffer;
}

GPUBuffer CreateStagingBuffer(VmaAllocator allocator, u64 size) {
        return CreateGPUBuffer(allocator, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, GPUBuffer_STAGING);
}

GPUBuffer CreateReadbackBuffer(VmaAllocator allocator, u64 size) {
        return CreateGPUBuffer(allocator, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, GPUBuffer_READBACK);
}