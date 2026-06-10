#include "Planet.h"
#include "Game.h"

void Planet::Init(GameContext& game_context) {
        vulkan_device         = game_context.vulkan_device;
        vulkan_allocator      = game_context.vulkan_allocator;
        compute_queue         = game_context.compute_queue;
        compute_queue_family  = game_context.compute_queue_family;
        transfer_queue_family = game_context.transfer_queue_family;
        transfer_engine       = game_context.transfer_engine;

        VkResult res;

        // ===== Create Command Pool ===== //

        VkCommandPoolCreateInfo command_pool_info{
                .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = compute_queue_family,
        };

        res = vkCreateCommandPool(vulkan_device, &command_pool_info, nullptr, &command_pool);
        if (res != VK_SUCCESS) exit(res);

        // ===== Allocate Command Buffers ===== //

        VkCommandBufferAllocateInfo compute_command_buffer_allocate_info{
                .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool        = command_pool,
                .commandBufferCount = command_buffer_count,
        };

        res = vkAllocateCommandBuffers(vulkan_device, &compute_command_buffer_allocate_info, &command_buffers[0]);
        if (res != VK_SUCCESS) exit(res);

        u32 free_slots = command_buffer_count / 2; // Each slot allows 2 command buffers
        free_command_buffers.reserve(free_slots);
        for (u32 i = 0; i < command_buffer_count; i += 2) {
                free_command_buffers.push_back((command_buffer_count - 2) - i);
        }

        // ===== Create Pipeline Layout ===== //

        VkPushConstantRange push_constant_range{
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .size       = sizeof(PlanetCreationInfo),
        };

        VkPipelineLayoutCreateInfo pipeline_layout_info{
                .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount         = 0,
                .pSetLayouts            = nullptr,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges    = &push_constant_range,
        };

        res = vkCreatePipelineLayout(vulkan_device, &pipeline_layout_info, nullptr, &pipeline_layout);
        if (res != VK_SUCCESS) exit(res);

        // ===== Load Shaders ===== //

        Slang::ComPtr<slang::IBlob>   diagnostics;
        Slang::ComPtr<slang::IModule> slang_module{
                game_context.slang_session->loadModuleFromSource("MarchingCubes", "Assets/Shaders/MarchingCubes.slang", nullptr, diagnostics.writeRef()),
        };

        if (diagnostics) {
                std::println("Slang Error: {}", (const char*)diagnostics->getBufferPointer());
                exit(1);
        }

        Slang::ComPtr<ISlangBlob> spirv;
        slang_module->getTargetCode(0, spirv.writeRef());

        VkShaderModuleCreateInfo shader_module_info{
                .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .codeSize = spirv->getBufferSize(),
                .pCode    = (u32*)spirv->getBufferPointer(),
        };

        VkShaderModule shader_module{};
        res = vkCreateShaderModule(vulkan_device, &shader_module_info, nullptr, &shader_module);

        if (res != VK_SUCCESS) {
                std::println("Failed creating compute shader module");
                exit(res);
        }

        // ===== Create Pipelines ===== //

        constexpr const char* entry_point_names[pass_count] = {
                "Pass1",
                "Pass2",
                "Pass3",
                "Pass4",
        };

        VkComputePipelineCreateInfo pipeline_infos[pass_count];

        for (int i = 0; i < pass_count; i++) {
                VkPipelineShaderStageCreateInfo shader_stage_info{
                        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                        .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
                        .module = shader_module,
                        .pName  = entry_point_names[i],
                };

                pipeline_infos[i] = {
                        .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                        .stage  = shader_stage_info,
                        .layout = pipeline_layout,
                };
        }

        vkCreateComputePipelines(vulkan_device, VK_NULL_HANDLE, pass_count, &pipeline_infos[0], nullptr, &pipelines[0]);

        // ===== Create Constant Buffers ===== //

        VmaAllocationCreateFlags allocation_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                                    VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        // == Edge Lookup == //

        edge_lookup_buffer = CreateGPUBuffer(vulkan_device, vulkan_allocator, sizeof(int) * 256, transfer_queue_family,
                                             VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, allocation_flags);

        VkBufferDeviceAddressInfo edge_lookup_buffer_address_info{
                .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .buffer = edge_lookup_buffer.buffer,
        };

        edge_lookup_buffer.device_address = vkGetBufferDeviceAddress(vulkan_device, &edge_lookup_buffer_address_info);

        // == Write Data == //

        GPUBufferWriteInfo edge_write_job_info{
                .src    = (u8*)&edge_table[0],
                .offset = 0,
                .size   = sizeof(int) * 256,
                .dst    = edge_lookup_buffer,

                .dst_queue_family = compute_queue_family,
        };

        TransferJobID edge_write_id = transfer_engine.QueueGPUBufferWrite(edge_write_job_info);

        // == Triangle Lookup == //

        triangle_lookup_buffer = CreateGPUBuffer(vulkan_device, vulkan_allocator, sizeof(int) * 256 * 16, transfer_queue_family,
                                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, allocation_flags);

        VkBufferDeviceAddressInfo triangle_lookup_buffer_address_info{
                .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .buffer = triangle_lookup_buffer.buffer,
        };

        triangle_lookup_buffer.device_address = vkGetBufferDeviceAddress(vulkan_device, &triangle_lookup_buffer_address_info);

        // == Write Data == //

        GPUBufferWriteInfo triangle_write_job_info{
                .src    = (u8*)&triangle_table[0][0],
                .offset = 0,
                .size   = sizeof(int) * 256 * 16,
                .dst    = triangle_lookup_buffer,

                .dst_queue_family = compute_queue_family,
        };

        TransferJobID triangle_write_id = transfer_engine.QueueGPUBufferWrite(triangle_write_job_info);

        // Ensure these buffers are written before trying to generate chunks.
        // Or can I use a semaphore to just set a semaphore to get waited on and the Transfer Engine doesnt keep track after finishing

        while (!transfer_engine.Check(edge_write_id)) {
                transfer_engine.Update();
        }

        while (!transfer_engine.Check(triangle_write_id)) {
                transfer_engine.Update();
        }

        // Need to acquire the buffers on the compute queue.
}

void Planet::Shutdown() {
        // TODO:
}

void Planet::Update() {
        // TODO: Need to actually check which chunks need to be loaded and unloaded.

        // Create list of all chunks which should be visible in order
        constexpr u32 chunk_load_range = 10'000;
        if (target) {
                Vec3 position = target->position;

                // Just do fixed number in area around player,
                for (u32 x = 0; x < 5; x++) {
                        for (u32 y = 0; y < 5; y++) {
                                for (u32 z = 0; z < 5; z++) {
                                        // How can we mark empty chunks to prevent adding them?
                                }
                        }
                }
        }

        // TODO: Check chunks to unload/reload at lower LOD
        for (u64 i = 0; i < chunks.size(); i++) {
                if (!target) break; // Cant check if no target given

                constexpr u32 lod0_distance = 100;
                constexpr u32 lod1_distance = 1000;
                // Do for as many lods as we want

                PlanetChunk& chunk = chunks[i];

                // Need to offset position to center
                Vec3 chunk_center = chunk.position + Vec3{32, 32, 32};
                // Can just use the squared values and square the lod distances to remove sqrt
                Vec3 distance = glm::magnitude(target.position - chunk_center);

                // If we use an array to store these values we can just loop instead of else if
                // But i guess how many lods would we want anyway? Maybe it doesnt matter
                if (distance > lod1_distance) {
                        // Unload
                } else if (distance > lod0_distance) {
                        if (chunk.lod == 1) continue; // Correct LOD

                        // TODO:
                        // Reload the chunk at the correct LOD level

                        // Loop adjacent lods to update their edges
                } else {
                        if (chunk.lod == 0) continue; // Correct LOD

                        // TODO:
                        // Reload the chunk at the correct LOD level

                        // Loop adjacent lods to update their edges
                }
        }

        // TODO: Check chunks to load
        // How can we check easily if we already have a chunk loaded?
        // Loop all chunks in radius and spawn ones that are not already there?
        // Maybe just make an array of chunks by position, and when we loop above,
        // remove the chunks which are already loaded, then we can just spawn the rest
        // of the list here.

        // Could do some checks to see where we moved from last frame and we know which positions
        // we actually need to check.

        // ===== Check all in progress chunks if they are ready to go to next stage ===== //
        for (u64 i = 0; i < chunks_in_progress.size(); i++) {
                PlanetChunkProgress& chunk_progress = chunks_in_progress[i];
                // NOTE: Want to make sure we are queing all the commands on chunk creation
                // This means we dont need to worry about calling the same function twice,
                // Otherwise we need a Stage*Busy stage too, to ensure we know when a stage
                // is active vs ready to start.

                // NOTE: The None stage is the only one we want to call several times as we
                // can only record the commands once a command buffer is available.

                // NOTE: Move all submits into a single vkQueueSubmit call.

                // NOTE: Even better dont submit commands until after we have looped all, so that we can group
                // all commands into one submit.

                // Move to next stage
                switch (chunk_progress.GetStage(vulkan_device)) {
                case PlanetGenerationStage::None: {
                        // GPU: Nothing
                        // CPU: Find Command Buffer and Memory for Generation

                        int command_buffer_index = GetCommandBufferIndex();
                        if (command_buffer_index == -1) break;
                        chunk_progress.command_buffer_index = command_buffer_index;

                        // ===== Record Commands ===== //

                        RecordCommands(chunk_progress);

                        // ===== Signal Semaphore ===== //

                        VkSemaphoreSignalInfo signal_info;
                        signal_info.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
                        signal_info.pNext     = NULL;
                        signal_info.semaphore = chunk_progress.semaphore;
                        signal_info.value     = (u64)PlanetGenerationStage::StageOne;

                        vkSignalSemaphore(vulkan_device, &signal_info);
                } break;
                case PlanetGenerationStage::StageOne: {
                        // GPU: Count Triangles and Vertices
                        // CPU: Nothing
                } break;
                case PlanetGenerationStage::StageTwo: {
                        // GPU: Nothing
                        // CPU: Sum triangle and vertex offsets

                        StartStageTwo(chunk_progress);
                } break;
                case PlanetGenerationStage::StageThree: {
                        // GPU: Write Output Data
                        // CPU: Nothing
                } break;
                case PlanetGenerationStage::Finished: {
                        // GPU: Nothing
                        // CPU: Free resource and add chunk to Planet.

                        FreeChunkInProgress(chunk_progress);
                        chunks_in_progress[i] = chunks_in_progress.back();
                        chunks_in_progress.pop_back();
                        i--;

                        // Add chunk to Planet
                } break;
                default:
                        std::println("Unexpected Stage in Planet Generation");
                        exit(1);
                }
        }
}

int Planet::GetCommandBufferIndex() {
        if (free_command_buffers.empty()) return -1;

        u32 index = free_command_buffers.back();
        free_command_buffers.pop_back();

        return index;
}

void Planet::ReleaseCommandBufferIndex(int index) {
        VkResult res = vkResetCommandBuffer(command_buffers[index], 0);
        if (res != VK_SUCCESS) exit(res);
        free_command_buffers.push_back(index);
}

void Planet::GenerateChunk(Vec3 position, u32 chunk_cells, u32 chunk_dims) {
        VkResult res;

        VkFence           fence;
        VkFenceCreateInfo fence_info{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = 0 };
        res = vkCreateFence(vulkan_device, &fence_info, nullptr, &fence);
        if (res != VK_SUCCESS) exit(res);

        VkSemaphore semaphore;

        VkSemaphoreTypeCreateInfo timeline_info{};
        timeline_info.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        timeline_info.pNext         = nullptr;
        timeline_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timeline_info.initialValue  = (u64)PlanetGenerationStage::None;

        VkSemaphoreCreateInfo semaphore_info{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                .pNext = &timeline_info,
        };

        res = vkCreateSemaphore(vulkan_device, &semaphore_info, nullptr, &semaphore);
        if (res != VK_SUCCESS) exit(res);

        // ===== Create Intermediate Buffers ===== //

        // NOTE: Can pre allocate and take from pool to save allocation work at runtime.
        // NOTE: Can use within budget allocation flag to only allocate if enough space, otherwise wait?
        /*
        - Edges : { u32 } : 67MB
        - EdgeSums : { u32, u32, u32, u32 } : 1MB
        - Triangle Normals : { f32 } : Unused right now
        */

        // == Useful Values == //

        u32 x_edges = chunk_cells;
        u32 y_edges = chunk_cells + 1;
        u32 z_edges = chunk_cells + 1;

        u32 edge_count = x_edges * y_edges * z_edges;
        u32 cell_count = chunk_cells * chunk_cells * chunk_cells;
        u32 row_count  = y_edges * z_edges;

        // == Edges Buffer - GPU read/write only == //

        VmaAllocationCreateFlags allocation_flags = 0;

        GPUBuffer edge_buffer = CreateGPUBuffer(vulkan_device, vulkan_allocator, sizeof(float) * edge_count, compute_queue_family,
                                                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, allocation_flags);

        VkBufferDeviceAddressInfo edge_buffer_address_info{
                .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .buffer = edge_buffer.buffer,
        };

        edge_buffer.device_address = vkGetBufferDeviceAddress(vulkan_device, &edge_buffer_address_info);

        // == Edge Sums Buffer - Host read == //

        allocation_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        GPUBuffer edge_sums_buffer = CreateGPUBuffer(vulkan_device, vulkan_allocator, sizeof(EdgeSumInfo) * row_count, compute_queue_family,
                                                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, allocation_flags);

        VkBufferDeviceAddressInfo edge_sums_buffer_address_info{
                .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .buffer = edge_sums_buffer.buffer,
        };

        edge_sums_buffer.device_address = vkGetBufferDeviceAddress(vulkan_device, &edge_sums_buffer_address_info);

        // ===== Initialise Creation Info ===== //

        PlanetCreationInfo info{
                .seed        = 0,
                .position    = position,
                .chunk_cells = chunk_cells,
                .chunk_dims  = chunk_dims,

                .edge_case_lookup = edge_lookup_buffer.device_address,
                .triangle_lookup  = triangle_lookup_buffer.device_address,

                .edges            = edge_buffer.device_address,
                .edge_sums        = edge_sums_buffer.device_address,
                .triangle_normals = 0, // TODO

                .indices  = 0, // Set later
                .vertices = 0, // Set later
        };

        PlanetChunk chunk{
                .vertex_count = 0,
                .index_count  = 0,

                .index_buffer  = VK_NULL_HANDLE,
                .vertex_buffer = VK_NULL_HANDLE,

                .lod       = 0,              // TODO,
                .edge_lods = { 0, 0, 0, 0 }, // TODO

                .index_offsets = nullptr, // Remove?
        };

        PlanetChunkProgress& chunk_in_progress = chunks_in_progress.emplace_back(chunk, info, edge_buffer, edge_sums_buffer, semaphore, fence);
}

void Planet::FreeChunkInProgress(PlanetChunkProgress& chunk_progress) {
        ReleaseCommandBufferIndex(chunk_progress.command_buffer_index);

        vkDestroySemaphore(vulkan_device, chunk_progress.semaphore, nullptr);
        vkDestroyFence(vulkan_device, chunk_progress.fence, nullptr);

        // Free intermediate buffers?
}

void Planet::RecordCommands(PlanetChunkProgress& chunk_progress) {
        VkSubmitInfo2 submit_infos[2];

        submit_infos[0] = RecordStageOne(chunk_progress);
        submit_infos[1] = RecordStageThree(chunk_progress);

        vkQueueSubmit2(compute_queue, 2, &submit_infos[0], VK_NULL_HANDLE);
}

// NOTE: I think this could easily have pre-recorded command buffers.

// NOTE: If i want to use pre recorded buffers I will need to move away from push constants and write to uniform buffer instead
// as push constants are written to the command buffer. But can use for Constant Data.

// Queue Pass 1 and Pass 2
VkSubmitInfo2 Planet::RecordStageOne(PlanetChunkProgress& chunk_progress) {
        VkResult res;

        // ===== Get and Begin Command Buffer ===== //

        VkCommandBuffer command_buffer = command_buffers[chunk_progress.command_buffer_index];

        VkCommandBufferBeginInfo command_buffer_begin_info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        res = vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);
        if (res != VK_SUCCESS) exit(res);

        // ===== Record Commands ===== //

        u32 dispatch_count_x = (chunk_progress.info.chunk_cells + (thread_group_x - 1)) / thread_group_x;
        u32 dispatch_count_y = (chunk_progress.info.chunk_cells + (thread_group_y - 1)) / thread_group_y;

        // == Pass One == //

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[0]);
        vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PlanetCreationInfo), &chunk_progress.info);
        vkCmdDispatch(command_buffer, dispatch_count_x, dispatch_count_y, thread_group_z);

        // == Make sure edge buffer is finished being written too, before trying to read == //

        VkBufferMemoryBarrier2 buffer_barrier{
                .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                .pNext               = nullptr,
                .srcStageMask        = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask       = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask        = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask       = VK_ACCESS_2_SHADER_READ_BIT,
                .srcQueueFamilyIndex = compute_queue_family,
                .dstQueueFamilyIndex = compute_queue_family,
                .buffer              = chunk_progress.edge_buffer.buffer,
                .offset              = 0,
                .size                = VK_WHOLE_SIZE,
        };

        VkDependencyInfo dependency_info{
                .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .pNext                    = nullptr,
                .dependencyFlags          = 0,
                .memoryBarrierCount       = 0,
                .pMemoryBarriers          = nullptr,
                .bufferMemoryBarrierCount = 1,
                .pBufferMemoryBarriers    = &buffer_barrier,
                .imageMemoryBarrierCount  = 0,
                .pImageMemoryBarriers     = nullptr,
        };

        vkCmdPipelineBarrier2KHR(command_buffer, &dependency_info);

        // == Pass Two == //

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[1]);
        vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PlanetCreationInfo), &chunk_progress.info);
        vkCmdDispatch(command_buffer, dispatch_count_x, dispatch_count_y, thread_group_z);

        // ===== Submit ===== //

        VkSemaphoreSubmitInfo wait_semaphore_info{};
        wait_semaphore_info.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        wait_semaphore_info.pNext     = nullptr;
        wait_semaphore_info.semaphore = chunk_progress.semaphore;
        wait_semaphore_info.value     = (u64)PlanetGenerationStage::StageOne;
        wait_semaphore_info.stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

        VkSemaphoreSubmitInfo signal_semaphore_info{};
        signal_semaphore_info.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signal_semaphore_info.pNext     = nullptr;
        signal_semaphore_info.semaphore = chunk_progress.semaphore;
        signal_semaphore_info.value     = (u64)PlanetGenerationStage::StageTwo;
        signal_semaphore_info.stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

        VkCommandBufferSubmitInfo command_buffer_submit_info{};
        command_buffer_submit_info.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        command_buffer_submit_info.pNext         = nullptr;
        command_buffer_submit_info.commandBuffer = command_buffer;

        VkSubmitInfo2 submit_info{};
        submit_info.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pNext                    = nullptr;
        submit_info.waitSemaphoreInfoCount   = 1;
        submit_info.pWaitSemaphoreInfos      = &wait_semaphore_info;
        submit_info.signalSemaphoreInfoCount = 1;
        submit_info.pSignalSemaphoreInfos    = &signal_semaphore_info;
        submit_info.commandBufferInfoCount   = 1;
        submit_info.pCommandBufferInfos      = &command_buffer_submit_info;

        // vkQueueSubmit2(compute_queue, 1, &submit_info, VK_NULL_HANDLE);
        return submit_info;
}

// CPU Sum
// NOTE: Have chunk generation have its own thread, then in update we can just loop all
// and call this function but with early exit if semaphore not at expected.
void Planet::StartStageTwo(PlanetChunkProgress& chunk_progress) {
        // Should already be here if this is called
        // if (chunk_progress.GetStage(vulkan_device) != PlanetGenerationStage::StageTwo) return;

        // ===== Start Calc ===== //

        EdgeSumInfo* mapped_data = (EdgeSumInfo*)chunk_progress.edge_sums_buffer.allocation_info.pMappedData;

        // Add all y edges
        u32 edge_count = chunk_progress.info.chunk_cells + 1;
        for (u32 z = 0; z < edge_count; z++) {
                for (u32 y = 1; y < edge_count; y++) {
                        u32 index      = (z * edge_count) + y;
                        u32 prev_index = index - 1;

                        mapped_data[index].vertex_offset += mapped_data[prev_index].vertex_offset;
                        mapped_data[index].triangle_offset += mapped_data[prev_index].triangle_offset;
                }
        }

        // Add last z edge
        for (u32 z = 1; z < edge_count; z++) {
                u32 index      = (z * edge_count) + (edge_count - 1);
                u32 prev_index = index - edge_count;

                mapped_data[index].vertex_offset += mapped_data[prev_index].vertex_offset;
                mapped_data[index].triangle_offset += mapped_data[prev_index].triangle_offset;
        }

        u32 last_index = (edge_count - 1) * (edge_count + 1);

        u32 vertex_count = mapped_data[last_index].vertex_offset + mapped_data[last_index].vertex_count;
        u32 index_count  = mapped_data[last_index].triangle_offset + mapped_data[last_index].triangle_count;

        // ===== Allocate Buffers ===== //

        VmaAllocationCreateFlags allocation_flags = 0;

        // == Vertex Buffer - Dont need to access on CPU == //

        chunk_progress.chunk.vertex_buffer = CreateGPUBuffer(vulkan_device, vulkan_allocator, sizeof(u32) * index_count, compute_queue_family,
                                                             VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, allocation_flags);

        VkBufferDeviceAddressInfo vertex_buffer_address_info{
                .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .buffer = chunk_progress.chunk.vertex_buffer.buffer,
        };

        chunk_progress.chunk.vertex_buffer.device_address = vkGetBufferDeviceAddress(vulkan_device, &vertex_buffer_address_info);

        // == Index Buffer == //

        chunk_progress.chunk.index_buffer = CreateGPUBuffer(vulkan_device, vulkan_allocator, sizeof(u32) * index_count, compute_queue_family,
                                                            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, allocation_flags);

        VkBufferDeviceAddressInfo index_buffer_address_info{
                .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .buffer = chunk_progress.chunk.index_buffer.buffer,
        };

        chunk_progress.chunk.index_buffer.device_address = vkGetBufferDeviceAddress(vulkan_device, &index_buffer_address_info);

        // == Set Info Addresses == //

        chunk_progress.info.indices  = chunk_progress.chunk.index_buffer.device_address;
        chunk_progress.info.vertices = chunk_progress.chunk.vertex_buffer.device_address;

        // ===== Signal Semaphore ===== //

        VkSemaphoreSignalInfo signal_info;
        signal_info.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
        signal_info.pNext     = NULL;
        signal_info.semaphore = chunk_progress.semaphore;
        signal_info.value     = (u64)PlanetGenerationStage::StageThree;

        vkSignalSemaphore(vulkan_device, &signal_info);
}

VkSubmitInfo2 Planet::RecordStageThree(PlanetChunkProgress& chunk_progress) {
        VkResult res;

        // ===== Get and Begin Command Buffer ===== //

        VkCommandBuffer command_buffer = command_buffers[chunk_progress.command_buffer_index + 1];

        VkCommandBufferBeginInfo command_buffer_begin_info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        res = vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);
        if (res != VK_SUCCESS) exit(res);

        // ===== Record Commands ===== //

        u32 dispatch_count_x = (chunk_progress.info.chunk_cells + (thread_group_x - 1)) / thread_group_x;
        u32 dispatch_count_y = (chunk_progress.info.chunk_cells + (thread_group_y - 1)) / thread_group_y;

        // == Pass Three == //

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[2]);
        vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PlanetCreationInfo), &chunk_progress.info);
        vkCmdDispatch(command_buffer, dispatch_count_x, dispatch_count_y, thread_group_z);

        // == TODO: Normals == //

        // VkBufferMemoryBarrier2 buffer_barrier{
        //         .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        //         .pNext               = nullptr,
        //         .srcStageMask        = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        //         .srcAccessMask       = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        //         .dstStageMask        = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        //         .dstAccessMask       = VK_ACCESS_2_SHADER_READ_BIT,
        //         .srcQueueFamilyIndex = compute_queue_family,
        //         .dstQueueFamilyIndex = compute_queue_family,
        //         .buffer              = chunk_progress.edge_buffer.buffer,
        //         .offset              = 0,
        //         .size                = VK_WHOLE_SIZE,
        // };

        // VkDependencyInfo dependency_info{
        //         .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        //         .pNext                    = nullptr,
        //         .dependencyFlags          = 0,
        //         .memoryBarrierCount       = 0,
        //         .pMemoryBarriers          = nullptr,
        //         .bufferMemoryBarrierCount = 1,
        //         .pBufferMemoryBarriers    = &buffer_barrier,
        //         .imageMemoryBarrierCount  = 0,
        //         .pImageMemoryBarriers     = nullptr,
        // };

        // vkCmdPipelineBarrier2KHR(command_buffer, &dependency_info);

        // // == Pass Four == //

        // vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[3]);
        // vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PlanetCreationInfo), &chunk_progress.info);
        // vkCmdDispatch(command_buffer, dispatch_count_x, dispatch_count_y, thread_group_z);

        // ===== Submit ===== //

        VkSemaphoreSubmitInfo wait_semaphore_info{};
        wait_semaphore_info.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        wait_semaphore_info.pNext     = nullptr;
        wait_semaphore_info.semaphore = chunk_progress.semaphore;
        wait_semaphore_info.value     = (u64)PlanetGenerationStage::StageThree;
        wait_semaphore_info.stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

        VkSemaphoreSubmitInfo signal_semaphore_info{};
        signal_semaphore_info.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signal_semaphore_info.pNext     = nullptr;
        signal_semaphore_info.semaphore = chunk_progress.semaphore;
        signal_semaphore_info.value     = (u64)PlanetGenerationStage::Finished;
        signal_semaphore_info.stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

        VkCommandBufferSubmitInfo command_buffer_submit_info{};
        command_buffer_submit_info.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        command_buffer_submit_info.pNext         = nullptr;
        command_buffer_submit_info.commandBuffer = command_buffer;

        VkSubmitInfo2 submit_info{};
        submit_info.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pNext                    = nullptr;
        submit_info.waitSemaphoreInfoCount   = 1;
        submit_info.pWaitSemaphoreInfos      = &wait_semaphore_info;
        submit_info.signalSemaphoreInfoCount = 1;
        submit_info.pSignalSemaphoreInfos    = &signal_semaphore_info;
        submit_info.commandBufferInfoCount   = 1;
        submit_info.pCommandBufferInfos      = &command_buffer_submit_info;

        // vkQueueSubmit2(compute_queue, 1, &submit_info, VK_NULL_HANDLE);
        return submit_info;
}