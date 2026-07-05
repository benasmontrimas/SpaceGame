#include <print>

#include "Base.h"
#include "Game.h"
#include "Planet.h"

void PlanetChunkTree::Init(Planet* _planet) {
        assert(_planet and "Planet must be a valid pointer.");
        planet = _planet;

        root.center      = planet->position;
        float half_width = TREE_ROOT_DIAMETER / 2.0f;
        root.radius      = { half_width, half_width, half_width };

        next_free_index = NO_FREE;

        NodeID root_children_index = AddChildren(PARENT_ROOT);
        assert(root_children_index == 0 and "Root nodes children should always start at index 0.");
}

void PlanetChunkTree::Update() {
        AABB child_bounds{};
        child_bounds.radius = root.radius / 2.0f;

        // Traverse Root children, root can not have chunk.
        for (u32 child_index = 0; child_index < 8; child_index++) {
                child_bounds.center = root.center + (PARENT_CENTER_OFFSET[child_index] * child_bounds.radius);
                Traverse(child_index, child_bounds, TREE_MAX_LOD - 1, false);
        }
}

void PlanetChunkTree::Shutdown() {
}

void PlanetChunkTree::CreateNodesChunk(NodeID node_index, AABB bounds, u32 lod) {
        assert(!nodes[node_index].HasChunk() and "Node should not have chunk before chunk creation");

        nodes[node_index].chunk_index = planet->GenerateChunk(bounds, lod);

        assert(nodes[node_index].HasChunk() and "Node should have chunk after chunk creation");
}

void PlanetChunkTree::DestroyNodesChunk(NodeID node_index) {
        u32  chunk_index = nodes[node_index].chunk_index;
        bool is_empty    = planet->chunks[chunk_index].is_empty;

        planet->DestroyChunk(nodes[node_index].chunk_index);
        nodes[node_index].chunk_index = NO_CHUNK;
        if (is_empty) nodes[node_index].chunk_index = EMPTY_CHUNK;
}

bool PlanetChunkTree::AreAllChildrenInitialised(NodeID node_index) {
        NodeID first_child_index = nodes[node_index].first_child_index;

        bool all_children_initialised = true;

        for (u32 child_offset = 0; child_offset < NODE_CHILD_COUNT; child_offset++) {
                u32 child_index = first_child_index + child_offset;

                if (nodes[child_index].HasChildren()) {
                        all_children_initialised = all_children_initialised and AreAllChildrenInitialised(child_index);
                } else {
                        bool chunk_is_initialised = false;

                        if (nodes[child_index].HasChunk()) {
                                ChunkState chunk_state = planet->chunks[nodes[child_index].chunk_index].state;
                                chunk_is_initialised   = chunk_state == ChunkState::Initialised;
                        }

                        all_children_initialised = all_children_initialised and (nodes[child_index].IsEmpty() or chunk_is_initialised);
                }
        }

        return all_children_initialised;
}

void PlanetChunkTree::TraverseChildren(NodeID node_index, AABB bounds, u32 lod, bool waited_on) {
        AABB child_bounds{};
        child_bounds.radius = bounds.radius / 2.0f;

        NodeID first_child_index = nodes[node_index].first_child_index;

        for (NodeID child_offset = 0; child_offset < NODE_CHILD_COUNT; child_offset++) {
                u32 child_index = first_child_index + child_offset;

                child_bounds.center = bounds.center + (PARENT_CENTER_OFFSET[child_offset] * child_bounds.radius);

                Traverse(child_index, child_bounds, lod - 1, waited_on);
        }
}

// Split the current node.
void PlanetChunkTree::Split(NodeID node_index, AABB bounds, u32 lod, bool waited_on) {

        // ===== Create Children ===== //

        if (!nodes[node_index].HasChildren()) {
                AddChildren(node_index);
        }

        // ===== Check Chunk ===== //

        if (nodes[node_index].HasChunk()) {
                if (planet->chunks[nodes[node_index].chunk_index].IsRenderReady()) {
                        planet->chunks_to_render.push_back(nodes[node_index].chunk_index);
                }

                if (AreAllChildrenInitialised(node_index) or planet->chunks[nodes[node_index].chunk_index].is_empty) {
                        DestroyNodesChunk(node_index);
                }
        }

        // ===== Traverse Children ===== //

        NodeID first_child_index = nodes[node_index].first_child_index;

        AABB child_bounds{};
        child_bounds.radius = bounds.radius / 2.0f;

        bool node_is_empty = true;

        for (NodeID child_offset = 0; child_offset < NODE_CHILD_COUNT; child_offset++) {
                u32 child_index = first_child_index + child_offset;

                child_bounds.center = bounds.center + (PARENT_CENTER_OFFSET[child_offset] * child_bounds.radius);

                Traverse(child_index, child_bounds, lod - 1, waited_on or nodes[node_index].HasChunk());

                bool child_is_empty = nodes[child_index].IsEmpty();
                node_is_empty       = node_is_empty && child_is_empty;
        }

        // ===== Check if all children are empty ===== //

        if (node_is_empty) {
                RemoveChildren(node_index);
                nodes[node_index].chunk_index = EMPTY_CHUNK;
                assert(nodes[node_index].IsEmpty() and "Node should be empty");
        }
}

// Check if we need to generate chunk.
void PlanetChunkTree::HandleLeaf(NodeID node_index, AABB bounds, u32 lod, bool waited_on) {
        bool has_children = nodes[node_index].HasChildren();

        if (!nodes[node_index].HasChunk()) {
                CreateNodesChunk(node_index, bounds, lod);
        }

        if (has_children) {
                TraverseChildren(node_index, bounds, lod, waited_on);
        }

        ChunkID    chunk_index = nodes[node_index].chunk_index;
        ChunkState chunk_state = planet->chunks[chunk_index].state;

        if (planet->chunks[chunk_index].IsRenderReady()) {
                planet->chunks_to_render.push_back(chunk_index);

                if (has_children) {
                        RemoveChildren(node_index);
                }
        }
}

void PlanetChunkTree::Traverse(NodeID node_index, AABB bounds, u32 lod, bool waited_on) {
        // ===== Is Node Empty? ===== //

        if (nodes[node_index].IsEmpty()) {
                return;
        }

        // ===== Does Node Split? ===== //

        // == Check Range == //

        u32   range_index    = std::min(lod, CHUNK_LOD_DISTANCE_COUNT - 1);
        float split_range_sq = CHUNK_LOD_DISTANCES[range_index] * CHUNK_LOD_DISTANCES[range_index];

        float node_distance_sq = bounds.DistanceSq(planet->target->position);

        bool node_splits = node_distance_sq < split_range_sq;

        // == Check LOD == //

        node_splits = node_splits && lod != 0;

        // == Handle == //

        if (node_splits and !waited_on) {
                Split(node_index, bounds, lod, waited_on);
        } else {
                HandleLeaf(node_index, bounds, lod, waited_on);
        }
}

NodeID PlanetChunkTree::AddChildren(NodeID node_index) {
        // ===== Use Existing Slots if Available ===== //

        NodeID first_child_index = 0;

        if (next_free_index == NO_FREE) {
                // ===== Append New Nodes ===== //

                first_child_index = (NodeID)nodes.size();
                nodes.resize(first_child_index + NODE_CHILD_COUNT);

                assert(parent_indices.size() == first_child_index / NODE_CHILD_COUNT);
                parent_indices.emplace_back(node_index);
        } else {
                // ===== Reuse Nodes ===== //

                u32 temp_index = parent_indices[next_free_index];

                parent_indices[next_free_index] = node_index;
                first_child_index               = next_free_index * NODE_CHILD_COUNT;
                next_free_index                 = temp_index;
        }

        // == Initialize Children == //

        for (NodeID child_offset = 0; child_offset < NODE_CHILD_COUNT; child_offset++) {
                NodeID child_index = first_child_index + child_offset;

                nodes[child_index].first_child_index = NO_CHILDREN;
                nodes[child_index].chunk_index       = NO_CHUNK;
        }

        // == Update Node - Root doesnt have node == //

        if (node_index == PARENT_ROOT) return first_child_index;

        nodes[node_index].first_child_index = first_child_index;
        assert(nodes[node_index].HasChildren() and "Node should have children after adding them");
        return first_child_index;
}

void PlanetChunkTree::RemoveChildren(NodeID node_index) {
        assert(nodes[node_index].HasChildren() and "Trying to remove children from node without children");

        // ===== Get Child Index ===== //

        NodeID first_child_index = nodes[node_index].first_child_index;

        // ===== Reset Children ===== //

        for (NodeID child_offset = 0; child_offset < NODE_CHILD_COUNT; child_offset++) {
                NodeID child_index = first_child_index + child_offset;

                // ===== Reset childs children ===== //

                if (nodes[child_index].HasChildren()) RemoveChildren(child_index);

                // ===== Destroy Child Chunk if Exists ===== //

                if (nodes[child_index].HasChunk() and !nodes[child_index].IsEmpty()) DestroyNodesChunk(child_index);
        }

        nodes[node_index].first_child_index = NO_CHILDREN;

        // ===== Store Free Index in parent index ===== //

        parent_indices[first_child_index / NODE_CHILD_COUNT] = next_free_index;
        next_free_index                                      = first_child_index / NODE_CHILD_COUNT;

        assert(!nodes[node_index].HasChildren() and "Should not have children after we remove them");
}

void Planet::Init(GameContext* _game_context, Transform* _target) {
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

        u32 free_slots = COMMAND_BUFFER_COUNT;
        free_command_buffers.reserve(free_slots);
        for (u32 i = 0; i < COMMAND_BUFFER_COUNT; i++) {
                free_command_buffers.push_back(i);
        }

        // ===== Create Semaphores ===== //

        VkSemaphoreTypeCreateInfo timeline_info{};
        timeline_info.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        timeline_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timeline_info.initialValue  = (u64)PlanetProcessingStages::Invalid;

        VkSemaphoreCreateInfo semaphore_info{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                .pNext = &timeline_info,
        };

        for (u32 i = 0; i < COMMAND_BUFFER_COUNT; i++) {
                VkResult res = vkCreateSemaphore(game_context->vulkan_device, &semaphore_info, nullptr, &semaphores[i]);
        }

        // ===== Create Pipeline Layout ===== //

        VkPushConstantRange push_constant_range{
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .offset     = 0,
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

        VkComputePipelineCreateInfo pipeline_infos[GENERATION_PASS_COUNT];

        for (u32 i = 0; i < GENERATION_PASS_COUNT; i++) {
                VkPipelineShaderStageCreateInfo shader_stage_info{
                        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                        .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
                        .module = shader_module,
                        .pName  = ENTRY_POINT_NAMES[i],
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

        VmaAllocationCreateFlags allocation_flags = 0;

        // == Edge Lookup == //

        edge_lookup_buffer =
                CreateGPUBuffer(game_context->vulkan_device, game_context->vulkan_allocator, sizeof(int) * 256, game_context->transfer_queue_family,
                                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, allocation_flags);

        // == Write Data == //

        GPUBufferWriteInfo edge_write_job_info{
                .src    = (u8*)&edge_table[0],
                .offset = 0,
                .size   = sizeof(int) * 256,
                .dst    = &edge_lookup_buffer,

                .src_queue_family = game_context->transfer_queue_family,
                .dst_queue_family = game_context->compute_queue_family,
        };

        TransferJobID edge_write_id = game_context->transfer_engine.QueueGPUBufferWrite(edge_write_job_info);

        // == Triangle Lookup == //

        triangle_lookup_buffer =
                CreateGPUBuffer(game_context->vulkan_device, game_context->vulkan_allocator, sizeof(int) * 256 * 16, game_context->transfer_queue_family,
                                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, allocation_flags);

        // == Write Data == //

        GPUBufferWriteInfo triangle_write_job_info{
                .src    = (u8*)&triangle_table[0][0],
                .offset = 0,
                .size   = sizeof(int) * 256 * 16,
                .dst    = &triangle_lookup_buffer,

                .src_queue_family = game_context->transfer_queue_family,
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

        triangle_lookup_buffer.Print();

        tree.Init(this);

        next_free_chunk = NO_FREE;

        // Load Textures

        ground_texture.Load(game_context->vulkan_device, game_context->graphics_command_pool, game_context->graphics_queue, "Assets/Textures/Ground.ktx",
                            game_context->vulkan_allocator);
        game_context->AddTexture(ground_texture, 1);

        ground_normal_texture.Load(game_context->vulkan_device, game_context->graphics_command_pool, game_context->graphics_queue,
                                   "Assets/Textures/GroundNormal.ktx", game_context->vulkan_allocator);
        game_context->AddTexture(ground_normal_texture, 2);

        planet_material.type                             = MaterialType::Planet;
        planet_material.planet.ground_diffuse_texture_id = 1;
        planet_material.planet.ground_normal_texture_id  = 2;
}

void Planet::Shutdown() {
        tree.Shutdown();

        for (ChunkID i = 0; i < chunks.size(); i++) {
                chunks[i].state = ChunkState::None;
                DestroyChunk(i);
        }

        for (ChunkID i = 0; i < chunks_in_progress.size(); i++) {
                FreeChunkInProgress(i);
        }

        vkDestroyCommandPool(game_context->vulkan_device, command_pool, nullptr);

        vkDestroyPipelineLayout(game_context->vulkan_device, pipeline_layout, nullptr);
        for (u32 i = 0; i < GENERATION_PASS_COUNT; i++) {
                vkDestroyPipeline(game_context->vulkan_device, pipelines[i], nullptr);
        }

        ground_texture.Destroy(game_context);
        ground_normal_texture.Destroy(game_context);
        ground_disp_texture.Destroy(game_context);

        DestroyGPUBuffer(game_context->vulkan_device, edge_lookup_buffer, game_context->vulkan_allocator);
        DestroyGPUBuffer(game_context->vulkan_device, triangle_lookup_buffer, game_context->vulkan_allocator);
}

void Planet::ProcessChunkIfAvailable(PlanetChunkProgress& chunk_progress) {
        // ===== Find a command buffer ===== //

        u32 command_buffer_index = GetCommandBufferIndex();

        if (command_buffer_index == u32_max) {
                return;
        }

        chunk_progress.resource_index = command_buffer_index;

        // Set semaphore

        chunk_progress.semaphore = semaphores[command_buffer_index];

        u64 sem_value;
        vkGetSemaphoreCounterValue(game_context->vulkan_device, chunk_progress.semaphore, &sem_value);

        chunk_progress.semaphore_start_value = sem_value;


        // ===== Allocate Intermediate Buffers ===== //

        // == Edges Buffer == //

        VmaAllocationCreateFlags allocation_flags = 0;

        chunk_progress.edge_buffer =
                CreateGPUBuffer(game_context->vulkan_device, game_context->vulkan_allocator, EDGE_BUFFER_SIZE, game_context->compute_queue_family,
                                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, allocation_flags, VMA_MEMORY_USAGE_AUTO,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        // == Edge Sums Buffer == //

        allocation_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        chunk_progress.cell_sums_buffer =
                CreateGPUBuffer(game_context->vulkan_device, game_context->vulkan_allocator, CELL_SUM_BUFFER_SIZE, game_context->compute_queue_family,
                                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, allocation_flags, VMA_MEMORY_USAGE_AUTO, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        // == Set Info == //

        chunk_progress.info.edges     = chunk_progress.edge_buffer.device_address;
        chunk_progress.info.cell_sums = chunk_progress.cell_sums_buffer.device_address;

        // ===== Start Stage One ===== //

        RecordStageOne(chunk_progress);

        // // ===== Signal Semaphore ===== //

        VkSemaphoreSignalInfo signal_info{};
        signal_info.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
        signal_info.semaphore = chunk_progress.semaphore;
        signal_info.value     = chunk_progress.semaphore_start_value + (u64)PlanetProcessingStages::StageOne;

        vkSignalSemaphore(game_context->vulkan_device, &signal_info);

        // ===== Set State ===== //

        chunks[chunk_progress.chunk_index].state = ChunkState::Processing;
}

void Planet::UpdateStates() {
        i32 chunk_in_progress_count = chunks_in_progress.size();

        // ===== Loop chunks in progress in reverse ===== //

        for (i32 chunk_progress_index = chunk_in_progress_count - 1; chunk_progress_index > -1; chunk_progress_index--) {
                PlanetChunkProgress& chunk_progress = chunks_in_progress[chunk_progress_index];
                ChunkState           chunk_state    = chunks[chunk_progress.chunk_index].state;

                // Dont bother loading if we havnt already started and its going to be destroyed
                if (chunk_state == ChunkState::WaitingOnDestroy) {
                        FreeChunkInProgress(chunk_progress_index);
                        continue;
                }

                if (chunk_state == ChunkState::Waiting) {
                        ProcessChunkIfAvailable(chunk_progress);
                        continue;
                }

                // == Get Current Processing Stage == //
                PlanetProcessingStages chunk_stage{};

                VkSemaphore semaphore = chunk_progress.semaphore;

                u64 value;
                vkGetSemaphoreCounterValue(game_context->vulkan_device, semaphore, &value);
                chunk_stage = static_cast<PlanetProcessingStages>(value % (u64)PlanetProcessingStages::Count);
                assert(chunk_stage != PlanetProcessingStages::Invalid and "Chunk in progress is in an invalid stage");

                // NOTE: Semaphore used before we actually know the resource index!

                // == Handle Stage == //

                switch (chunk_stage) {
                        case PlanetProcessingStages::Invalid:
                                exit(200);
                        case PlanetProcessingStages::StageOne:
                                break; // GPU
                        case PlanetProcessingStages::AllocateBuffers:
                                AllocateBuffers(chunk_progress);
                                break;
                        case PlanetProcessingStages::StageTwo:
                                break; // GPU
                        case PlanetProcessingStages::CopyMeshData:
                                {
                                        ModelID chunk_model_id = chunks[chunk_progress.chunk_index].model_id;
                                        Mesh&   chunk_mesh     = game_context->model_system[chunk_model_id].meshes[0];

                                        u64 buffer_size          = chunk_mesh.vertices_size + (sizeof(u32) * chunk_mesh.index_count);
                                        chunk_progress.mesh_data = (u8*)malloc(buffer_size);

                                        GPUBufferReadInfo read_info{
                                                .src    = &chunk_mesh.buffer,
                                                .offset = 0,
                                                .size   = buffer_size,
                                                .dst    = chunk_progress.mesh_data,

                                                .src_queue_family = game_context->compute_queue_family,
                                                .dst_queue_family = game_context->graphics_queue_family,
                                        };

                                        chunk_progress.copy_id = game_context->transfer_engine.QueueGPUBufferRead(read_info);

                                        // Signal to CreatingBVH

                                        VkSemaphoreSignalInfo signal_info{};
                                        signal_info.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
                                        signal_info.semaphore = chunk_progress.semaphore;
                                        signal_info.value     = chunk_progress.semaphore_start_value + (u64)PlanetProcessingStages::CreatingBVH;

                                        vkSignalSemaphore(game_context->vulkan_device, &signal_info);
                                }
                                break;
                        case PlanetProcessingStages::CreatingBVH:
                                {
                                        if (!game_context->transfer_engine.Check(chunk_progress.copy_id)) break;

                                        // Create BVH
                                        ModelID chunk_model_id = chunks[chunk_progress.chunk_index].model_id;
                                        Mesh&   chunk_mesh     = game_context->model_system[chunk_model_id].meshes[0];
                                        chunk_mesh.vertices    = (VertexDrawData*)malloc(chunk_mesh.vertices_size);
                                        memcpy(chunk_mesh.vertices, chunk_progress.mesh_data, chunk_mesh.vertices_size);

                                        u64 indices_size   = chunk_mesh.index_count * sizeof(u32);
                                        chunk_mesh.indices = (u32*)malloc(indices_size);
                                        memcpy(chunk_mesh.indices, chunk_progress.mesh_data + chunk_mesh.vertices_size, indices_size);
                                        // chunk_mesh.indices = (u32*)(chunk_progress.mesh_data + chunk_mesh.vertices_size);

                                        chunk_mesh.bvh.Init(chunk_mesh.vertices, chunk_mesh.vertices_size / sizeof(VertexDrawData), chunk_mesh.indices,
                                                            chunk_mesh.index_count);

                                        free(chunk_progress.mesh_data);
                                        chunk_progress.mesh_data = nullptr;

                                        // Signal to Finished

                                        VkSemaphoreSignalInfo signal_info{};
                                        signal_info.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
                                        signal_info.semaphore = chunk_progress.semaphore;
                                        signal_info.value     = chunk_progress.semaphore_start_value + (u64)PlanetProcessingStages::Finished;

                                        vkSignalSemaphore(game_context->vulkan_device, &signal_info);
                                }
                                break;
                        case PlanetProcessingStages::Finished:
                                {
                                        chunks[chunk_progress.chunk_index].state = ChunkState::Initialised;
                                        FreeChunkInProgress(chunk_progress_index);
                                }
                                break;
                        default:
                                std::println("Error: Unexpected chunk processing stage: {}", (int)chunk_stage);
                                exit(200);
                }
        }
}

void Planet::Update() {
        // ===== Check all in progress chunks if they are ready to go to next stage ===== //

        UpdateStates();

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

u32 Planet::GetCommandBufferIndex() {
        if (free_command_buffers.empty()) return u32_max;

        u32 index = free_command_buffers.back();
        free_command_buffers.pop_back();

        return index;
}

void Planet::ReleaseCommandBufferIndex(u32 index) {
        VkResult res = vkResetCommandBuffer(command_buffers[index], 0);
        if (res != VK_SUCCESS) {
                std::println("Failed to reset chunk command buffer: {}", (u32)res);
                exit(1);
        }
        free_command_buffers.push_back(index);
}

ChunkID Planet::GenerateChunk(AABB bounds, u32 lod) {
        // ===== Initialise Creation Info ===== //

        Vec3 chunk_position = bounds.center - bounds.radius;

        PlanetCreationInfo info{
                .edge_case_lookup = edge_lookup_buffer.device_address,
                .triangle_lookup  = triangle_lookup_buffer.device_address,

                .chunk_cells = CHUNK_CELLS,
                .chunk_dims  = (u32)(bounds.radius.x * 2),
                .position    = chunk_position,
                .seed        = PLANET_SEED,
        };


        // ===== Find a chunk index ===== //

        ChunkID chunk_index = 0;

        if (next_free_chunk == NO_FREE) {
                chunk_index = (u32)chunks.size();
                chunks.push_back({});
        } else {
                chunk_index     = next_free_chunk;
                next_free_chunk = chunks[next_free_chunk].next_free_index;
        }

        // ===== Set Chunk State ===== //

        chunks[chunk_index].state    = ChunkState::Waiting;
        chunks[chunk_index].position = chunk_position;
        chunks[chunk_index].lod      = lod;
        chunks[chunk_index].model_id = u32_max;
        chunks[chunk_index].is_empty = false;

        chunks_in_progress.emplace_back(chunk_index, info, 0, VK_NULL_HANDLE, GPUBuffer{}, GPUBuffer{}, GPUBuffer{}, u32_max);

        return chunk_index;
}

void Planet::DestroyChunk(ChunkID chunk_index) {
        // ===== Check if we can Destroy Now ===== //

        ChunkState chunk_state = chunks[chunk_index].state;

        bool can_destroy = false;

        // ===== Check If Resources In Use ===== //

        switch (chunk_state) {
                case ChunkState::None:
                        can_destroy = true;
                        break;
                case ChunkState::Waiting:
                case ChunkState::Initialised:
                        chunks[chunk_index].state = ChunkState::WaitingOnDestroy;
                        break;
                case ChunkState::WaitingOnDestroy:
                        if (chunks[chunk_index].model_id == u32_max) {
                                can_destroy = true;
                                break;
                        }
                        const Model& model = game_context->model_system[chunks[chunk_index].model_id];

                        can_destroy = model.last_frame_rendered < game_context->GetLastFinishedFrame();
                        can_destroy = can_destroy or (model.mesh_count > 0 and model.meshes[0].index_count == 0);
                        break;
        }

        if (can_destroy) {
                // ===== Destroy Chunk ===== //
                if (chunks[chunk_index].model_id != u32_max) {
                        game_context->model_system.DestroyModel(chunks[chunk_index].model_id);
                }

                if (chunks[chunk_index].index_offsets) free(chunks[chunk_index].index_offsets);

                chunks[chunk_index]          = {};
                chunks[chunk_index].model_id = u32_max;

                chunks[chunk_index].next_free_index = next_free_chunk;
                next_free_chunk                     = chunk_index;

                return;
        }

        // ===== Add back to list if cant destroy ===== //

        if (destroy_count < chunks_to_destroy.size()) chunks_to_destroy[destroy_count] = chunk_index;
        else chunks_to_destroy.push_back(chunk_index);

        destroy_count++;
}

void Planet::FreeChunkInProgress(u32 index) {
        PlanetChunkProgress& chunk_progress = chunks_in_progress[index];

        // ===== Set semaphore to invalid state ===== //

        if (chunk_progress.resource_index != u32_max) {
                ReleaseCommandBufferIndex(chunk_progress.resource_index);

                VkSemaphoreSignalInfo signal_info{};
                signal_info.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
                signal_info.semaphore = chunk_progress.semaphore;
                signal_info.value     = chunk_progress.semaphore_start_value + (u64)PlanetProcessingStages::Count;

                vkSignalSemaphore(game_context->vulkan_device, &signal_info);
        }

        // ===== Free Resources ===== //

        if (chunk_progress.edge_buffer.buffer != VK_NULL_HANDLE) {
                DestroyGPUBuffer(game_context->vulkan_device, chunk_progress.edge_buffer, game_context->vulkan_allocator);
        }
        if (chunk_progress.cell_sums_buffer.buffer != VK_NULL_HANDLE) {
                DestroyGPUBuffer(game_context->vulkan_device, chunk_progress.cell_sums_buffer, game_context->vulkan_allocator);
        }
        if (chunk_progress.normal_buffer.buffer != VK_NULL_HANDLE) {
                DestroyGPUBuffer(game_context->vulkan_device, chunk_progress.normal_buffer, game_context->vulkan_allocator);
        }

        // ===== Remove Chunk In Progress ===== //

        chunks_in_progress[index] = chunks_in_progress.back();
        chunks_in_progress.pop_back();
}

// NOTE: I think this could easily have pre-recorded command buffers.

// NOTE: If i want to use pre recorded buffers I will need to move away from push constants and write to uniform buffer instead
// as push constants are written to the command buffer. But can use for Constant Data.

// Queue Pass 1 and Pass 2
void Planet::RecordStageOne(PlanetChunkProgress& chunk_progress) {
        VkResult res{};

        // ===== Get and Begin Command Buffer ===== //

        VkCommandBuffer command_buffer = command_buffers[chunk_progress.resource_index];

        VkCommandBufferBeginInfo command_buffer_begin_info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        res = vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);
        if (res != VK_SUCCESS) exit(res);

        // ===== Record Commands ===== //

        constexpr u32 PASS_1_X_DISPATCH_COUNT = (CHUNK_EDGES + (THREAD_GROUP_X - 1)) / THREAD_GROUP_X;
        constexpr u32 PASS_1_Y_DISPATCH_COUNT = (CHUNK_EDGES + (THREAD_GROUP_Y - 1)) / THREAD_GROUP_Y;

        // == Pass One == //

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[(u32)PlanetPipelineID::Pass1]);
        vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PlanetCreationInfo), &chunk_progress.info);
        vkCmdDispatch(command_buffer, PASS_1_X_DISPATCH_COUNT, PASS_1_Y_DISPATCH_COUNT, THREAD_GROUP_Z);

        // == Wait On Edges == //

        VkBufferMemoryBarrier2 buffer_barrier{
                .sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                .srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
                .buffer        = chunk_progress.edge_buffer.buffer,
                .offset        = 0,
                .size          = VK_WHOLE_SIZE,
        };

        VkDependencyInfo dependency_info{
                .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .bufferMemoryBarrierCount = 1,
                .pBufferMemoryBarriers    = &buffer_barrier,
        };

        vkCmdPipelineBarrier2(command_buffer, &dependency_info);

        // == Pass Two == //

        constexpr u32 PASS_2_X_DISPATCH_COUNT = (CHUNK_CELLS + (THREAD_GROUP_X - 1)) / THREAD_GROUP_X;
        constexpr u32 PASS_2_Y_DISPATCH_COUNT = (CHUNK_CELLS + (THREAD_GROUP_Y - 1)) / THREAD_GROUP_Y;

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[(u32)PlanetPipelineID::Pass2]);
        vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PlanetCreationInfo), &chunk_progress.info);
        vkCmdDispatch(command_buffer, PASS_2_X_DISPATCH_COUNT, PASS_2_Y_DISPATCH_COUNT, THREAD_GROUP_Z);

        // == Wait On Cell Sums == //

        buffer_barrier = {
                .sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                .srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
                .buffer        = chunk_progress.cell_sums_buffer.buffer,
                .offset        = 0,
                .size          = VK_WHOLE_SIZE,
        };

        dependency_info = {
                .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .bufferMemoryBarrierCount = 1,
                .pBufferMemoryBarriers    = &buffer_barrier,
        };

        vkCmdPipelineBarrier2(command_buffer, &dependency_info);

        // == Pass Three == //

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[(u32)PlanetPipelineID::Pass3]);
        vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PlanetCreationInfo), &chunk_progress.info);
        vkCmdDispatch(command_buffer, 1, 1, 1);

        // ===== End Command Buffer ===== //

        res = vkEndCommandBuffer(command_buffer);

        // ===== Submit ===== //

        VkSemaphore semaphore = chunk_progress.semaphore;

        VkSemaphoreSubmitInfo signal_semaphore_info{};
        signal_semaphore_info.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signal_semaphore_info.semaphore = semaphore;
        signal_semaphore_info.value     = chunk_progress.semaphore_start_value + (u64)PlanetProcessingStages::AllocateBuffers;
        signal_semaphore_info.stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

        VkCommandBufferSubmitInfo command_buffer_submit_info{};
        command_buffer_submit_info.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        command_buffer_submit_info.commandBuffer = command_buffer;

        VkSubmitInfo2 submit_info{};
        submit_info.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submit_info.signalSemaphoreInfoCount = 1;
        submit_info.pSignalSemaphoreInfos    = &signal_semaphore_info;
        submit_info.commandBufferInfoCount   = 1;
        submit_info.pCommandBufferInfos      = &command_buffer_submit_info;

        res = vkQueueSubmit2(game_context->compute_queue, 1, &submit_info, VK_NULL_HANDLE);
        if (res != VK_SUCCESS) {
                std::println("Failed Submit: {}", (int)res);
                exit(1);
        }
}

void Planet::AllocateBuffers(PlanetChunkProgress& chunk_progress) {
        // ===== Get Counts ===== //

        EdgeSumInfo* mapped_data = (EdgeSumInfo*)chunk_progress.cell_sums_buffer.allocation_info.pMappedData;

        u32 cell_count = CHUNK_CELLS;

        u32 last_index = (cell_count * cell_count) - 1;

        u32 index_count = mapped_data[last_index].triangle_offset;

        // ===== If no triangles we can finish early ===== //

        if (index_count == 0) {
                // ===== Signal Semaphore as Finished ===== //

                VkSemaphore semaphore = chunk_progress.semaphore;

                VkSemaphoreSignalInfo signal_info{};
                signal_info.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
                signal_info.semaphore = semaphore;
                signal_info.value     = chunk_progress.semaphore_start_value + (u64)PlanetProcessingStages::Finished;

                vkSignalSemaphore(game_context->vulkan_device, &signal_info);

                chunks[chunk_progress.chunk_index].is_empty = true;

                return;
        }

        // ===== Allocate Buffers ===== //

        VmaAllocationCreateFlags allocation_flags = 0;

        u32 vertex_count = mapped_data[last_index].vertex_offset;

        u64 vertex_buffer_size = sizeof(VertexDrawData) * vertex_count;
        u64 index_buffer_size  = sizeof(u32) * index_count;

        chunks[chunk_progress.chunk_index].model_id = game_context->model_system.CreateModel(vertex_count, index_count, 1);

        game_context->model_system[chunks[chunk_progress.chunk_index].model_id].material = planet_material;

        const GPUBuffer& buffer = game_context->model_system[chunks[chunk_progress.chunk_index].model_id].meshes[0].buffer;

        // == Temp Normals Buffer == //

        u64 normal_buffer_size = sizeof(Vec4) * 4 * vertex_count;

        chunk_progress.normal_buffer = CreateGPUBuffer(game_context->vulkan_device, game_context->vulkan_allocator, normal_buffer_size,
                                                       game_context->compute_queue_family, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, allocation_flags);

        // == Set Info Addresses == //

        chunk_progress.info.vertices = buffer.device_address;
        chunk_progress.info.indices  = buffer.device_address + vertex_buffer_size;
        chunk_progress.info.normals  = chunk_progress.normal_buffer.device_address;

        // ===== Signal Semaphore ===== //

        VkSemaphore semaphore = chunk_progress.semaphore;

        VkSemaphoreSignalInfo signal_info{};
        signal_info.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
        signal_info.semaphore = semaphore;
        signal_info.value     = chunk_progress.semaphore_start_value + (u64)PlanetProcessingStages::StageTwo;

        vkSignalSemaphore(game_context->vulkan_device, &signal_info);

        // ===== Start Next Stage ===== //
        RecordStageTwo(chunk_progress);
}

void Planet::RecordStageTwo(PlanetChunkProgress& chunk_progress) {
        VkResult res{};

        // ===== Get and Begin Command Buffer ===== //

        VkCommandBuffer command_buffer = command_buffers[chunk_progress.resource_index];

        // == Reset == //

        res = vkResetCommandBuffer(command_buffer, 0);
        if (res < 0) {
                std::println("Failed resetting command buffer: {}", (int)res);
                exit(1);
        }

        VkCommandBufferBeginInfo command_buffer_begin_info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        res = vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);
        if (res != VK_SUCCESS) exit(res);

        // ===== Record Commands ===== //

        // == Pass Four == //

        constexpr u32 PASS_4_X_DISPATCH_COUNT = (CHUNK_CELLS + (THREAD_GROUP_X - 1)) / THREAD_GROUP_X;
        constexpr u32 PASS_4_Y_DISPATCH_COUNT = (CHUNK_CELLS + (THREAD_GROUP_Y - 1)) / THREAD_GROUP_Y;

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[(u32)PlanetPipelineID::Pass4]);
        vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PlanetCreationInfo), &chunk_progress.info);
        vkCmdDispatch(command_buffer, PASS_4_X_DISPATCH_COUNT, PASS_4_Y_DISPATCH_COUNT, THREAD_GROUP_Z);

        // ===== Normals ===== //
        const Model& model = game_context->model_system[chunks[chunk_progress.chunk_index].model_id];

        VkBufferMemoryBarrier2 buffer_barrier{
                .sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                .srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
                .buffer        = model.meshes[0].buffer.buffer,
                .offset        = 0,
                .size          = VK_WHOLE_SIZE,
        };

        VkDependencyInfo dependency_info{
                .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .bufferMemoryBarrierCount = 1,
                .pBufferMemoryBarriers    = &buffer_barrier,
        };

        vkCmdPipelineBarrier2(command_buffer, &dependency_info);

        // == Pass Five == //

        constexpr u32 PASS_5_X_DISPATCH_COUNT = (CHUNK_CELLS + (THREAD_GROUP_X - 1)) / THREAD_GROUP_X;
        constexpr u32 PASS_5_Y_DISPATCH_COUNT = (CHUNK_CELLS + (THREAD_GROUP_Y - 1)) / THREAD_GROUP_Y;

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[(u32)PlanetPipelineID::Pass5]);
        vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PlanetCreationInfo), &chunk_progress.info);
        vkCmdDispatch(command_buffer, PASS_5_X_DISPATCH_COUNT, PASS_5_Y_DISPATCH_COUNT, THREAD_GROUP_Z);

        // ===== Normals Sum ===== //

        VkBufferMemoryBarrier2 buffer_barrier2{
                .sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                .srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
                .buffer        = chunk_progress.normal_buffer.buffer,
                .offset        = 0,
                .size          = VK_WHOLE_SIZE,
        };

        VkDependencyInfo dependency_info2{
                .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .bufferMemoryBarrierCount = 1,
                .pBufferMemoryBarriers    = &buffer_barrier2,
        };

        vkCmdPipelineBarrier2(command_buffer, &dependency_info2);

        // == Pass Five == //

        constexpr u32 PASS_6_X_DISPATCH_COUNT = (CHUNK_CELLS + (127 - 1)) / 127;
        constexpr u32 PASS_6_Y_DISPATCH_COUNT = ((CHUNK_CELLS / 4) + (THREAD_GROUP_Y - 1)) / THREAD_GROUP_Y;

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[(u32)PlanetPipelineID::Pass6]);
        vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PlanetCreationInfo), &chunk_progress.info);
        vkCmdDispatch(command_buffer, PASS_6_X_DISPATCH_COUNT, PASS_6_Y_DISPATCH_COUNT, THREAD_GROUP_Z);

        // ===== Release Mesh Buffer ===== //

        // VkBufferMemoryBarrier2 ownership_barrier{
        //         .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        //         .srcStageMask        = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        //         .srcAccessMask       = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        //         .dstStageMask        = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        //         .dstAccessMask       = VK_ACCESS_2_SHADER_READ_BIT,
        //         .srcQueueFamilyIndex = game_context->compute_queue_family,
        //         .dstQueueFamilyIndex = game_context->transfer_queue_family,
        //         .buffer              = model.meshes[0].buffer.buffer,
        //         .offset              = 0,
        //         .size                = VK_WHOLE_SIZE,
        // };

        // VkDependencyInfo dependency_info_ownership{
        //         .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        //         .bufferMemoryBarrierCount = 1,
        //         .pBufferMemoryBarriers    = &ownership_barrier,
        // };

        // vkCmdPipelineBarrier2(command_buffer, &dependency_info_ownership);

        BufferOwnershipInfo ownership_info{
                .command_buffer   = command_buffer,
                .buffer           = &model.meshes[0].buffer,
                .old_queue_family = game_context->compute_queue_family,
                .new_queue_family = game_context->transfer_queue_family,
                .stage_mask       = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .access_mask      = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        };

        CmdReleaseBufferOwnership(ownership_info);

        // ===== End Command Buffer ===== //

        res = vkEndCommandBuffer(command_buffer);

        // ===== Submit ===== //
        VkSemaphore semaphore = chunk_progress.semaphore;

        VkSemaphoreSubmitInfo signal_semaphore_info{};
        signal_semaphore_info.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signal_semaphore_info.pNext     = nullptr;
        signal_semaphore_info.semaphore = semaphore;
        signal_semaphore_info.value     = chunk_progress.semaphore_start_value + (u64)PlanetProcessingStages::CopyMeshData;
        signal_semaphore_info.stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

        VkSemaphoreSubmitInfo signal_ownership_semaphore_info{};
        signal_ownership_semaphore_info.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signal_ownership_semaphore_info.pNext     = nullptr;
        signal_ownership_semaphore_info.semaphore = model.meshes[0].buffer.ownership_semaphore;
        signal_ownership_semaphore_info.stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

        VkSemaphoreSubmitInfo signal_semaphore_infos[2] = { signal_semaphore_info, signal_ownership_semaphore_info };

        VkCommandBufferSubmitInfo command_buffer_submit_info{};
        command_buffer_submit_info.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        command_buffer_submit_info.commandBuffer = command_buffer;

        VkSubmitInfo2 submit_info{};
        submit_info.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submit_info.signalSemaphoreInfoCount = 2;
        submit_info.pSignalSemaphoreInfos    = &signal_semaphore_infos[0];
        submit_info.commandBufferInfoCount   = 1;
        submit_info.pCommandBufferInfos      = &command_buffer_submit_info;

        vkQueueSubmit2(game_context->compute_queue, 1, &submit_info, VK_NULL_HANDLE);
}

// Return distance intersected
float Planet::CheckIntersection(Ray ray, float ray_length) {
        float closest_distance = f32_max;

        AABB child_bounds{};
        child_bounds.radius = tree.root.radius / 2.0f;

        for (u32 child_index = 0; child_index < 8; child_index++) {
                child_bounds.center = tree.root.center + (PARENT_CENTER_OFFSET[child_index] * child_bounds.radius);
                float distance      = tree.RayIntersect(child_index, child_bounds, ray, ray_length);
                closest_distance    = std::min(closest_distance, distance);
        }

        return closest_distance;
}

float PlanetChunkTree::RayIntersect(NodeID node_index, AABB bounds, Ray ray, float ray_length) {
        if (nodes[node_index].IsEmpty()) return f32_max;
        if (!RayAABB(ray, bounds)) return f32_max;

        // Check children
        float t = f32_max;

        if (nodes[node_index].HasChunk()) {
                PlanetChunk& chunk = planet->chunks[nodes[node_index].chunk_index];


                ModelID chunk_model_id = planet->chunks[nodes[node_index].chunk_index].model_id;

                if (chunk_model_id != u32_max and planet->game_context->model_system[chunk_model_id].mesh_count > 0) {
                        Mesh& chunk_mesh = planet->game_context->model_system[chunk_model_id].meshes[0];

                        if (chunk_mesh.bvh.nodes.size() > 0) t = chunk_mesh.Traverse(1, ray, ray_length);
                }
        }

        if (nodes[node_index].HasChildren()) {
                NodeID first_child_index = nodes[node_index].first_child_index;

                AABB child_bounds{};
                child_bounds.radius = bounds.radius / 2.0f;

                for (u32 child_offset = 0; child_offset < 8; child_offset++) {
                        NodeID child_index = first_child_index + child_offset;

                        child_bounds.center = bounds.center + (PARENT_CENTER_OFFSET[child_offset] * child_bounds.radius);
                        float distance      = RayIntersect(child_index, child_bounds, ray, ray_length);
                        t                   = std::min(t, distance);
                }
        }

        return t;
}