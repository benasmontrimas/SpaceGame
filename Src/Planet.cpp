#include "Base.h"
#include "Planet.h"
#include "Game.h"

void PlanetChunkTree::Init(Planet* _planet) {
        assert(_planet and "Planet must be a valid pointer.");
        planet = _planet;

        root.center      = planet->position;
        float half_width = TREE_ROOT_DIAMETER / 2.0f;
        root.radius      = { half_width, half_width, half_width };

        u32 root_children_index = AssignChildren(PARENT_ROOT);
        assert(root_children_index == 0 and "Root nodes children should always start at index 0.");
}

void PlanetChunkTree::Update() {
        AABB child_bounds{};
        child_bounds.radius = root.radius / 2.0f;

        // Traverse Root children, root can not have chunk.
        for (u32 i = 0; i < NODE_CHILD_COUNT; i++) {
                child_bounds.center = root.center + (PARENT_CENTER_OFFSET[i] * child_bounds.radius);
                Traverse(i, child_bounds, TREE_MAX_LOD - 1);
        }

        std::println("Nodes: {}", nodes.size());
        std::println("Chunks: {}", planet->chunks.size());
}

void PlanetChunkTree::Shutdown() {
}

void PlanetChunkTree::Traverse(u32 node_index, AABB bounds, u32 lod_level) {
        if (nodes[node_index].IsEmpty()) return;

        // ===== Traverse ===== //

        u32   max_lod_distance_index = std::min(lod_level, CHUNK_LOD_DISTANCE_COUNT - 1);
        float max_lod_distance       = CHUNK_LOD_DISTANCES[max_lod_distance_index];
        max_lod_distance             = max_lod_distance * max_lod_distance;

        // ===== If in range spawn chunk and return ===== //

        float distance_sq = bounds.DistanceSq(planet->target->position);
        if (distance_sq > max_lod_distance or lod_level == 0) {
                std::println("Should Gen");
                // ===== Check Existing ===== //
                if (nodes[node_index].HasChunk()) {
                        std::println("WHY");
                        u32 chunk_index = nodes[node_index].chunk_index;
                        if (planet->chunks[chunk_index].lod_level == lod_level) {
                                if (planet->chunks[chunk_index].state == PlanetGenerationState::Empty) {
                                        nodes[node_index].SetEmpty(true);
                                        planet->DestroyChunk(chunk_index);
                                        return;
                                }

                                if (planet->chunks[chunk_index].state == PlanetGenerationState::Initialised) {
                                        planet->chunks_to_render.push_back(chunk_index);
                                }

                                return;
                        }

                        // ===== Destroy any existing chunks ===== //

                        ResetNode(node_index);
                }

                // ===== Spawn New Chunk ===== //

                nodes[node_index].chunk_index = planet->GenerateChunk(bounds, lod_level);

                return;
        }

        // ===== If out of range for LOD level, Subdivide and Update Children ===== //

        u32 base_child_index = nodes[node_index].first_child_index;

        if (!nodes[node_index].HasChildren()) {
                base_child_index = AssignChildren(node_index);
        }

        // // ===== Loop and Update Children ===== //

        bool children_are_empty = true;

        AABB child_bounds{};
        child_bounds.radius = bounds.radius / 2.0f;

        for (u32 i = 0; i < NODE_CHILD_COUNT; i++) {
                u32 child_index = base_child_index + i;

                child_bounds.center = bounds.center + (PARENT_CENTER_OFFSET[i] * child_bounds.radius);
                // Traverse(child_index, child_bounds, lod_level - 1);

                children_are_empty &= nodes[child_index].IsEmpty();
        }

        // ===== If All Children Empty Combine ===== //

        if (children_are_empty) {
                ResetNode(node_index);
                nodes[node_index].SetEmpty(true);
        }
}

u32 PlanetChunkTree::AssignChildren(u32 parent_index) {
        // ===== Use Existing Slots if Available ===== //

        if (next_free_index == 0) {
                u32 index = (u32)nodes.size();
                nodes.resize(index + NODE_CHILD_COUNT);

                for (u32 i = 0; i < NODE_CHILD_COUNT; i++) {
                        nodes[index + i] = {
                                .first_child_index = NO_CHILDREN,
                                .chunk_index       = NO_CHUNK,
                        };
                }

                assert(parent_indices.size() == index / NODE_CHILD_COUNT);
                parent_indices.emplace_back(parent_index);
                if (parent_index != PARENT_ROOT) nodes[parent_index].first_child_index = index;

                return index;
        }

        u32 index       = next_free_index;
        next_free_index = parent_indices[next_free_index / NODE_CHILD_COUNT];

        // ===== Reset nodes ===== //

        for (u32 i = 0; i < NODE_CHILD_COUNT; i++) {
                nodes[index + i].first_child_index = NO_CHILDREN;
                nodes[index + i].chunk_index       = NO_CHUNK;
        }

        parent_indices[index / NODE_CHILD_COUNT] = parent_index;
        if (parent_index != PARENT_ROOT) nodes[parent_index].first_child_index = index;

        return index;
}

void PlanetChunkTree::RemoveChildren(u32 parent_index) {
        // ===== Get Child Index ===== //

        if (!nodes[parent_index].HasChildren()) return;
        u32 child_index = nodes[parent_index].first_child_index;

        // ===== Reset Children ===== //

        for (u32 i = 0; i < NODE_CHILD_COUNT; i++) {
                nodes[child_index + i].first_child_index = NO_CHILDREN;
                nodes[child_index + i].chunk_index       = NO_CHUNK;
        }

        // ===== Store Free Index in parent index ===== //

        parent_indices[child_index / NODE_CHILD_COUNT] = next_free_index;
        next_free_index                                = child_index;
}

void PlanetChunkTree::ResetNode(u32 node_index) {
        if (nodes[node_index].HasChildren()) {
                for (u32 i = 0; i < NODE_CHILD_COUNT; i++) {
                        ResetNode(nodes[node_index].first_child_index + i);
                }

                RemoveChildren(nodes[node_index].first_child_index);
        }

        if (nodes[node_index].IsEmpty() or !nodes[node_index].HasChunk()) return;

        planet->DestroyChunk(node_index);
}

void Planet::Init(GameContext* _game_context, GameObject* _target) {
        game_context = _game_context;
        target       = _target;

        VkResult res{};

        // ===== Create Command Pool ===== //

        VkCommandPoolCreateInfo command_pool_info{
                .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = game_context->compute_queue_family,
        };

        res = vkCreateCommandPool(game_context->vulkan_device, &command_pool_info, nullptr, &command_pool);
        if (res != VK_SUCCESS) exit(res);

        // ===== Allocate Command Buffers ===== //

        VkCommandBufferAllocateInfo compute_command_buffer_allocate_info{
                .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool        = command_pool,
                .commandBufferCount = COMMAND_BUFFER_COUNT,
        };

        res = vkAllocateCommandBuffers(game_context->vulkan_device, &compute_command_buffer_allocate_info, &command_buffers[0]);
        if (res != VK_SUCCESS) exit(res);

        u32 free_slots = COMMAND_BUFFER_COUNT; // Each slot allows 2 command buffers
        free_command_buffers.reserve(free_slots);
        for (u32 i = 0; i < COMMAND_BUFFER_COUNT; i++) {
                free_command_buffers.push_back((COMMAND_BUFFER_COUNT - 1) - i);
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

        res = vkCreatePipelineLayout(game_context->vulkan_device, &pipeline_layout_info, nullptr, &pipeline_layout);
        if (res != VK_SUCCESS) exit(res);

        // ===== Load Shaders ===== //

        Slang::ComPtr<slang::IBlob>   diagnostics;
        Slang::ComPtr<slang::IModule> slang_module{
                game_context->slang_session->loadModuleFromSource("MarchingCubes", "Assets/Shaders/MarchingCubes.slang", nullptr, diagnostics.writeRef()),
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
        res = vkCreateShaderModule(game_context->vulkan_device, &shader_module_info, nullptr, &shader_module);

        if (res != VK_SUCCESS) {
                std::println("Failed creating compute shader module");
                exit(res);
        }

        // ===== Create Pipelines ===== //

        constexpr const char* entry_point_names[GENERATION_PASS_COUNT] = {
                "Pass1",
                "Pass2",
                "Pass3",
                "Pass4",
        };

        VkComputePipelineCreateInfo pipeline_infos[GENERATION_PASS_COUNT];

        for (int i = 0; i < GENERATION_PASS_COUNT; i++) {
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

        vkCreateComputePipelines(game_context->vulkan_device, VK_NULL_HANDLE, GENERATION_PASS_COUNT, &pipeline_infos[0], nullptr, &pipelines[0]);

        vkDestroyShaderModule(game_context->vulkan_device, shader_module, nullptr);

        // ===== Create Constant Buffers ===== //

        VmaAllocationCreateFlags allocation_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                                    VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        // == Edge Lookup == //

        edge_lookup_buffer =
                CreateGPUBuffer(game_context->vulkan_device, game_context->vulkan_allocator, sizeof(int) * 256, game_context->transfer_queue_family,
                                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, allocation_flags);

        // Dont need this?
        // VkBufferDeviceAddressInfo edge_lookup_buffer_address_info{
        //         .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        //         .buffer = edge_lookup_buffer.buffer,
        // };

        // edge_lookup_buffer.device_address = vkGetBufferDeviceAddress(game_context->vulkan_device, &edge_lookup_buffer_address_info);

        // == Write Data == //

        GPUBufferWriteInfo edge_write_job_info{
                .src    = (u8*)&edge_table[0],
                .offset = 0,
                .size   = sizeof(int) * 256,
                .dst    = edge_lookup_buffer,

                .dst_queue_family = game_context->compute_queue_family,
        };

        TransferJobID edge_write_id = game_context->transfer_engine.QueueGPUBufferWrite(edge_write_job_info);

        // == Triangle Lookup == //

        triangle_lookup_buffer =
                CreateGPUBuffer(game_context->vulkan_device, game_context->vulkan_allocator, sizeof(int) * 256 * 16, game_context->transfer_queue_family,
                                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, allocation_flags);

        // VkBufferDeviceAddressInfo triangle_lookup_buffer_address_info{
        //         .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        //         .buffer = triangle_lookup_buffer.buffer,
        // };

        // triangle_lookup_buffer.device_address = vkGetBufferDeviceAddress(game_context->vulkan_device, &triangle_lookup_buffer_address_info);

        // == Write Data == //

        GPUBufferWriteInfo triangle_write_job_info{
                .src    = (u8*)&triangle_table[0][0],
                .offset = 0,
                .size   = sizeof(int) * 256 * 16,
                .dst    = triangle_lookup_buffer,

                .dst_queue_family = game_context->compute_queue_family,
        };

        TransferJobID triangle_write_id = game_context->transfer_engine.QueueGPUBufferWrite(triangle_write_job_info);

        // Ensure these buffers are written before trying to generate chunks.
        // Or can I use a semaphore to just set a semaphore to get waited on and the Transfer Engine doesnt keep track after finishing

        while (!game_context->transfer_engine.Check(edge_write_id)) {
                game_context->transfer_engine.Update();
        }

        while (!game_context->transfer_engine.Check(triangle_write_id)) {
                game_context->transfer_engine.Update();
        }

        // Need to acquire the buffers on the compute queue.

        tree.Init(this);
}

void Planet::Shutdown() {
        tree.Shutdown();

        vkDestroyCommandPool(game_context->vulkan_device, command_pool, nullptr);

        vkDestroyPipelineLayout(game_context->vulkan_device, pipeline_layout, nullptr);
        for (u32 i = 0; i < GENERATION_PASS_COUNT; i++) {
                vkDestroyPipeline(game_context->vulkan_device, pipelines[i], nullptr);
        }

        DestroyGPUBuffer(game_context->vulkan_device, edge_lookup_buffer, game_context->vulkan_allocator);
        DestroyGPUBuffer(game_context->vulkan_device, triangle_lookup_buffer, game_context->vulkan_allocator);
}

void Planet::Update() {
        // ===== Check all in progress chunks if they are ready to go to next stage ===== //

        for (u64 i = 0; i < chunks_in_progress.size(); i++) {
                PlanetChunkProgress& chunk_progress = chunks_in_progress[i];

                PlanetGenerationState chunk_state = chunks[chunk_progress.chunk_index].state;

                switch (chunk_state) {

                        // ===== Generation States ===== //
                        case PlanetGenerationState::WaitingOnGeneration:
                                {
                                        // GPU: Nothing
                                        // CPU: Find Command Buffer and Memory for Generation

                                        // ===== Find a command buffer ===== //

                                        int command_buffer_index = GetCommandBufferIndex();
                                        if (command_buffer_index == -1) break;
                                        chunk_progress.command_buffer_index = command_buffer_index;

                                        // ===== Record Commands ===== //

                                        // VkSubmitInfo2 submit_info = RecordStageOne(chunk_progress);
                                        // vkQueueSubmit2(game_context->compute_queue, 1, &submit_info, VK_NULL_HANDLE);

                                        // ===== Signal Semaphore ===== //

                                        VkSemaphoreSignalInfo signal_info;
                                        signal_info.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
                                        signal_info.pNext     = NULL;
                                        signal_info.semaphore = chunk_progress.semaphore;
                                        signal_info.value     = (u64)PlanetGenerationState::ProcessingStageOne;

                                        vkSignalSemaphore(game_context->vulkan_device, &signal_info);

                                        // Set Stage

                                        chunks[chunk_progress.chunk_index].state = PlanetGenerationState::ProcessingStageOne;
                                }
                                break;
                        case PlanetGenerationState::ProcessingStageOne:
                                {
                                        // GPU: Count Triangles and Vertices
                                        // CPU: Nothing

                                        // Query Semaphore
                                }
                                break;
                        case PlanetGenerationState::ProcessingStageTwo:
                                {
                                        // GPU: Nothing
                                        // CPU: Sum triangle and vertex offsets

                                        StartStageTwo(chunk_progress);
                                }
                                break;
                        case PlanetGenerationState::ProcessingStageThree:
                                {
                                        // GPU: Write Output Data
                                        // CPU: Nothing
                                }
                                break;
                        case PlanetGenerationState::FinishingProcessing:
                                {
                                        // GPU: Nothing
                                        // CPU: Free resource and add chunk to Planet.

                                        FreeChunkInProgress(chunk_progress);
                                        chunks_in_progress[i] = chunks_in_progress.back();
                                        chunks_in_progress.pop_back();
                                        i--;

                                        // Add chunk to Planet
                                }
                                break;

                        // ===== Inactive States ===== //
                        case PlanetGenerationState::WaitingOnDestroy:
                                break;
                        case PlanetGenerationState::Initialised:
                                break;
                        case PlanetGenerationState::Empty:
                                break;

                        // ===== Default ===== //
                        default:
                                std::println("Unexpected Stage in Planet Generation");
                                exit(1);
                }
        }

        // ===== Clear Render List ===== //

        chunks_to_render.clear();

        // ===== Process Delete List ===== //

        destroy_count             = 0;
        u32 num_chunks_to_destroy = (u32)chunks_to_destroy.size();
        for (u32 i = 0; i < num_chunks_to_destroy; i++) {
                DestroyChunk(chunks_to_destroy[i]);
        }
        chunks_to_destroy.resize(destroy_count);

        // ===== Update Tree ===== //

        tree.Update();
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

u32 Planet::GenerateChunk(AABB bounds, u32 lod_level) {
        std::println("Generate");

        VkResult res{};

        VkFence           fence;
        VkFenceCreateInfo fence_info{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = 0 };
        res = vkCreateFence(game_context->vulkan_device, &fence_info, nullptr, &fence);
        if (res != VK_SUCCESS) exit(res);

        VkSemaphore semaphore;

        VkSemaphoreTypeCreateInfo timeline_info{};
        timeline_info.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        timeline_info.pNext         = nullptr;
        timeline_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timeline_info.initialValue  = (u64)PlanetGenerationState::WaitingOnGeneration;

        VkSemaphoreCreateInfo semaphore_info{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                .pNext = &timeline_info,
        };

        res = vkCreateSemaphore(game_context->vulkan_device, &semaphore_info, nullptr, &semaphore);
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

        u32 x_edges = CHUNK_CELLS;
        u32 y_edges = CHUNK_CELLS + 1;
        u32 z_edges = CHUNK_CELLS + 1;

        u32 edge_count = x_edges * y_edges * z_edges;
        u32 cell_count = CHUNK_CELLS * CHUNK_CELLS * CHUNK_CELLS;
        u32 row_count  = y_edges * z_edges;

        // == Edges Buffer - GPU read/write only == //

        VmaAllocationCreateFlags allocation_flags = 0;

        GPUBuffer edge_buffer = CreateGPUBuffer(game_context->vulkan_device, game_context->vulkan_allocator, sizeof(float) * edge_count,
                                                game_context->compute_queue_family, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, allocation_flags);

        VkBufferDeviceAddressInfo edge_buffer_address_info{
                .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .buffer = edge_buffer.buffer,
        };

        edge_buffer.device_address = vkGetBufferDeviceAddress(game_context->vulkan_device, &edge_buffer_address_info);

        // == Edge Sums Buffer - Host read == //

        allocation_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        GPUBuffer edge_sums_buffer = CreateGPUBuffer(game_context->vulkan_device, game_context->vulkan_allocator, sizeof(EdgeSumInfo) * row_count,
                                                     game_context->compute_queue_family,
                                                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, allocation_flags);

        VkBufferDeviceAddressInfo edge_sums_buffer_address_info{
                .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .buffer = edge_sums_buffer.buffer,
        };

        edge_sums_buffer.device_address = vkGetBufferDeviceAddress(game_context->vulkan_device, &edge_sums_buffer_address_info);

        // ===== Initialise Creation Info ===== //

        PlanetCreationInfo info{
                .seed        = 0,
                .position    = position,
                .chunk_cells = CHUNK_CELLS,
                .chunk_dims  = (u32)bounds.radius.x * 2,

                .edge_case_lookup = edge_lookup_buffer.device_address,
                .triangle_lookup  = triangle_lookup_buffer.device_address,

                .edges            = edge_buffer.device_address,
                .edge_sums        = edge_sums_buffer.device_address,
                .triangle_normals = 0, // TODO

                .indices  = 0, // Set later
                .vertices = 0, // Set later
        };

        // ===== Find a chunk index ===== //

        u32 chunk_index = 0;

        if (next_free_chunk == 0) {
                chunk_index = (u32)chunks.size();
                chunks.push_back({});
        } else {
                chunk_index     = next_free_chunk;
                next_free_chunk = chunks[next_free_chunk].next_free_index;
        }

        chunks[chunk_index].state    = PlanetGenerationState::WaitingOnGeneration;
        chunks[chunk_index].position = bounds.center - bounds.radius;
        chunks[chunk_index].lod_level = lod_level;

        PlanetChunkProgress& chunk_in_progress = chunks_in_progress.emplace_back(chunk_index, info, edge_buffer, edge_sums_buffer, semaphore, fence);

        return chunk_index;
}

void Planet::DestroyChunk(u32 index) {
        // ===== Check if we can Destroy Now ===== //

        PlanetGenerationState chunk_state = chunks[index].state;

        bool can_destroy = false;

        // ===== Check If Resources In Use ===== //

        if (chunk_state == PlanetGenerationState::Initialised or chunk_state == PlanetGenerationState::WaitingOnDestroy) {

                // ===== If Not In Use Can Destroy ===== //

                can_destroy |= (chunks[index].model.last_frame_rendered <= game_context->GetLastFinishedFrame());

                if (!can_destroy) {
                        chunks[index].state = PlanetGenerationState::WaitingOnDestroy;
                        return;
                }
        }

        // ===== Check If In Destroyable State ===== //

        can_destroy |= (chunk_state == PlanetGenerationState::None);
        can_destroy |= (chunk_state == PlanetGenerationState::Empty);

        // ===== If Cant Destroy Add To List ===== //

        if (!can_destroy) {
                if (destroy_count < chunks_to_destroy.size()) {
                        chunks_to_destroy[destroy_count] = index;
                } else {
                        chunks_to_destroy.push_back(index);
                }

                destroy_count++;
                return;
        }

        // ===== Destroy Chunk ===== //

        chunks[index].model.Destroy(*game_context);
        if (chunks[index].index_offsets) free(chunks[index].index_offsets);

        chunks[index] = {}; // Clear it

        // ===== Set Free Index ===== //

        chunks[index].next_free_index = next_free_chunk;
        next_free_chunk               = index;
}

void Planet::FreeChunkInProgress(PlanetChunkProgress& chunk_progress) {
        ReleaseCommandBufferIndex(chunk_progress.command_buffer_index);

        vkDestroySemaphore(game_context->vulkan_device, chunk_progress.semaphore, nullptr);
        vkDestroyFence(game_context->vulkan_device, chunk_progress.fence, nullptr);

        // Free intermediate buffers?
}

// NOTE: I think this could easily have pre-recorded command buffers.

// NOTE: If i want to use pre recorded buffers I will need to move away from push constants and write to uniform buffer instead
// as push constants are written to the command buffer. But can use for Constant Data.

// Queue Pass 1 and Pass 2
VkSubmitInfo2 Planet::RecordStageOne(PlanetChunkProgress& chunk_progress) {
        VkResult res{};

        // ===== Get and Begin Command Buffer ===== //

        VkCommandBuffer command_buffer = command_buffers[chunk_progress.command_buffer_index];

        VkCommandBufferBeginInfo command_buffer_begin_info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        res = vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);
        if (res != VK_SUCCESS) exit(res);

        // ===== Record Commands ===== //

        u32 dispatch_count_x = (chunk_progress.info.chunk_cells + (THREAD_GROUP_X - 1)) / THREAD_GROUP_X;
        u32 dispatch_count_y = (chunk_progress.info.chunk_cells + (THREAD_GROUP_Y - 1)) / THREAD_GROUP_Y;

        // == Pass One == //

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[0]);
        vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PlanetCreationInfo), &chunk_progress.info);
        vkCmdDispatch(command_buffer, dispatch_count_x, dispatch_count_y, THREAD_GROUP_Z);

        // == Make sure edge buffer is finished being written too, before trying to read == //

        VkBufferMemoryBarrier2 buffer_barrier{
                .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                .pNext               = nullptr,
                .srcStageMask        = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask       = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask        = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask       = VK_ACCESS_2_SHADER_READ_BIT,
                .srcQueueFamilyIndex = game_context->compute_queue_family,
                .dstQueueFamilyIndex = game_context->compute_queue_family,
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
        vkCmdDispatch(command_buffer, dispatch_count_x, dispatch_count_y, THREAD_GROUP_Z);

        // ===== Submit ===== //

        VkSemaphoreSubmitInfo wait_semaphore_info{};
        wait_semaphore_info.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        wait_semaphore_info.pNext     = nullptr;
        wait_semaphore_info.semaphore = chunk_progress.semaphore;
        wait_semaphore_info.value     = (u64)PlanetGenerationState::ProcessingStageOne;
        wait_semaphore_info.stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

        VkSemaphoreSubmitInfo signal_semaphore_info{};
        signal_semaphore_info.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signal_semaphore_info.pNext     = nullptr;
        signal_semaphore_info.semaphore = chunk_progress.semaphore;
        signal_semaphore_info.value     = (u64)PlanetGenerationState::ProcessingStageTwo;
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

        // vkQueueSubmit2(game_context->compute_queue, 1, &submit_info, VK_NULL_HANDLE);
        return submit_info;
}

// CPU Sum
// NOTE: Have chunk generation have its own thread, then in update we can just loop all
// and call this function but with early exit if semaphore not at expected.
void Planet::StartStageTwo(PlanetChunkProgress& chunk_progress) {
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

        u32 index_count  = mapped_data[last_index].triangle_offset + mapped_data[last_index].triangle_count;
        u32 vertex_count = mapped_data[last_index].vertex_offset + mapped_data[last_index].vertex_count;

        if (index_count == 0) {
                // TODO: Need to set as empty!
                return;
        }

        // ===== Allocate Buffers ===== //

        VmaAllocationCreateFlags allocation_flags = 0;

        u64 vertex_buffer_size = sizeof(Vertex) * vertex_count;
        u64 index_buffer_size  = sizeof(u32) * index_count;

        u64       buffer_size = vertex_buffer_size + index_buffer_size;
        GPUBuffer buffer      = CreateGPUBuffer(game_context->vulkan_device, game_context->vulkan_allocator, buffer_size, game_context->compute_queue_family,
                                                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, allocation_flags);

        chunks[chunk_progress.chunk_index].model.meshes.emplace_back(buffer, vertex_buffer_size, index_count);

        // // == Vertex Buffer - Dont need to access on CPU == //

        // chunks[chunk_progress.chunk_index].model.vertex_buffer =
        //         CreateGPUBuffer(game_context->vulkan_device, game_context->vulkan_allocator, sizeof(Vertex) * vertex_count,
        //         game_context->compute_queue_family,
        //                         VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, allocation_flags);

        // model.meshes[0].vertices_size = sizeof(Vertex) * vertex_count;

        // VkBufferDeviceAddressInfo vertex_buffer_address_info{
        //         .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        //         .buffer = chunks[chunk_progress.chunk_index].model.vertex_buffer.buffer,
        // };

        // chunks[chunk_progress.chunk_index].model.vertex_buffer.device_address =
        //         vkGetBufferDeviceAddress(game_context->vulkan_device, &vertex_buffer_address_info);

        // // == Index Buffer == //

        // chunks[chunk_progress.chunk_index].model.index_buffer =
        //         CreateGPUBuffer(game_context->vulkan_device, game_context->vulkan_allocator, sizeof(u32) * index_count, game_context->compute_queue_family,
        //                         VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, allocation_flags);

        // VkBufferDeviceAddressInfo index_buffer_address_info{
        //         .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        //         .buffer = chunks[chunk_progress.chunk_index].model.index_buffer.buffer,
        // };

        // chunks[chunk_progress.chunk_index].model.index_buffer.device_address =
        //         vkGetBufferDeviceAddress(game_context->vulkan_device, &index_buffer_address_info);

        // == Set Info Addresses == //

        chunk_progress.info.vertices = buffer.device_address;
        chunk_progress.info.indices  = buffer.device_address + vertex_buffer_size;

        // ===== Signal Semaphore ===== //

        VkSemaphoreSignalInfo signal_info;
        signal_info.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
        signal_info.pNext     = NULL;
        signal_info.semaphore = chunk_progress.semaphore;
        signal_info.value     = (u64)PlanetGenerationState::ProcessingStageThree;

        vkSignalSemaphore(game_context->vulkan_device, &signal_info);

        // Start Next Stage
        VkSubmitInfo2 submit_info = RecordStageThree(chunk_progress);
        vkQueueSubmit2(game_context->compute_queue, 1, &submit_info, VK_NULL_HANDLE);
}

VkSubmitInfo2 Planet::RecordStageThree(PlanetChunkProgress& chunk_progress) {
        VkResult res{};

        // ===== Get and Begin Command Buffer ===== //

        VkCommandBuffer command_buffer = command_buffers[chunk_progress.command_buffer_index];

        VkCommandBufferBeginInfo command_buffer_begin_info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        res = vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);
        if (res != VK_SUCCESS) exit(res);

        // ===== Record Commands ===== //

        u32 dispatch_count_x = (chunk_progress.info.chunk_cells + (THREAD_GROUP_X - 1)) / THREAD_GROUP_X;
        u32 dispatch_count_y = (chunk_progress.info.chunk_cells + (THREAD_GROUP_Y - 1)) / THREAD_GROUP_Y;

        // == Pass Three == //

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[2]);
        vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PlanetCreationInfo), &chunk_progress.info);
        vkCmdDispatch(command_buffer, dispatch_count_x, dispatch_count_y, THREAD_GROUP_Z);

        // == TODO: Normals == //

        // VkBufferMemoryBarrier2 buffer_barrier{
        //         .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        //         .pNext               = nullptr,
        //         .srcStageMask        = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        //         .srcAccessMask       = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        //         .dstStageMask        = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        //         .dstAccessMask       = VK_ACCESS_2_SHADER_READ_BIT,
        //         .srcQueueFamilyIndex = game_context->compute_queue_family,
        //         .dstQueueFamilyIndex = game_context->compute_queue_family,
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
        // vkCmdDispatch(command_buffer, dispatch_count_x, dispatch_count_y, THREAD_GROUP_Z);

        // ===== Submit ===== //

        VkSemaphoreSubmitInfo wait_semaphore_info{};
        wait_semaphore_info.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        wait_semaphore_info.pNext     = nullptr;
        wait_semaphore_info.semaphore = chunk_progress.semaphore;
        wait_semaphore_info.value     = (u64)PlanetGenerationState::ProcessingStageThree;
        wait_semaphore_info.stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

        VkSemaphoreSubmitInfo signal_semaphore_info{};
        signal_semaphore_info.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signal_semaphore_info.pNext     = nullptr;
        signal_semaphore_info.semaphore = chunk_progress.semaphore;
        signal_semaphore_info.value     = (u64)PlanetGenerationState::FinishingProcessing;
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

        // vkQueueSubmit2(game_context->compute_queue, 1, &submit_info, VK_NULL_HANDLE);
        return submit_info;
}