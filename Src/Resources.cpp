#include "Resources.h"

#include <volk/volk.h>

#include <print>
#include <string.h>

void GPUBuffer::Print() {
        std::println("Buffer Info:");
        std::println("    Ptr = {}", (u64)buffer);
        std::println("    Memory Type = {}", (u64)allocation_info.memoryType);
        std::println("    Device Memory Ptr = {}", (u64)allocation_info.deviceMemory);
        std::println("    Size = {}", (u64)allocation_info.size);
        std::println("    Device Address = {}", (u64)device_address);
        std::println("    Queue Family  = {}", (u64)owning_queue_family);
}

// ===== DEBUG ===== //

u64 BUFFER_ALLOCATION_COUNT = 0;
u64 BUFFER_FREE_COUNT       = 0;
u64 BUFFER_ALLOCATION_MEM   = 0;

GPUBuffer CreateGPUBuffer(VkDevice vulkan_device, VmaAllocator allocator, u64 size, u32 owning_queue_family, VkBufferUsageFlags usage,
                          VmaAllocationCreateFlags allocation_flags, VmaMemoryUsage memory_usage, VkMemoryPropertyFlags memory_flags) {
        VkBufferCreateInfo buffer_info{
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size  = size,
                .usage = usage,
        };

        VmaAllocationCreateInfo buffer_allocation_info{
                .flags         = allocation_flags,
                .usage         = memory_usage,
                .requiredFlags = memory_flags,
        };

        GPUBuffer buffer{};

        VkResult res = vmaCreateBuffer(allocator, &buffer_info, &buffer_allocation_info, &buffer.buffer, &buffer.allocation, &buffer.allocation_info);

        if (res != VK_SUCCESS) {
                std::println("Failed creating buffer: size {} : {}", size, (i32)res);

                char* string;
                vmaBuildStatsString(allocator, &string, false);
                std::println("{}", string);

                exit(res);
        }

        buffer.owning_queue_family = owning_queue_family;

        VkSemaphoreCreateInfo semaphore_info{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        res = vkCreateSemaphore(vulkan_device, &semaphore_info, nullptr, &buffer.ownership_semaphore);
        if (res < 0) {
                std::println("Sem Failed: {}", (u32)res);
                exit(1);
        }

        res = vkCreateSemaphore(vulkan_device, &semaphore_info, nullptr, &buffer.transfer_semaphore);
        if (res < 0) {
                std::println("Sem Failed: {}", (u32)res);
                exit(1);
        }

        if ((usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) == VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
                VkBufferDeviceAddressInfo buffer_address_info{
                        .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                        .buffer = buffer.buffer,
                };

                buffer.device_address = vkGetBufferDeviceAddress(vulkan_device, &buffer_address_info);
        }

        BUFFER_ALLOCATION_MEM += (u64)buffer.allocation_info.size;
        BUFFER_ALLOCATION_COUNT++;
        // if (BUFFER_ALLOCATION_COUNT % 100 == 0) std::println("Buffer Allocation: {}, Allocations: {}, Frees: {}, Diff: {}", BUFFER_ALLOCATION_MEM,
        // BUFFER_ALLOCATION_COUNT, BUFFER_FREE_COUNT, BUFFER_ALLOCATION_COUNT - BUFFER_FREE_COUNT);

        if (BUFFER_ALLOCATION_MEM > 4'000'000'000) {
                std::println("Hit 4GB memory cap for buffers");
                exit(100);
        }

        return buffer;
}

void DestroyGPUBuffer(VkDevice vulkan_device, GPUBuffer& buffer, VmaAllocator vulkan_allocator) {
        vmaDestroyBuffer(vulkan_allocator, buffer.buffer, buffer.allocation);
        vkDestroySemaphore(vulkan_device, buffer.ownership_semaphore, nullptr);
        vkDestroySemaphore(vulkan_device, buffer.transfer_semaphore, nullptr);

        BUFFER_ALLOCATION_MEM -= (u64)buffer.allocation_info.size;
        BUFFER_FREE_COUNT++;
        // if (BUFFER_FREE_wCOUNT - BUFFER_FREE_COUNT);

        // Zero out memory so we dont store old refs.
        memset(&buffer, 0, sizeof(GPUBuffer));
}

GPUBuffer CreateStagingBuffer(VkDevice vulkan_device, VmaAllocator allocator, u64 size, u32 owning_queue_family) {
        return CreateGPUBuffer(vulkan_device, allocator, size, owning_queue_family, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               GPUBuffer_STAGING);
}

GPUBuffer CreateReadbackBuffer(VkDevice vulkan_device, VmaAllocator allocator, u64 size, u32 owning_queue_family) {
        return CreateGPUBuffer(vulkan_device, allocator, size, owning_queue_family, VK_BUFFER_USAGE_TRANSFER_DST_BIT, GPUBuffer_READBACK);
}

// ===== Transfer Functions ===== //

void TransferEngine::Init(VkDevice device, u32 transfer_queue_family_index, VmaAllocator allocator) {
        VkResult res{};

        // ===== Set Variables ===== //

        vulkan_allocator      = allocator;
        vulkan_device         = device;
        transfer_queue_family = transfer_queue_family_index;

        vkGetDeviceQueue(vulkan_device, transfer_queue_family, 0, &queue);

        // ===== Create Command Pool ===== //

        VkCommandPoolCreateInfo command_pool_info{
                .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = transfer_queue_family,
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
                .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };

        for (u32 i = 0; i < max_transfers_in_flight; i++) {
                res = vkCreateFence(vulkan_device, &fence_info, nullptr, &fences[i]);

                if (res != VK_SUCCESS) {
                        std::println("Failed creating transfer fence");
                        exit(res);
                }

                // ===== Mark Fences As Not In Use ===== //
                fence_in_use[i] = false;
        }

        // ===== Allocate Staging Buffer ===== //

        staging_buffer = CreateStagingBuffer(vulkan_device, vulkan_allocator, staging_buffer_total_size, transfer_queue_family);

        free_staging_buffer_region_count = 1;
        free_staging_buffer_regions[0]   = {
                  .block_offset = 0,
                  .block_count  = staging_buffer_block_count,
        };

        // ===== Initialise Job Queue ===== //

        next_free_job = 0;
}

void TransferEngine::Shutdown() {
        vkDeviceWaitIdle(vulkan_device);

        vkDestroyCommandPool(vulkan_device, command_pool, nullptr);

        for (u32 i = 0; i < max_transfers_in_flight; i++) {
                vkDestroyFence(vulkan_device, fences[i], nullptr);
        }

        DestroyGPUBuffer(vulkan_device, staging_buffer, vulkan_allocator);
}

void TransferEngine::Update() {
        // std::println("Update");

        VkResult res{};

        TransferJobID free_job                    = next_free_job;
        u32           last_checked_resource_index = 0;

        for (u32 job_index = 0; job_index < transfer_jobs.size(); job_index++) {
                if (job_index == free_job) {
                        free_job = transfer_jobs[job_index].next;
                        continue;
                }

                // ===== Find a Free Fence ===== //

                u32 resource_index = u32_max;

                for (u32 i = last_checked_resource_index; i < max_transfers_in_flight; i++) {
                        last_checked_resource_index = i;

                        if (fence_in_use[i]) continue;

                        // std::println("Fence: {}", i);
                        res = vkGetFenceStatus(vulkan_device, fences[i]);

                        if (res == VK_SUCCESS) {
                                resource_index = i;
                                break;
                        } else if (res != VK_NOT_READY) {
                                std::println("Fence in use should be marked but it is not! There is a bug.");
                                exit(1);
                        }
                }

                // If all resources in use
                if (resource_index == u32_max) break;

                // ===== If Copy Dont need Staging ===== //

                if (transfer_jobs[job_index].type == TransferJobType::Read) {
                        BufferRegion buffer_region = GetStagingBufferRegion(transfer_jobs[job_index].read_job.size);

                        if (buffer_region.block_count == 0) continue;

                        transfer_jobs[job_index].read_job.staging_buffer_region = buffer_region;

                } else if (transfer_jobs[job_index].type == TransferJobType::Write) {
                        BufferRegion buffer_region = GetStagingBufferRegion(transfer_jobs[job_index].write_job.size);

                        if (buffer_region.block_count == 0) continue;

                        transfer_jobs[job_index].write_job.staging_buffer_region = buffer_region;
                }

                TransferJob& job   = transfer_jobs[job_index];
                job.transfer_index = resource_index;

                // ===== Reset fence ===== //

                res = vkResetFences(vulkan_device, 1, &fences[job.transfer_index]);

                if (res != VK_SUCCESS) {
                        std::println("Failed resetting transfer fence: {}", i32(res));
                        exit(1);
                }

                // ===== Get Command Buffer ===== //

                VkCommandBuffer command_buffer = command_buffers[job.transfer_index];
                res                            = vkResetCommandBuffer(command_buffer, 0);

                // ===== Start Job ===== //

                switch (job.type) {
                        case TransferJobType::Copy:
                                {
                                        std::println("Copy not yet implemented");
                                        exit(1);
                                }
                                break;
                        case TransferJobType::Read:
                                {
                                        GPUBufferRead(command_buffer, job);
                                }
                                break;
                        case TransferJobType::Write:
                                {
                                        GPUBufferWrite(command_buffer, job);
                                }
                                break;
                }

                // ===== Set Vars ===== //

                fence_in_use[job.transfer_index] = true;
                last_checked_resource_index++;
        }
}

// Function to release ownership of a buffer from a queue. command buffer must have been started.
// TODO: Pass access and stage in.
void CmdReleaseBufferOwnership(BufferOwnershipInfo& info) {
        VkBufferMemoryBarrier2 release_barrier{
                .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                .srcStageMask        = info.stage_mask,
                .srcAccessMask       = info.access_mask,
                .dstStageMask        = VK_PIPELINE_STAGE_2_NONE, // Unused
                .dstAccessMask       = VK_ACCESS_NONE,           // Unused
                .srcQueueFamilyIndex = info.old_queue_family,
                .dstQueueFamilyIndex = info.new_queue_family,
                .buffer              = info.buffer->buffer,
                .offset              = 0,
                .size                = VK_WHOLE_SIZE,
        };

        VkDependencyInfo release_dependency_info{
                .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .bufferMemoryBarrierCount = 1,
                .pBufferMemoryBarriers    = &release_barrier,
        };

        vkCmdPipelineBarrier2(info.command_buffer, &release_dependency_info);

        info.buffer->owning_queue_family = u32_max;
}

void CmdAcquireBufferOwnership(BufferOwnershipInfo& info) {
        VkBufferMemoryBarrier2 acquire_barrier{
                .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                .dstStageMask        = info.stage_mask,
                .dstAccessMask       = info.access_mask,
                .srcQueueFamilyIndex = info.old_queue_family,
                .dstQueueFamilyIndex = info.new_queue_family,
                .buffer              = info.buffer->buffer,
                .offset              = 0,
                .size                = VK_WHOLE_SIZE,
        };

        VkDependencyInfo acquire_dependency_info{
                .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .bufferMemoryBarrierCount = 1,
                .pBufferMemoryBarriers    = &acquire_barrier,
        };

        vkCmdPipelineBarrier2(info.command_buffer, &acquire_dependency_info);

        info.buffer->owning_queue_family = info.new_queue_family;
}

BufferRegion TransferEngine::GetStagingBufferRegion(u64 size) {
        u32 blocks_needed = u32((size + (staging_buffer_block_size - 1)) / staging_buffer_block_size);

        BufferRegion buffer_region{};

        for (u32 i = 0; i < free_staging_buffer_region_count; i++) {
                BufferRegion& free_buffer_region = free_staging_buffer_regions[i];

                if (free_buffer_region.block_count < blocks_needed) continue;

                buffer_region.block_count  = blocks_needed;
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

                memmove(&free_staging_buffer_regions[1], &free_staging_buffer_regions[0], sizeof(BufferRegion) * free_staging_buffer_region_count);
                free_staging_buffer_regions[0] = buffer_region;
                free_staging_buffer_region_count++;
                return;
        }

        bool one_before_joins = free_staging_buffer_regions[i - 1].block_count + free_staging_buffer_regions[i - 1].block_offset == buffer_region.block_offset;
        bool one_after_joins  = free_staging_buffer_regions[i].block_offset == buffer_region.block_offset + buffer_region.block_count;

        // Somewhere in the middle
        // Check behind first
        // If region before connects to allocation block we join
        if (one_before_joins) {
                free_staging_buffer_regions[i - 1].block_count += buffer_region.block_count;

                // If the one infront also joins join all into one.
                if (one_after_joins) {
                        free_staging_buffer_regions[i - 1].block_count += free_staging_buffer_regions[i].block_count;

                        u64 bytes_to_copy = sizeof(BufferRegion) * (free_staging_buffer_region_count - (i + 1));
                        memmove(&free_staging_buffer_regions[i], &free_staging_buffer_regions[i + 1], bytes_to_copy);
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
        memmove(&free_staging_buffer_regions[i + 1], &free_staging_buffer_regions[i], bytes_to_copy);
        free_staging_buffer_region_count++;
}

void TransferEngine::GPUBufferRead(VkCommandBuffer command_buffer, const TransferJob& job) {
        // std::println("GPUBufferRead Called");
        VkResult res{};

        // ===== Begin Command Buffer ===== //

        VkCommandBufferBeginInfo command_buffer_begin_info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        res = vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);

        // ===== Acquire On Transfer Queue ===== //

        // NOTE: Need to check if acquire is needed. If this is first time using the buffer we can skip this.
        // Can we check this from the buffers owning queue? means we have to manage this manually, but should only need to
        // change on creation and on transfer, both which are easy to automate.

        BufferOwnershipInfo acquire_ownership_info{
                .command_buffer   = command_buffer,
                .buffer           = job.read_job.src,
                .old_queue_family = job.read_job.src_queue_family,
                .new_queue_family = transfer_queue_family,
                .stage_mask       = VK_PIPELINE_STAGE_2_COPY_BIT,
                .access_mask      = VK_ACCESS_2_TRANSFER_READ_BIT,
        };

        bool acquired_buffer = false;
        if (job.read_job.src->owning_queue_family != transfer_queue_family) {
                CmdAcquireBufferOwnership(acquire_ownership_info);
                acquired_buffer = true;
        }

        // If we acquire we make sure the correct queue family has acquired
        // If the buffer was not released then we need to make sure the buffer was in the tranfer family already
        // assert(job.read_job.src->owning_queue_family == transfer_queue_family && "Make sure you release buffers from other queue families before transfer.");

        // ===== Record Copy Commands ===== //

        VkBufferCopy copy_region{
                .srcOffset = job.read_job.offset,
                .dstOffset = job.read_job.staging_buffer_region.GetOffset(),
                .size      = job.read_job.size,
        };

        vkCmdCopyBuffer(command_buffer, job.read_job.src->buffer, staging_buffer.buffer, 1, &copy_region);

        // ===== Release Ownership ===== //

        // Swap new and old
        acquire_ownership_info.old_queue_family = transfer_queue_family;
        acquire_ownership_info.new_queue_family = job.read_job.dst_queue_family;

        bool released_semaphore = false;
        if (job.read_job.dst_queue_family != transfer_queue_family) {
                CmdReleaseBufferOwnership(acquire_ownership_info);
                released_semaphore = true;
        }

        // ===== End Command Buffer ===== //

        vkEndCommandBuffer(command_buffer);

        // ===== Submit ====== //


        std::vector<VkSemaphoreSubmitInfo> wait_semaphore_infos;

        // Make sure we only wait on the release of the buffer if it was actually released
        if (acquired_buffer) {
                VkSemaphoreSubmitInfo wait_semaphore_info{
                        .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                        .semaphore   = job.read_job.src->ownership_semaphore,
                        .stageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                        .deviceIndex = 0, // Must be 0 if not using a device group.
                };

                wait_semaphore_infos.push_back(wait_semaphore_info);
        }

        VkSemaphoreSubmitInfo signal_semaphore_info{
                .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore   = job.read_job.src->ownership_semaphore,
                .stageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                .deviceIndex = 0, // Must be 0 if not using a device group.
        };

        u32 signal_count = 0;
        if (released_semaphore) {
                signal_count++;
        }

        VkCommandBufferSubmitInfo command_buffer_submit_info{
                .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .commandBuffer = command_buffer,
                .deviceMask    = 0,
        };

        VkSubmitInfo2 submit_info{
                .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .waitSemaphoreInfoCount   = (u32)wait_semaphore_infos.size(),
                .pWaitSemaphoreInfos      = wait_semaphore_infos.data(),
                .commandBufferInfoCount   = 1,
                .pCommandBufferInfos      = &command_buffer_submit_info,
                .signalSemaphoreInfoCount = signal_count,
                .pSignalSemaphoreInfos    = &signal_semaphore_info,
        };

        res = vkQueueSubmit2(queue, 1, &submit_info, fences[job.transfer_index]);

        if (res != VK_SUCCESS) {
                std::println("Failed submitting on transfer queue for buffer read");
                exit(res);
        }
}

// u64 for size and offset?
void TransferEngine::GPUBufferWrite(VkCommandBuffer command_buffer, const TransferJob& job) {
        // std::println("GPUBufferWrite Called");

        VkResult res{};

        // ===== Begin Command Buffer ===== //

        VkCommandBufferBeginInfo command_buffer_begin_info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        res = vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);

        // ===== Acquire On Transfer Queue ===== //

        BufferOwnershipInfo acquire_ownership_info{
                .command_buffer   = command_buffer,
                .buffer           = job.write_job.dst,
                .old_queue_family = job.write_job.src_queue_family,
                .new_queue_family = transfer_queue_family,
                .stage_mask       = VK_PIPELINE_STAGE_2_COPY_BIT,
                .access_mask      = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        };

        bool acquired_buffer = false;

        if (job.write_job.dst->owning_queue_family != transfer_queue_family) {
                CmdAcquireBufferOwnership(acquire_ownership_info);
                acquired_buffer = true;
        }

        // ===== Copy Data To Staging Buffer ===== //

        u8* staging_ptr = (u8*)staging_buffer.allocation_info.pMappedData;
        memcpy(&staging_ptr[job.write_job.staging_buffer_region.GetOffset()], job.write_job.src, job.write_job.size);

        // ===== Record Copy Command ===== //

        VkBufferCopy copy_region{
                .srcOffset = job.write_job.staging_buffer_region.GetOffset(),
                .dstOffset = job.write_job.offset,
                .size      = job.write_job.size,
        };

        vkCmdCopyBuffer(command_buffer, staging_buffer.buffer, job.write_job.dst->buffer, 1, &copy_region);

        // ===== Release Ownership ===== //

        // Swap new and old
        acquire_ownership_info.old_queue_family = transfer_queue_family;
        acquire_ownership_info.new_queue_family = job.write_job.dst_queue_family;

        bool released_buffer = false;
        if (job.write_job.dst_queue_family != transfer_queue_family) {
                CmdReleaseBufferOwnership(acquire_ownership_info);
                released_buffer = true;
        }

        // ===== End Command Buffer ===== //

        vkEndCommandBuffer(command_buffer);

        // ===== Submit ===== //

        std::vector<VkSemaphoreSubmitInfo> wait_semaphore_infos;

        // Make sure we only wait on the release of the buffer if it was actually released
        if (acquired_buffer) {
                VkSemaphoreSubmitInfo wait_semaphore_info{
                        .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                        .semaphore   = job.write_job.dst->ownership_semaphore,
                        .stageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                        .deviceIndex = 0, // Must be 0 if not using a device group.
                };

                wait_semaphore_infos.push_back(wait_semaphore_info);
        }

        VkSemaphoreSubmitInfo signal_semaphore_info{
                .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore   = job.write_job.dst->ownership_semaphore,
                .stageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                .deviceIndex = 0, // Must be 0 if not using a device group.
        };

        u32 signal_count = 0;
        if (released_buffer) {
                signal_count++;
        }

        VkCommandBufferSubmitInfo command_buffer_submit_info{
                .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .commandBuffer = command_buffer,
                .deviceMask    = 0,
        };

        VkSubmitInfo2 submit_info{
                .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .waitSemaphoreInfoCount   = (u32)wait_semaphore_infos.size(),
                .pWaitSemaphoreInfos      = wait_semaphore_infos.data(),
                .commandBufferInfoCount   = 1,
                .pCommandBufferInfos      = &command_buffer_submit_info,
                .signalSemaphoreInfoCount = signal_count,
                .pSignalSemaphoreInfos    = &signal_semaphore_info,
        };

        res = vkQueueSubmit2(queue, 1, &submit_info, fences[job.transfer_index]);

        if (res != VK_SUCCESS) {
                std::println("Failed submitting on transfer queue for buffer read");
                exit(res);
        }
}

TransferJobID TransferEngine::GetJob() {
        if (next_free_job >= transfer_jobs.size()) {
                transfer_jobs.push_back({});

                TransferJobID id = next_free_job;
                next_free_job++;

                transfer_jobs[id].id = id;

                return id;
        }

        TransferJob& job = transfer_jobs[next_free_job];

        next_free_job = job.next;
        job           = { .id = job.id };

        return job.id;
}

void TransferEngine::FreeJob(TransferJobID id) {
        TransferJobID free_job = next_free_job;

        if (id < free_job) {
                transfer_jobs[id].next = free_job;
                next_free_job          = id;
                return;
        }

        while (id > transfer_jobs[free_job].next) {
                free_job = transfer_jobs[free_job].next;
        }

        transfer_jobs[id].next       = transfer_jobs[free_job].next;
        transfer_jobs[free_job].next = id;
}

TransferJobID TransferEngine::QueueGPUBufferRead(GPUBufferReadInfo read_job) {
        if (read_job.size > staging_buffer_total_size) {
                // NOTE: This can be fixed by splitting the job.
                std::println("Trying to read memory which is larger than the staging buffer");
                exit(1);
        }

        TransferJobID job_index = GetJob();
        TransferJob&  job       = transfer_jobs[job_index];

        job.type     = TransferJobType::Read;
        job.read_job = read_job;

        job.transfer_index = u32_max;

        return job.id;
}

TransferJobID TransferEngine::QueueGPUBufferWrite(GPUBufferWriteInfo write_job) {
        if (write_job.size > staging_buffer_total_size) {
                // NOTE: This can be fixed by splitting the job.
                std::println("Trying to write memory which is larger than the staging buffer");
                exit(1);
        }

        u32          job_index = GetJob();
        TransferJob& job       = transfer_jobs[job_index];

        job.type      = TransferJobType::Write;
        job.write_job = write_job;

        job.transfer_index = u32_max;

        return job.id;
}

bool TransferEngine::Check(TransferJobID id) {

        TransferJob& job = transfer_jobs[id];

        if (job.transfer_index == u32_max) return false; // Not queued yet!

        VkFence fence = fences[job.transfer_index];

        // std::println("Checking ID: {}", id);
        // Ahh, we need to see if we actually queued the job yet.
        VkResult res = vkGetFenceStatus(vulkan_device, fence);

        if (res == VK_SUCCESS) {
                fence_in_use[job.transfer_index] = false;

                if (job.type == TransferJobType::Read) {
                        u8* staging_ptr = (u8*)staging_buffer.allocation_info.pMappedData;
                        memcpy(job.read_job.dst, &staging_ptr[job.read_job.staging_buffer_region.GetOffset()], job.read_job.size);
                        ReleaseStagingBufferRegion(job.read_job.staging_buffer_region);
                } else if (job.type == TransferJobType::Write) {
                        ReleaseStagingBufferRegion(job.write_job.staging_buffer_region);
                }

                FreeJob(id);

                return true;
        } else if (res == VK_NOT_READY) {
                return false;
        } else {
                std::println("Failed Getting transfer fence status");
                exit(res);
        }

        return false;
}