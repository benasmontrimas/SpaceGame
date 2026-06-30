#include "Model.h"

#include "Game.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <volk/volk.h>

#include <ktx.h>
#include <ktxvulkan.h>

#include <print>

void Model::Init(GameContext& game_context, VertexDrawData* vertices, u64 vertex_count, u32* indices, u64 index_count, u32 _max_instance_count) {
        assert(meshes.size() == 0 and "Initialising a model which is already initialised");

        Mesh mesh{};

        // ===== Allocate Buffer ===== //

        VkDeviceSize vertex_buffer_size = sizeof(VertexDrawData) * vertex_count;
        VkDeviceSize index_buffer_size  = sizeof(u32) * index_count;
        VkDeviceSize buffer_size        = vertex_buffer_size + index_buffer_size;

        VmaAllocationCreateFlags allocation_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        mesh.buffer = CreateGPUBuffer(game_context.vulkan_device, game_context.vulkan_allocator, buffer_size, game_context.graphics_queue_family,
                                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, allocation_flags);

        // ===== Copy Vertex and Index Data ===== //

        memcpy(mesh.buffer.allocation_info.pMappedData, vertices, vertex_buffer_size);
        memcpy(((char*)mesh.buffer.allocation_info.pMappedData) + vertex_buffer_size, indices, index_buffer_size);

        mesh.index_count   = index_count;
        mesh.vertices_size = (u64)vertex_buffer_size;

        meshes.push_back(mesh);

        // ===== Allocate Instance Buffer ===== //

        max_instance_count = _max_instance_count;

        u64 instance_buffer_size = sizeof(InstanceDrawData) * max_instance_count * max_frames_in_flight;

        instance_buffer = CreateGPUBuffer(game_context.vulkan_device, game_context.vulkan_allocator, instance_buffer_size, game_context.graphics_queue_family,
                                          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, allocation_flags);
}

void Model::LoadFromOBJ(GameContext& game_context, const std::string& file_name, u32 _max_instance_count) {
        tinyobj::attrib_t                attrib;
        std::vector<tinyobj::shape_t>    shapes;
        std::vector<tinyobj::material_t> materials;

        bool load_ok = tinyobj::LoadObj(&attrib, &shapes, &materials, nullptr, file_name.c_str());

        if (!load_ok) {
                std::println("Failed loading mesh: {}", file_name);
                exit(1);
        }

        std::println("Loaded model: {}, with {} meshes", file_name, shapes.size());

        for (size_t mesh_index = 0; mesh_index < shapes.size(); mesh_index++) {
                std::vector<VertexDrawData> vertices;
                std::vector<u32>    indices;

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

                Mesh mesh{};

                // ===== Allocate Vertex and Index Buffer ===== //

                VkDeviceSize vertex_buffer_size{ sizeof(VertexDrawData) * vertices.size() };
                VkDeviceSize index_buffer_size{ sizeof(u32) * indices.size() };
                VkDeviceSize buffer_size{ vertex_buffer_size + index_buffer_size };

                VmaAllocationCreateFlags allocation_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                                            VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

                mesh.buffer = CreateGPUBuffer(game_context.vulkan_device, game_context.vulkan_allocator, buffer_size, game_context.graphics_queue_family,
                                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, allocation_flags);

                // ===== Copy Vertex and Index Data ===== //

                memcpy(mesh.buffer.allocation_info.pMappedData, vertices.data(), vertex_buffer_size);
                memcpy(((char*)mesh.buffer.allocation_info.pMappedData) + vertex_buffer_size, indices.data(), index_buffer_size);

                mesh.index_count   = (u64)indices.size();
                mesh.vertices_size = (u64)vertex_buffer_size;

                meshes.push_back(mesh);
        }

        // ===== Allocate Instance Buffer ===== //

        VmaAllocationCreateFlags allocation_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        max_instance_count = _max_instance_count;

        u64 instance_buffer_size = sizeof(InstanceDrawData) * max_instance_count * max_frames_in_flight;

        instance_buffer = CreateGPUBuffer(game_context.vulkan_device, game_context.vulkan_allocator, instance_buffer_size, game_context.graphics_queue_family,
                                          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, allocation_flags);
}

void Model::Destroy(GameContext& game_context) {
        for (auto& mesh : meshes) {
                DestroyGPUBuffer(game_context.vulkan_device, mesh.buffer, game_context.vulkan_allocator);
        }

        DestroyGPUBuffer(game_context.vulkan_device, instance_buffer, game_context.vulkan_allocator);
}

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

        VmaAllocationCreateInfo texture_image_allocation_info{ .usage = VMA_MEMORY_USAGE_AUTO };

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

        VkBuffer      image_src_buffer{};
        VmaAllocation image_src_allocation{};

        VkBufferCreateInfo image_src_buffer_info{
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size  = (u32)ktx_texture->dataSize,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        };

        VmaAllocationCreateInfo image_src_allocation_create_info{
                .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                .usage = VMA_MEMORY_USAGE_AUTO,
        };

        VmaAllocationInfo image_src_allocation_info{};

        res = vmaCreateBuffer(allocator, &image_src_buffer_info, &image_src_allocation_create_info, &image_src_buffer, &image_src_allocation,
                              &image_src_allocation_info);

        if (res != VK_SUCCESS) {
                std::println("Failed creating buffer for image upload");
                exit(res);
        }

        memcpy(image_src_allocation_info.pMappedData, ktx_texture->pData, ktx_texture->dataSize);

        VkFenceCreateInfo fence_info{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };

        VkFence fence;
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

        std::vector<VkBufferImageCopy> copy_regions(ktx_texture->numLevels);

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
                .maxAnisotropy    = 8.0f,
                .maxLod           = (float)ktx_texture->numLevels,
        };

        res = vkCreateSampler(device, &sampler_info, nullptr, &sampler);

        if (res != VK_SUCCESS) {
                std::println("Failed creating sampler");
                exit(res);
        }

        ktxTexture_Destroy(ktx_texture);
}

void Texture::Destroy(GameContext& game_context) {
        // Destroy Sampler?
        vkDestroySampler(game_context.vulkan_device, sampler, nullptr);

        // Destroy Image View
        vkDestroyImageView(game_context.vulkan_device, image_view, nullptr);

        // Destroy Image
        vmaDestroyImage(game_context.vulkan_allocator, image, allocation);
}

// ===== TEXT RENDERING ===== //

void TextSystem::Init(GameContext& game_context) {
        // ===== Initialise Letter Quad ===== //

        constexpr u32 index_count  = 6;
        constexpr u32 vertex_count = 4;

        // Counter clock wise
        Vec3 normal{ 0.0f, 0.0f, -1.0f };

        constexpr float texture_row_size   = 512.0f;
        constexpr float letter_row_size    = 64.0f;
        constexpr float letters_in_row     = texture_row_size / letter_row_size;
        constexpr float texture_coord_size = 1 / letters_in_row;

        // bottom left
        VertexDrawData v0{
                .position = { 0.0f, 0.0f, 0.0f },
                .normal   = normal,
                .uv       = { 0.0f, 0.0f },
        };

        // bottom right
        VertexDrawData v1 {
                .position = { 1.0f, 0.0f, 0.0f }, .normal = normal,
                .uv = {
                        texture_coord_size,
                        0.0,
                },
        };

        // top right
        VertexDrawData v2{
                .position = { 1.0f, 1.0f, 0.0f },
                .normal   = normal,
                .uv       = { texture_coord_size, texture_coord_size },
        };

        // top left
        VertexDrawData v3{
                .position = { 0.0f, 1.0f, 0.0f },
                .normal   = normal,
                .uv       = { 0.0f, texture_coord_size },
        };

        VertexDrawData vertices[vertex_count] = { v0, v1, v2, v3 };
        u32    indices[index_count]   = { 0, 1, 2, 0, 2, 3 };

        model.Init(game_context, &vertices[0], vertex_count, &indices[0], index_count, 100'000);

        // ===== Initialise Instance Buffer ===== //

        // constexpr u64 buffer_size = frame_buffer_size * max_frames_in_flight;

        // VmaAllocationCreateFlags allocation_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        // instance_data = CreateGPUBuffer(game_context.vulkan_device, game_context.vulkan_allocator, buffer_size, game_context.graphics_queue_family,
        //                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, allocation_flags);

        // ===== Initialise Next Frame Data Array ===== //

        // next_frame_draw_data = (InstanceDrawData*)malloc(frame_buffer_size);

        // ===== Initialise Letter Count ===== //

        letter_count = 0;
}

// Maybe just collect and write when we render. Easier to track as well.
void TextSystem::AddText(Transform* text_data, u32 text_letter_count) {
        model.instances.resize(letter_count + text_letter_count);

        u64 write_offset = letter_count * sizeof(Transform);
        u64 write_size   = text_letter_count * sizeof(Transform);
        memcpy((&model.instances[letter_count]), text_data, write_size);

        letter_count += text_letter_count;
}

void TextSystem::Draw(VkCommandBuffer command_buffer) {
        if (letter_count == 0) return;

        // ===== Write Data ===== //

        // Ideally use transfer queue to transfer next_frame_draw_data to instance_buffer at offset
        u64 write_offset = frame_index * frame_buffer_size;
        u64 write_size   = letter_count * sizeof(InstanceDrawData);
        memcpy(((u8*)instance_data.allocation_info.pMappedData) + write_offset, next_frame_draw_data, write_size);

        // ===== Vulkan Draw Command ===== //

        VkDeviceSize vertex_offset[2] = { 0, 0 };
        VkBuffer     buffers[2]       = { model.meshes[0].buffer.buffer, model.instance_buffer.buffer };
        vkCmdBindVertexBuffers(command_buffer, 0, 1, &buffers[0], &vertex_offset[0]);

        u32 index_offset = model.meshes[0].vertices_size;
        vkCmdBindIndexBuffer(command_buffer, model.meshes[0].buffer.buffer, index_offset, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(command_buffer, (u32)model.meshes[0].index_count, letter_count, 0, 0, 0);

        // ===== Reset for next frame ===== //

        letter_count = 0;
        frame_index  = (frame_index + 1) % max_frames_in_flight;
}

void RenderedText::SetText(const std::string& new_text) {
        text         = new_text;
        u32 text_len = text.length();

        letter_data.clear();
        letter_data.reserve(text_len); // Might not need all that space

        float x_offset = 0;
        float y_offset = 0;

        constexpr float space_offset = 32.0f;

        for (u32 i = 0; i < text_len; i++) {
                char letter = text[i];

                if (letter >= 'a' and letter <= 'z') letter += 'A' - 'a';

                if (letter >= 'A' and letter <= 'Z') {
                        // TODO: Get actual offset - FreeType Library
                        Transform2D transform{};

                        transform.position = { x_offset, y_offset };
                        x_offset += 64;

                        letter_data.emplace_back(letter, transform, default_colour);
                } else {
                        switch (letter) {
                                case ' ':
                                        x_offset += space_offset;
                                        break;
                                case '\n':
                                        x_offset = 0;
                                        y_offset += 64;
                                        break;
                                case '\t':
                                        x_offset += 4 * space_offset;
                                        break;

                                default:
                                        std::println("[WARN] Character is not supported, skipping: {}", letter);
                        }
                }
        }
}

void RenderedText::Draw(TextSystem& text_system) {
        std::vector<Transform> letter_draw_data;
        letter_draw_data.resize(letter_data.size());

        // NOTE: No rotation yet
        Mat4 text_mat = glm::translate(Mat4(1.0f), transform.position);
        text_mat      = glm::scale(text_mat, transform.scale);

        for (u32 i = 0; i < letter_data.size(); i++) {
                Vec3 position = {
                        transform.position.x + letter_data[i].transform.position.x,
                        transform.position.y + letter_data[i].transform.position.y,
                        transform.position.z,
                };

                Vec3 scale = {
                        transform.scale.x + letter_data[i].transform.scale.x,
                        transform.scale.y + letter_data[i].transform.scale.y,
                        transform.scale.z,
                };

                Mat4 letter_mat = glm::translate(Mat4(1.0f), position);
                letter_mat      = glm::scale(letter_mat, scale);

                letter_draw_data[i].position = position;
                letter_draw_data[i].rotation = Quat{};
                letter_draw_data[i].scale    = scale;
        }

        text_system.AddText(letter_draw_data.data(), letter_draw_data.size());
}
