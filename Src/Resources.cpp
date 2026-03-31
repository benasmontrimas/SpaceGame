#include "Resources.h"

#include <volk/volk.h>

#include <print>

GPUBuffer CreateGPUBuffer(VmaAllocator allocator, u64 size, VkBufferUsageFlags usage, VmaAllocationCreateFlags allocation_flags, VmaMemoryUsage memory_usage) {
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

// ===== Transfer Functions ===== //

void TransferEngine::Init(VkDevice device, u32 transfer_queue_family_index, VmaAllocator allocator) {
        VkResult res;

        // ===== Set Variables ===== //

        vulkan_allocator   = allocator;
        vulkan_device      = device;
        queue_family_index = transfer_queue_family_index;

        res = vkGetDeviceQueue(vulkan_device, queue_family_index, 1, &queue);

        if (res != VK_SUCCESS) {
                std::println("Failed to get device queues for tranfer engine");
                exit(res);
        }

        // ===== Create Command Pool ===== //

        VkCommandPoolCreateInfo command_pool_info{
                .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = queue_family_index,
        };

        res = vkCreateCommandPool(vulkan_device, &command_pool_info, nullptr, &command_pool);

        if (res != VK_SUCCESS) {
                std::println("Failed to create transfer command pool");
                exit(res);
        }

        // ===== Allocate Command Buffers ===== //

        VkCommandBufferAllocateInfo command_buffer_allocate_info{
                .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool        = command_pool,
                .commandBufferCount = max_transfers_in_flight,
        };

        res = vkAllocateCommandBuffers(vulkan_device, &command_buffer_allocate_info, &command_buffers[0]);

        if (res != VK_SUCCESS) {
                std::println("Failed to allocate transfer command buffers");
                exit(res);
        }

        // ===== Create Fences ===== //

        VkFenceCreateInfo fence_info{
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = 0,
        };

        for (u32 i = 0; i < max_transfers_in_flight; i++) {
                res = vkCreateFence(vulkan_device, &fence_info, nullptr, &compute_fences[0]);

                if (res != VK_SUCCESS) {
                        std::println("Failed creating fence");
                        exit(res);
                }

                // ===== Mark Fences As Not In Use ===== //
                fence_in_use[i] = false;
        }

        // ===== Allocate Staging Buffer ===== //

        staging_buffer = CreateStagingBuffer(vulkan_allocator, staging_buffer_total_size);

        free_staging_buffer_region_count = 1;
        free_staging_buffer_regions[0] = {
                .block_offset = 0,
                .block_count = staging_buffer_block_count,
        };
}

// Function to release ownership of a buffer from a queue. command buffer must have been started.
// TODO: Pass access and stage in.
void CmdReleaseBufferOwnership(const BufferOwnershipInfo& info) {
        VkBufferMemoryBarrier2 release_barrier{
                .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                .srcStageMask        = info.stage_mask,
                .srcAccessMask       = info.access_mask,
                .dstStageMask        = VK_PIPELINE_STAGE_2_NONE, // Unused
                .dstAccessMask       = VK_ACCESS_NONE,           // Unused
                .srcQueueFamilyIndex = info.old_queue_family,
                .dstQueueFamilyIndex = info.new_queue_family,
                .buffer              = info.buffer.buffer,
                .offset              = 0,
                .size                = VK_WHOLE_SIZE,
        };

        VkDependencyInfo release_dependency_info{
                .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .bufferMemoryBarrierCount = 1,
                .pBufferMemoryBarriers    = &release_barrier,
        };

        vkCmdPipelineBarrier2(info.command_buffer, &release_dependency_info);
}

void CmdAcquireBufferOwnership(const BufferOwnershipInfo& info) {
        VkBufferMemoryBarrier2 acquire_barrier{
                .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                .srcStageMask        = VK_PIPELINE_STAGE_2_NONE, // Unused
                .srcAccessMask       = VK_ACCESS_NONE,           // Unused
                .dstStageMask        = info.stage_mask,
                .dstAccessMask       = info.access_mask,
                .srcQueueFamilyIndex = info.old_queue_family,
                .dstQueueFamilyIndex = info.new_queue_family,
                .buffer              = info.buffer.buffer,
                .offset              = 0,
                .size                = VK_WHOLE_SIZE,
        };

        VkDependencyInfo acquire_dependency_info{
                .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .bufferMemoryBarrierCount = 1,
                .pBufferMemoryBarriers    = &acquire_barrier,
        };

        vkCmdPipelineBarrier2(info.command_buffer, &acquire_dependency_info);
}

BufferRegion TransferEngine::GetStagingBufferRegion(u64 size) {
        u32 blocks_needed = u32((size + (staging_buffer_block_size - 1)) / staging_buffer_block_size);

        BufferRegion buffer_region {};

        for (u32 i = 0; i < free_staging_buffer_region_count; i++) {
                BufferRegion& free_buffer_region = free_staging_buffer_regions[i];

                if (free_buffer_region.block_count < blocks_needed) continue;

                buffer_region.block_count = blocks_needed;
                buffer_region.block_offset = free_buffer_region.block_offset;

                // ===== Reduce Region ===== //

                free_buffer_region.block_offset += blocks_needed;
                free_buffer_region.block_count -= blocks_needed;

                if (free_buffer_region.block_count > 0) break;

                // ===== Remove Empty Region ===== //

                u64 bytes_to_copy = sizeof(BufferRegion) * (free_staging_buffer_region_count - (i + 1));
                memmove(&free_staging_buffer_regions[i], &free_staging_buffer_regions[i + 1], bytes_to_copy);

                free_staging_buffer_region_count--;
        }

        return buffer_region;
}

void TransferEngine::ReleaseStagingBufferRegion(BufferRegion buffer_region) {
        // NOTE: can do binary search here. but realistically how many free regions are we having?
        // Also there is definitly a better way to do this but this will work for now.
        // TODO: Can just check if front and back join, behaviour is the same if it doesnt join and if there isnt one there.

        if (free_staging_buffer_region_count == 0) {
                free_staging_buffer_regions[0] = buffer_region;
                free_staging_buffer_region_count++;
                return;
        }

        // Need to check both sides of the region to see if we can append.
        // Find first after buffer region

        u32 i;
        for (i = 0; i < free_staging_buffer_region_count; i++) {
                if (free_staging_buffer_regions[i].block_offset > buffer_region.block_offset) break;
        }

        // This is last block
        if (i == free_staging_buffer_region_count) {
                if (free_staging_buffer_regions[i - 1].block_offset + free_staging_buffer_regions[i - 1].block_count == buffer_region.block_offset) {
                        free_staging_buffer_regions[i - 1].block_count += buffer_region.block_count;
                        return;
                }

                free_staging_buffer_regions[i] = buffer_region;
                free_staging_buffer_region_count++;
                return;
        }

        // This is the first block
        if (i == 0) {
                if (free_staging_buffer_regions[i].block_offset == buffer_region.block_offset + buffer_region.block_count) {
                        free_staging_buffer_regions[i].block_offset = buffer_region.block_offset;
                        free_staging_buffer_regions[i].block_count += buffer_region.block_count;
                        return;
                }

                memmove(free_staging_buffer_regions[1], free_staging_buffer_regions[0], sizeof(BufferRegion) * free_staging_buffer_region_count);
                free_staging_buffer_regions[0] = buffer_region;
                free_staging_buffer_region_count++;
                return;
        }

        bool one_before_joins = free_staging_buffer_regions[i - 1].block_count + free_staging_buffer_regions[i - 1].block_offset == buffer_region.block_offset;
        bool one_after_joins = free_staging_buffer_regions[i].block_offset == buffer_region.block_offset + buffer_region.block_count;

        // Somewhere in the middle
        // Check behind first
        // If region before connects to allocation block we join
        if (one_before_joins) {
                free_staging_buffer_regions[i - 1].block_count += buffer_region.block_count;

                // If the one infront also joins join all into one.
                if (one_after_joins) {
                        free_staging_buffer_regions[i - 1].block_count += free_staging_buffer_regions[i].block_count;

                        u64 bytes_to_copy = sizeof(BufferRegion) * (free_staging_buffer_region_count - (i + 1));
                        memmove(free_staging_buffer_regions[i], free_staging_buffer_regions[i + 1], bytes_to_copy);
                        free_staging_buffer_region_count--;
                        return;
                }

                return;
        }

        // Check Front
        if (one_after_joins) {
                free_staging_buffer_regions[i].block_offset = buffer_region.block_offset;
                free_staging_buffer_regions[i].block_count += buffer_region.block_count;
                return;
        }

        // Add new region at i
        u64 bytes_to_copy = sizeof(BufferRegion) * (free_staging_buffer_region_count - i);
        memmove(free_staging_buffer_regions[i + 1], free_staging_buffer_regions[i], bytes_to_copy);
        free_staging_buffer_region_count++;
}

// NOTE: Its rare we will want to read data back from buffers, usually we just want to transfer ownership to use it straight away
// // or transfer to be able to write to it.
void TransferEngine::ReadGPUBuffer(ReadTransferJob read_job) {
        VkResult res;

        // ===== Find a Free Fence ===== //

        bool found = false;
        u32  index;

        for (u32 i = 1; i <= max_transfers_in_flight; i++) {
                index = (transfer_index + i) % max_transfers_in_flight;

                if (fence_in_use[index]) continue;

                res = vkGetFenceStatus(vulkan_device, fences[index]);

                if (res == VK_SUCCESS) {
                        found = true;
                        break;
                } else if (res != VK_NOT_READY) {
                        std::println("Failed checking transfer fence status");
                        exit(res);
                }
        }

        if (!found) {
                std::println("Failed to find a free transfer fence, this is a temporary issue until I add a queue");
                exit(1);
        }

        // ===== Get Staging Buffer Segment ===== //

        // TODO
        BufferRegion buffer_region = GetStagingBufferRegion(read_job.size);

        if (buffer_region.block_count == 0) {
                std::println("Failed to find staging buffer region which is big enough");
                exit(1);
        }

        transfer_index = index;

        // ===== Reset fence ===== //

        res = vkResetFences(vulkan_device, 1, &fences[transfer_index]);

        if (res != VK_SUCCESS) {
                std::println("Failed resetting transfer fence");
                exit(res);
        }

        // ===== Get Command Buffer ===== //

        VkCommandBuffer command_buffer = command_buffers[transfer_index];

        // ===== Begin Command Buffer ===== //

        VkCommandBufferBeginInfo command_buffer_begin_info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        res = vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);

        // ===== Acquire On Transfer Queue ===== //

        BufferOwnershipInfo acquire_ownership_info{
                .command_buffer   = command_buffer,
                .buffer           = read_job.src,
                .old_queue_family = read_job.src_queue_family,
                .new_queue_family = queue_family_index,
                .stage_mask       = VK_PIPELINE_STAGE_2_COPY_BIT,
                .access_mask      = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        };

        CmdAcquireBufferOwnership(acquire_ownership_info);

        // ===== Specify Copy Regions ===== //

        VkBufferCopy copy_region{
                .srcOffset = read_job.offset,
                .dstOffset = buffer_region.GetOffset(),
                .size      = read_job.size,
        };

        // ===== Record Copy Commands ===== //

        vkCmdCopyBuffer(command_buffer, src.buffer, staging_buffer.buffer, 1, &copy_region);

        // ===== Release Ownership ===== //

        // Swap new and old
        acquire_ownership_info.new_queue_family = acquire_ownership_info.old_queue_family;
        acquire_ownership_info.old_queue_family = queue_family_index;

        CmdReleaseBufferOwnership(acquire_ownership_info);

        // ===== End Command Buffer ===== //

        vkEndCommandBuffer(command_buffer);

        // ===== Submit ====== //

        VkSemaphoreSubmitInfo wait_semaphore_info{
                .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore   = read_job.buffer.ownership_semaphore,
                .stageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                .deviceIndex = 0, // Must be 0 if not using a device group.
        };

        VkSemaphoreSubmitInfo signal_semaphore_info{
                .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore   = read_job.buffer.transfer_semaphore,
                .stageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                .deviceIndex = 0, // Must be 0 if not using a device group.
        };

        VkCommandBufferSubmitInfo command_buffer_submit_info{
                .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .commandBuffer = command_buffer,
                .deviceMask    = 0,
        };

        VkSubmitInfo2 submit_info{
                .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .waitSemaphoreInfoCount   = 1,
                .pWaitSemaphoreInfos      = &wait_semaphore_info,
                .commandBufferInfoCount   = 1,
                .pCommandBufferInfos      = &command_buffer_submit_info,
                .signalSemaphoreInfoCount = 1,
                .pSignalSemaphoreInfos    = &signal_semaphore_info,
        };

        res = vkQueueSubmit2(queue, 1, &submit_info, fences[transfer_index]);

        if (res != VK_SUCCESS) {
                std::println("Failed submitting on transfer queue for buffer read");
                exit(res);
        }

        // ===== Update Job Info ===== //

        read_job.transfer_index      = transfer_index;
        fence_in_use[transfer_index] = true;
}