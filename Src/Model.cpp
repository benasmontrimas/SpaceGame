#include "Model.h"

#include "Game.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <volk/volk.h>

#include <ktx.h>
#include <ktxvulkan.h>

#include <print>

i32 model_count;

void Model::Init(GameContext* game_context, VertexDrawData* vertices, u64 vertex_count, u32* indices, u64 index_count, u32 _max_instance_count) {
        model_count++;
        assert(mesh_count == 0 and "Initialising a model which is already initialised");

        meshes = new Mesh[1]();
        mesh_count++;

        // ===== BVH ===== //

        // meshes[0].bvh.Init(vertices, (u32)vertex_count, indices, (u32)index_count);

        // ===== Allocate Buffer ===== //

        VkDeviceSize vertex_buffer_size = sizeof(VertexDrawData) * vertex_count;
        VkDeviceSize index_buffer_size  = sizeof(u32) * index_count;
        VkDeviceSize buffer_size        = vertex_buffer_size + index_buffer_size;

        VmaAllocationCreateFlags allocation_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        meshes[0].buffer = CreateGPUBuffer(game_context->vulkan_device, game_context->vulkan_allocator, buffer_size, game_context->graphics_queue_family,
                                           VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, allocation_flags);

        // ===== Copy Vertex and Index Data ===== //

        memcpy(meshes[0].buffer.allocation_info.pMappedData, vertices, vertex_buffer_size);
        memcpy(((char*)meshes[0].buffer.allocation_info.pMappedData) + vertex_buffer_size, indices, index_buffer_size);

        meshes[0].index_count   = index_count;
        meshes[0].vertices_size = (u64)vertex_buffer_size;

        // ===== Allocate Instance Buffer ===== //

        max_instance_count = _max_instance_count;

        u64 instance_buffer_size = sizeof(InstanceDrawData) * max_instance_count * max_frames_in_flight;

        instance_buffer = CreateGPUBuffer(game_context->vulkan_device, game_context->vulkan_allocator, instance_buffer_size,
                                          game_context->graphics_queue_family, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, allocation_flags);

        instance_draw_data = (InstanceDrawData*)malloc(sizeof(InstanceDrawData) * max_instance_count);

        instance_count = 0;
}

void Model::Init(GameContext* game_context, u64 vertex_count, u64 index_count, u32 _max_instance_count) {
        model_count++;
        assert(mesh_count == 0 and "Initialising a model which is already initialised");

        meshes = new Mesh[1]();
        mesh_count++;

        // ===== Allocate Buffer ===== //

        VkDeviceSize vertex_buffer_size = sizeof(VertexDrawData) * vertex_count;
        VkDeviceSize index_buffer_size  = sizeof(u32) * index_count;
        VkDeviceSize buffer_size        = vertex_buffer_size + index_buffer_size;

        VmaAllocationCreateFlags allocation_flags = 0; // VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        meshes[0].buffer = CreateGPUBuffer(game_context->vulkan_device, game_context->vulkan_allocator, buffer_size, game_context->graphics_queue_family,
                                           VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                           allocation_flags);

        meshes[0].index_count   = index_count;
        meshes[0].vertices_size = (u64)vertex_buffer_size;

        // ===== Allocate Instance Buffer ===== //

        max_instance_count = _max_instance_count;

        u64 instance_buffer_size = sizeof(InstanceDrawData) * max_instance_count * max_frames_in_flight;

        allocation_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        instance_buffer =
                CreateGPUBuffer(game_context->vulkan_device, game_context->vulkan_allocator, instance_buffer_size, game_context->graphics_queue_family,
                                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, allocation_flags);

        instance_draw_data = (InstanceDrawData*)malloc(sizeof(InstanceDrawData) * max_instance_count);

        instance_count = 0;
}

void Model::LoadFromOBJ(GameContext* game_context, const std::string& file_name, u32 _max_instance_count) {
        assert(mesh_count == 0 and "Initialising a model which is already initialised");
        model_count++;

        tinyobj::attrib_t                attrib;
        std::vector<tinyobj::shape_t>    shapes;
        std::vector<tinyobj::material_t> materials;

        bool load_ok = tinyobj::LoadObj(&attrib, &shapes, &materials, nullptr, file_name.c_str());

        if (!load_ok) {
                std::println("Failed loading mesh: {}", file_name);
                exit(1);
        }

        mesh_count = (u32)shapes.size();
        meshes     = new Mesh[mesh_count]();

        for (size_t mesh_index = 0; mesh_index < mesh_count; mesh_index++) {
                std::vector<VertexDrawData> vertices;
                std::vector<u32>            indices;

                vertices.reserve(shapes[mesh_index].mesh.indices.size());
                indices.reserve(shapes[mesh_index].mesh.indices.size());

                for (tinyobj::index_t& index : shapes[mesh_index].mesh.indices) {
                        VertexDrawData v{};

                        if (attrib.vertices.size() > 0) {
                                v.position = { attrib.vertices[index.vertex_index * 3 + 0], -attrib.vertices[index.vertex_index * 3 + 1],
                                               attrib.vertices[index.vertex_index * 3 + 2] };
                        }

                        if (attrib.normals.size() > 0) {
                                v.normal = { attrib.normals[index.normal_index * 3 + 0], -attrib.normals[index.normal_index * 3 + 1],
                                             attrib.normals[index.normal_index * 3 + 2] };
                        }

                        if (attrib.texcoords.size() > 0) {
                                v.uv = { attrib.texcoords[index.texcoord_index * 2 + 0], 1.0 - attrib.texcoords[index.texcoord_index * 2 + 1] };
                        }

                        vertices.push_back(v);
                        indices.push_back((u32)indices.size());
                }

                // ===== Create Mesh ===== //

                // ===== Allocate Vertex and Index Buffer ===== //

                VkDeviceSize vertex_buffer_size{ sizeof(VertexDrawData) * vertices.size() };
                VkDeviceSize index_buffer_size{ sizeof(u32) * indices.size() };
                VkDeviceSize buffer_size{ vertex_buffer_size + index_buffer_size };

                VmaAllocationCreateFlags allocation_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;


                // ===== BVH ===== //

                // meshes[0].bvh.Init(&vertices[0], (u32)vertices.size(), &indices[0], (u32)indices.size());


                meshes[mesh_index].buffer =
                        CreateGPUBuffer(game_context->vulkan_device, game_context->vulkan_allocator, buffer_size, game_context->graphics_queue_family,
                                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, allocation_flags);

                // ===== Copy Vertex and Index Data ===== //

                memcpy(meshes[mesh_index].buffer.allocation_info.pMappedData, vertices.data(), vertex_buffer_size);
                memcpy(((char*)meshes[mesh_index].buffer.allocation_info.pMappedData) + vertex_buffer_size, indices.data(), index_buffer_size);

                meshes[mesh_index].index_count   = (u64)indices.size();
                meshes[mesh_index].vertices_size = (u64)vertex_buffer_size;
        }

        // ===== Allocate Instance Buffer ===== //

        VmaAllocationCreateFlags allocation_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        max_instance_count = _max_instance_count;

        u64 instance_buffer_size = sizeof(InstanceDrawData) * max_instance_count * max_frames_in_flight;

        instance_buffer = CreateGPUBuffer(game_context->vulkan_device, game_context->vulkan_allocator, instance_buffer_size,
                                          game_context->graphics_queue_family, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, allocation_flags);

        // Instance Array

        instance_draw_data = (InstanceDrawData*)malloc(sizeof(InstanceDrawData) * max_instance_count);
        instance_count     = 0;
}

void Model::Destroy(GameContext* game_context) {
        model_count--;

        // std::println("Model Count: {}", model_count);

        for (u32 mesh_index = 0; mesh_index < mesh_count; mesh_index++) {
                DestroyGPUBuffer(game_context->vulkan_device, meshes[mesh_index].buffer, game_context->vulkan_allocator);
                free(meshes[mesh_index].indices);
                free(meshes[mesh_index].vertices);
        }

        DestroyGPUBuffer(game_context->vulkan_device, instance_buffer, game_context->vulkan_allocator);

        delete[] (meshes);
        free(instance_draw_data);
}

void Model::Draw(GameContext* game_context, VkCommandBuffer command_buffer, VkPipelineLayout pipeline_layout, u32 frame_index) {
        if (instance_count == 0) return;

        // ===== Update Per Model Data ===== //

        ModelDrawData mesh_draw_data;
        mesh_draw_data.material = material;
        vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ModelDrawData), &mesh_draw_data);

        // ===== Update Per Instance Data ===== //

        u64 instance_buffer_offset  = sizeof(InstanceDrawData) * max_instance_count * frame_index;
        u8* instance_buffer_ptr     = ((u8*)instance_buffer.allocation_info.pMappedData) + instance_buffer_offset;
        u64 instance_draw_data_size = sizeof(InstanceDrawData) * instance_count;
        memcpy(instance_buffer_ptr, &instance_draw_data[0], instance_draw_data_size);

        // ===== Draw Meshes ===== //

        for (u32 mesh_index = 0; mesh_index < mesh_count; mesh_index++) {
                Mesh& mesh = meshes[mesh_index];
                assert(mesh.index_count != 0 and "Empty mesh is an error");

                // == Bind Vertex + Index Buffers == //

                VkDeviceSize vertex_offset[2] = { 0, instance_buffer_offset };
                VkBuffer     buffers[2]       = { mesh.buffer.buffer, instance_buffer.buffer };
                vkCmdBindVertexBuffers(command_buffer, 0, 2, &buffers[0], &vertex_offset[0]);

                u32 index_offset = (u32)mesh.vertices_size;
                vkCmdBindIndexBuffer(command_buffer, mesh.buffer.buffer, index_offset, VK_INDEX_TYPE_UINT32);

                // == Draw == //

                vkCmdDrawIndexed(command_buffer, (u32)mesh.index_count, (u32)instance_count, 0, 0, 0);
        }

        instance_count = 0;
}

// ===== Texture ===== //


void Texture::Load(VkDevice device, VkCommandPool command_pool, VkQueue queue, const std::string& file_name, const VmaAllocator& allocator) {
        VkResult res{};

        ktxTexture* ktx_texture{};

        ktxTexture_CreateFromNamedFile(file_name.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktx_texture);

        VkImageCreateInfo texture_image_info {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = VK_IMAGE_TYPE_2D,
                .format = ktxTexture_GetVkFormat(ktx_texture),
                .extent = {
                        .width = ktx_texture->baseWidth,
                        .height = ktx_texture->baseHeight,
                        .depth = 1,
                },
                .mipLevels = ktx_texture->numLevels,
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        VmaAllocationCreateInfo texture_image_allocation_info{
                // .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                .usage = VMA_MEMORY_USAGE_AUTO,
        };

        res = vmaCreateImage(allocator, &texture_image_info, &texture_image_allocation_info, &image, &allocation, nullptr);

        if (res != VK_SUCCESS) {
                std::println("Failed creating texture image");
                exit(res);
        }

        VkImageViewCreateInfo texture_image_view_info {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = texture_image_info.format,
                .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .levelCount = ktx_texture->numLevels,
                        .layerCount = 1,
                },
        };

        res = vkCreateImageView(device, &texture_image_view_info, nullptr, &image_view);

        if (res != VK_SUCCESS) {
                std::println("Failed creating texture image view");
                exit(res);
        }

        // ===== Upload Texture To GPU =====
        // TODO: Use a Transfer Queue? Can copy images whilst rendering.

        VkBuffer          image_src_buffer{};
        VmaAllocation     image_src_allocation{};
        VmaAllocationInfo image_src_allocation_info{};

        VkBufferCreateInfo image_src_buffer_info{
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size  = (u32)ktx_texture->dataSize,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        };

        VmaAllocationCreateInfo image_src_allocation_create_info{
                .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                .usage = VMA_MEMORY_USAGE_AUTO,
        };


        res = vmaCreateBuffer(allocator, &image_src_buffer_info, &image_src_allocation_create_info, &image_src_buffer, &image_src_allocation,
                              &image_src_allocation_info);

        if (res != VK_SUCCESS) {
                std::println("Failed creating buffer for image upload");
                exit(res);
        }

        memcpy(image_src_allocation_info.pMappedData, ktx_texture->pData, ktx_texture->dataSize);

        VkFenceCreateInfo fence_info{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };

        VkFence fence{};
        res = vkCreateFence(device, &fence_info, nullptr, &fence);

        if (res != VK_SUCCESS) {
                std::println("Failed creating fence for texture upload");
                exit(res);
        }

        VkCommandBuffer command_buffer{};

        VkCommandBufferAllocateInfo command_buffer_allocate_info{
                .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool        = command_pool,
                .commandBufferCount = 1,
        };

        res = vkAllocateCommandBuffers(device, &command_buffer_allocate_info, &command_buffer);

        if (res != VK_SUCCESS) {
                std::println("Failed allocating command buffer for texture upload");
                exit(res);
        }

        VkCommandBufferBeginInfo command_buffer_begin_info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        res = vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);

        if (res != VK_SUCCESS) {
                std::println("Failed begining command buffer");
                exit(res);
        }

        VkImageMemoryBarrier2 texture_barrier {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask = VK_ACCESS_2_NONE,
                .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .image = image,
                .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .levelCount = ktx_texture->numLevels,
                        .layerCount = 1,
                },
        };

        VkDependencyInfo texture_barrier_info{
                .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers    = &texture_barrier,
        };

        vkCmdPipelineBarrier2(command_buffer, &texture_barrier_info);

        std::vector<VkBufferImageCopy> copy_regions;
        copy_regions.resize(ktx_texture->numLevels);

        std::println("Copy Regions: {}", ktx_texture->numLevels);
        for (u32 i = 0; i < copy_regions.size(); i++) {
                ktx_size_t     mip_offset{ 0 };
                KTX_error_code ktx_err = ktxTexture_GetImageOffset(ktx_texture, i, 0, 0, &mip_offset);

                if (ktx_err != 0) {
                        std::println("Failed getting image offset for mip level");
                        exit(ktx_err);
                }

                copy_regions[i] = {
                        .bufferOffset = mip_offset,
                        .imageSubresource = {
                                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                .mipLevel = i,
                                .layerCount = 1,
                        },
                        .imageExtent = {
                                .width = ktx_texture->baseWidth >> i,
                                .height = ktx_texture->baseHeight >> i,
                                .depth = 1,
                        }
                };

                std::println("Texture: {}, {}", copy_regions[i].imageExtent.width, copy_regions[i].imageExtent.height);
        }


        vkCmdCopyBufferToImage(command_buffer, image_src_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (u32)copy_regions.size(), copy_regions.data());

        VkImageMemoryBarrier2 texture_read_barrier {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                .image = image,
                .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .levelCount = ktx_texture->numLevels,
                        .layerCount = 1,
                },
        };

        texture_barrier_info.pImageMemoryBarriers = &texture_read_barrier;

        vkCmdPipelineBarrier2(command_buffer, &texture_barrier_info);

        res = vkEndCommandBuffer(command_buffer);

        if (res != VK_SUCCESS) {
                std::println("Failed ending command buffer");
                exit(res);
        }

        VkSubmitInfo submit_info{
                .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers    = &command_buffer,
        };

        res = vkQueueSubmit(queue, 1, &submit_info, fence);

        if (res != VK_SUCCESS) {
                std::println("Failed submitting upload commands");
                exit(res);
        }

        res = vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);

        if (res != VK_SUCCESS) {
                std::println("Failed waiting on texture upload fence");
                exit(res);
        }

        // ===== Free Resource =====

        vkDestroyFence(device, fence, nullptr);
        vmaDestroyBuffer(allocator, image_src_buffer, image_src_allocation);
        vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);

        // ===== Sampler =====

        VkSamplerCreateInfo sampler_info{
                .sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .magFilter        = VK_FILTER_LINEAR,
                .minFilter        = VK_FILTER_LINEAR,
                .mipmapMode       = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                .anisotropyEnable = VK_TRUE,
                .maxAnisotropy    = 1.0f,
                .maxLod           = (float)ktx_texture->numLevels,
        };

        res = vkCreateSampler(device, &sampler_info, nullptr, &sampler);

        if (res != VK_SUCCESS) {
                std::println("Failed creating sampler");
                exit(res);
        }

        ktxTexture_Destroy(ktx_texture);
}

void Texture::Destroy(GameContext* game_context) {
        // Destroy Sampler?
        vkDestroySampler(game_context->vulkan_device, sampler, nullptr);

        // Destroy Image View
        vkDestroyImageView(game_context->vulkan_device, image_view, nullptr);

        // Destroy Image
        vmaDestroyImage(game_context->vulkan_allocator, image, allocation);
}

// ===== MODEL SYSTEM ===== //

void ModelSystem::Init(GameContext* _game_context) {
        game_context = _game_context;

        next_free_index = 0;
}

ModelID ModelSystem::GetNewModelID() {
        if (next_free_index >= models.size()) {
                models.push_back({});
                ModelID id = next_free_index;

                next_free_index++;

                return id;
        }

        ModelID id       = next_free_index;
        next_free_index  = models[next_free_index].next_free_index;
        models[id].model = {};

        return id;
}

ModelID ModelSystem::LoadModel(const std::string& file_path, u32 max_instance_count, bool is_transparent) {
        ModelID id = GetNewModelID();

        Model& model = models[id].model;
        model        = {};
        model.LoadFromOBJ(game_context, file_path, max_instance_count);
        model.is_transparent = is_transparent;

        return id;
}

ModelID ModelSystem::CreateModel(VertexDrawData* vertices, u64 vertex_count, u32* indices, u64 index_count, u32 _max_instance_count, bool is_transparent) {
        ModelID id = GetNewModelID();

        Model& model = models[id].model;
        model        = {};
        model.Init(game_context, vertices, vertex_count, indices, index_count, _max_instance_count);
        model.is_transparent = is_transparent;

        return id;
}

ModelID ModelSystem::CreateModel(u64 vertex_count, u64 index_count, u32 _max_instance_count, bool is_transparent) {
        ModelID id = GetNewModelID();

        Model& model = models[id].model;
        model        = {};
        model.Init(game_context, vertex_count, index_count, _max_instance_count);
        model.is_transparent = is_transparent;

        return id;
}

ModelID ModelSystem::AddModel(const Model& model) {
        ModelID id = GetNewModelID();

        models[id].model = model;

        return id;
}

void ModelSystem::DestroyModel(ModelID id) {
        models[id].model.Destroy(game_context);

        ModelID free_index = next_free_index;
        if (id < free_index) {
                models[id].next_free_index = free_index;
                next_free_index            = id;
                return;
        }

        while (id > models[free_index].next_free_index) {
                free_index = models[free_index].next_free_index;
        }

        models[id].next_free_index         = models[free_index].next_free_index;
        models[free_index].next_free_index = id;
}

ModelInstance ModelSystem::CreateModelInstance(ModelID id) const {
        return ModelInstance{
                .model_id   = id,
                .transform  = {},
                .user_value = {},
        };
}

void ModelSystem::Draw(ModelInstance instance) {
        Model& model = models[instance.model_id].model;

        Mat4 translate_matrix = glm::translate(Mat4(1.0f), instance.transform.position);
        Mat4 rotate_matrix    = glm::mat4_cast(instance.transform.rotation);
        Mat4 scale_matrix     = glm::scale(Mat4(1.0f), instance.transform.scale);

        Mat4 model_matrix = translate_matrix * rotate_matrix * scale_matrix;

        model.instance_draw_data[model.instance_count] = { model_matrix,         instance.colour0,     instance.colour1,    instance.user_value,
                                                           instance.user_value1, instance.user_value2, instance.user_value3 };
        model.instance_count++;

        model.last_frame_rendered = (u32)game_context->frames_submitted + 2;
}

void Model::Acquire(GameContext* game_context, VkCommandBuffer command_buffer, std::vector<VkSemaphore>& wait_semaphores) {
        if (instance_count == 0) return;

        for (u32 i = 0; i < mesh_count; i++) {
                Mesh& mesh = meshes[i];

                if (mesh.buffer.owning_queue_family != game_context->graphics_queue_family) {
                        BufferOwnershipInfo ownership_info{
                                .command_buffer   = command_buffer,
                                .buffer           = &mesh.buffer,
                                .old_queue_family = game_context->transfer_queue_family,
                                .new_queue_family = game_context->graphics_queue_family,
                                .stage_mask       = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
                                .access_mask      = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
                        };

                        CmdAcquireBufferOwnership(ownership_info);

                        wait_semaphores.push_back(mesh.buffer.ownership_semaphore);
                }
        }
}

void ModelSystem::Acquire(VkCommandBuffer command_buffer, std::vector<VkSemaphore>& wait_semaphores) {
        u32 free_index = next_free_index;

        std::vector<u32> draw_after_list;

        for (u32 model_index = 0; model_index < models.size(); model_index++) {
                // Skip free slots
                if (model_index == free_index) {
                        free_index = models[free_index].next_free_index;
                        continue;
                }

                models[model_index].model.Acquire(game_context, command_buffer, wait_semaphores);
        }
}

void ModelSystem::Draw(VkCommandBuffer command_buffer, u32 frame_index) {
        u32 free_index = next_free_index;

        std::vector<u32> draw_after_list;

        for (u32 model_index = 0; model_index < models.size(); model_index++) {
                // Skip free slots
                if (model_index == free_index) {
                        free_index = models[free_index].next_free_index;
                        continue;
                }

                if (models[model_index].model.is_transparent) {
                        draw_after_list.push_back(model_index);
                        continue;
                }

                models[model_index].model.Draw(game_context, command_buffer, game_context->pipeline_layout, frame_index);
        }

        for (u32 i = 0; i < draw_after_list.size(); i++) {
                models[draw_after_list[i]].model.Draw(game_context, command_buffer, game_context->pipeline_layout, frame_index);
        }
}

// ===== Triangle BVH ===== //

void TriangleBVH::Init(VertexDrawData* vertices, u32 vertex_count, u32* indices, u32 index_count) {
        nodes.reserve((index_count / 2) * 2);
        nodes.resize(root_node + 1);

        // nodes[root_node].bounds.center = vertices[0].position;
        nodes[root_node].bounds.radius = { -f32_max, -f32_max, -f32_max };

        for (u32 i = 0; i < vertex_count; i++) {
                nodes[root_node].bounds.Extend(vertices[i].position);
        }

        Split(vertices, indices, index_count, 0, root_node);
}

float TriangleBVH::CalculateCost(float parent_area, u32 index_count, const TriangleBVHNode& left, const TriangleBVHNode& right) {
        float left_area  = left.bounds.Area();
        float right_area = right.bounds.Area();

        float left_triangle_count  = float(left.index_count / 3);
        float right_triangle_count = float(right.index_count / 3);

        float cost = bounds_intersect_cost + (left_area / parent_area) * left_triangle_count * triangle_intersect_cost +
                     (right_area / parent_area) * right_triangle_count * triangle_intersect_cost;

        return cost;
}

bool TriangleBVH::GetBestSplit(VertexDrawData* vertices, u32* indices, u32 index_count, u32 node_index, Vec3* split_position) {
        constexpr u32   dimension_count                                 = 3;
        constexpr u32   bin_count                                       = bin_count_per_dimension * dimension_count;
        TriangleBVHNode bins[bin_count_per_dimension * dimension_count] = {};

        for (u32 bin_index = 0; bin_index < bin_count; bin_index++) {
                bins[bin_index].bounds.radius = { -f32_max, -f32_max, -f32_max };
        }

        Vec3 node_min = nodes[node_index].bounds.center - nodes[node_index].bounds.radius;
        Vec3 node_max = nodes[node_index].bounds.center + nodes[node_index].bounds.radius;

        Vec3 bin_size         = (node_max - node_min) / (float)bin_count_per_dimension;
        Vec3 inverse_bin_size = 1.0f / bin_size;

        for (u32 i = 0; i < index_count; i += 3) {
                // ===== NOTE: This data is needed later, should I store it? ===== //
                Vec3 v0 = vertices[indices[i + 0]].position;
                Vec3 v1 = vertices[indices[i + 1]].position;
                Vec3 v2 = vertices[indices[i + 2]].position;

                Vec3 triangle_center = (v0 + v1 + v2) / 3.0f;

                iVec3 bin_index_per_dimension = glm::min(uVec3((triangle_center - node_min) * inverse_bin_size), bin_count_per_dimension - 1);

                for (u32 dimension = 0; dimension < dimension_count; dimension++) {
                        u32 bin_index_offset = dimension * bin_count_per_dimension;
                        u32 bin_index        = bin_index_offset + bin_index_per_dimension[dimension];

                        bins[bin_index].bounds.Extend(v0);
                        bins[bin_index].bounds.Extend(v1);
                        bins[bin_index].bounds.Extend(v2);

                        bins[bin_index].index_count += 3;
                }
        }

        TriangleBVHNode left_bins[bin_count]{};
        TriangleBVHNode right_bins[bin_count]{};

        for (u32 dimension = 0; dimension < dimension_count; dimension++) {
                left_bins[dimension * bin_count_per_dimension].bounds.radius                                  = { -f32_max, -f32_max, -f32_max };
                right_bins[dimension * bin_count_per_dimension + (bin_count_per_dimension - 1)].bounds.radius = { -f32_max, -f32_max, -f32_max };
        }

        for (u32 dimension = 0; dimension < dimension_count; dimension++) {
                for (u32 dimension_bin_index = 1; dimension_bin_index < bin_count_per_dimension; dimension_bin_index++) {
                        u32 left_bin_index = dimension * bin_count_per_dimension + dimension_bin_index;

                        TriangleBVHNode&       left_bin      = left_bins[left_bin_index];
                        const TriangleBVHNode& prev_left_bin = left_bins[left_bin_index - 1];

                        left_bin = prev_left_bin;

                        left_bin.bounds.Extend(bins[left_bin_index - 1].bounds);
                        left_bin.index_count += bins[left_bin_index - 1].index_count;


                        u32 right_bin_index = dimension * bin_count_per_dimension + ((bin_count_per_dimension - dimension_bin_index) - 1);

                        TriangleBVHNode&       right_bin      = right_bins[right_bin_index];
                        const TriangleBVHNode& prev_right_bin = right_bins[right_bin_index + 1];

                        right_bin = prev_right_bin;

                        right_bin.bounds.Extend(bins[right_bin_index + 1].bounds);
                        right_bin.index_count += bins[right_bin_index + 1].index_count;
                }
        }

        u32   best_dimension = 0;
        u32   best_bin       = 0;
        float best_cost      = FLT_MAX;

        float area_of_parent = nodes[node_index].bounds.Area();

        for (u32 dimension = 0; dimension < dimension_count; dimension++) {
                for (u32 i = 1; i < bin_count_per_dimension; i++) {
                        u32 left_bin_index  = dimension * bin_count_per_dimension + i - 1;
                        u32 right_bin_index = left_bin_index + 1;

                        const TriangleBVHNode& left_bin  = left_bins[left_bin_index];
                        const TriangleBVHNode& right_bin = right_bins[right_bin_index];

                        if (left_bin.index_count == 0 or right_bin.index_count == 0) continue;

                        float cost = CalculateCost(area_of_parent, index_count, left_bin, right_bin);

                        if (cost < best_cost) {
                                best_dimension = dimension;
                                best_bin       = i;
                                best_cost      = cost;
                        }
                }
        }

        float leaf_cost = triangle_intersect_cost * (index_count / 3);
        if (best_cost > leaf_cost) return false;

        Vec3 bin_relative_pos            = { 0, 0, 0 };
        bin_relative_pos[best_dimension] = (float)best_bin * bin_size[best_dimension];
        *split_position                  = node_min + bin_relative_pos;

        return true;
}

void TriangleBVH::Split(VertexDrawData* vertices, u32* indices, u32 index_count, u32 index_offset, u32 node_index) {
        assert(node_index < nodes.size() and "Node index out of range");

        if (index_count <= max_indices_in_node) {
                nodes[node_index].index_count  = index_count;
                nodes[node_index].index_offset = index_offset;
                return;
        }

        Vec3 split_position;
        bool found_split = GetBestSplit(vertices, indices, index_count, node_index, &split_position);

        if (!found_split) {
                nodes[node_index].index_count  = index_count;
                nodes[node_index].index_offset = index_offset;
                return;
        }

        u32 end   = index_count - 3;
        u32 start = 0;
        while (start < end) {
                Vec3 v0 = vertices[indices[start + 0]].position;
                Vec3 v1 = vertices[indices[start + 1]].position;
                Vec3 v2 = vertices[indices[start + 2]].position;

                Vec3 triangle_center = (v0 + v1 + v2) / 3.0f;

                if (glm::all(glm::greaterThanEqual(triangle_center, split_position))) {
                        for (u32 i = 0; i < 3; i++) {
                                u32 temp           = indices[start + i];
                                indices[start + i] = indices[end + i];
                                indices[end + i]   = temp;
                        }

                        end -= 3;
                } else {
                        start += 3;
                }
        }

        TriangleBVHNode left_child{};
        left_child.bounds.radius = { -f32_max, -f32_max, -f32_max };

        for (u32 i = 0; i < start; i++) {
                left_child.bounds.Extend(vertices[indices[i]].position);
        }

        TriangleBVHNode right_child{};
        right_child.bounds.radius = { -f32_max, -f32_max, -f32_max };

        for (u32 i = start; i < index_count; i++) {
                right_child.bounds.Extend(vertices[indices[i]].position);
        }

        nodes[node_index].child_index = (u32)nodes.size();
        nodes.push_back(left_child);
        nodes.push_back(right_child);

        nodes[node_index].index_offset = u32_max;

        Split(vertices, &indices[0], start, index_offset, nodes[node_index].child_index);
        Split(vertices, &indices[start], index_count - start, index_offset + start, nodes[node_index].child_index + 1);
}

float Mesh::Traverse(u32 node_index, Ray ray, float ray_length) {
        const TriangleBVHNode& node = bvh.nodes[node_index];

        if (!RayAABB(ray, node.bounds)) return f32_max;

        if (node.index_offset == u32_max) {
                // not leaf
                u32 l_child = node.child_index;
                u32 r_child = l_child + 1;

                float l_distance = Traverse(l_child, ray, ray_length);
                float r_distance = Traverse(r_child, ray, ray_length);

                return std::min(l_distance, r_distance);
        } else {
                // Leaf

                float closest_intersection = f32_max;

                for (u32 i = 0; i < node.index_count; i += 3) {
                        u32 index_index = node.index_offset + i;

                        Vec3 v0 = vertices[indices[index_index + 0]].position;
                        Vec3 v1 = vertices[indices[index_index + 1]].position;
                        Vec3 v2 = vertices[indices[index_index + 2]].position;

                        float intersection   = RayTriangle(ray, v0, v1, v2);
                        closest_intersection = std::min(closest_intersection, intersection);
                }

                return closest_intersection;
        }
}