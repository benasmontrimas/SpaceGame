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

        meshes     = (Mesh*)malloc(sizeof(Mesh));
        mesh_count = 1;

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

        meshes     = (Mesh*)malloc(sizeof(Mesh));
        mesh_count = 1;

        // ===== Allocate Buffer ===== //

        VkDeviceSize vertex_buffer_size = sizeof(VertexDrawData) * vertex_count;
        VkDeviceSize index_buffer_size  = sizeof(u32) * index_count;
        VkDeviceSize buffer_size        = vertex_buffer_size + index_buffer_size;

        VmaAllocationCreateFlags allocation_flags = 0;

        meshes[0].buffer = CreateGPUBuffer(game_context->vulkan_device, game_context->vulkan_allocator, buffer_size, game_context->graphics_queue_family,
                                           VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
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

        std::println("Loaded model: {}, with {} meshes", file_name, shapes.size());

        mesh_count = shapes.size();
        meshes     = (Mesh*)malloc(sizeof(Mesh) * mesh_count);

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

                VmaAllocationCreateFlags allocation_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                                            VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

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
        }

        DestroyGPUBuffer(game_context->vulkan_device, instance_buffer, game_context->vulkan_allocator);

        free(meshes);
        free(instance_draw_data);
}

void Model::Draw(VkCommandBuffer command_buffer, VkPipelineLayout pipeline_layout, u32 frame_index) {
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
                const Mesh& mesh = meshes[mesh_index];
                assert(mesh.index_count != 0 and "Empty mesh is an error");

                // == Bind Vertex + Index Buffers == //

                VkDeviceSize vertex_offset[2] = { 0, instance_buffer_offset };
                VkBuffer     buffers[2]       = { mesh.buffer.buffer, instance_buffer.buffer };
                vkCmdBindVertexBuffers(command_buffer, 0, 2, &buffers[0], &vertex_offset[0]);

                u32 index_offset = mesh.vertices_size;
                vkCmdBindIndexBuffer(command_buffer, mesh.buffer.buffer, index_offset, VK_INDEX_TYPE_UINT32);

                // == Draw == //

                vkCmdDrawIndexed(command_buffer, mesh.index_count, instance_count, 0, 0, 0);
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

void Texture::Destroy(GameContext* game_context) {
        // Destroy Sampler?
        vkDestroySampler(game_context->vulkan_device, sampler, nullptr);

        // Destroy Image View
        vkDestroyImageView(game_context->vulkan_device, image_view, nullptr);

        // Destroy Image
        vmaDestroyImage(game_context->vulkan_allocator, image, allocation);
}

// ===== TEXT RENDERING ===== //

void TextSystem::Init(GameContext* _game_context) {
        game_context = _game_context;
        // ===== Initialise Letter Quad ===== //

        constexpr u32 index_count  = 6;
        constexpr u32 vertex_count = 4;

        // Counter clock wise
        Vec3 normal{ 0.0f, 0.0f, -1.0f };

        constexpr float texture_row_size   = 512.0f;
        constexpr float letter_row_size    = 64.0f;
        constexpr float letters_in_row     = texture_row_size / letter_row_size;
        // constexpr float texture_coord_size = 1.0f / letters_in_row;
        constexpr float texture_coord_size = 1.0f;

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
        u32            indices[index_count]   = { 0, 1, 2, 0, 2, 3 };

        model_id = game_context->model_system.CreateModel(&vertices[0], vertex_count, &indices[0], index_count, max_letter_count);

        Material text_material{
                .type = MaterialType::Text,
                .text = { .texture_start = (99) },
        };

        game_context->model_system[model_id].material = text_material;

        // ===== Textures ===== //

        title_font_atlas.Load(game_context->vulkan_device, game_context->graphics_command_pool, game_context->graphics_queue, "Assets/Fonts/TitleAtlas.ktx",
                              game_context->vulkan_allocator);

        game_context->AddTexture(title_font_atlas, 99);

        default_font_atlas.Load(game_context->vulkan_device, game_context->graphics_command_pool, game_context->graphics_queue, "Assets/Fonts/DefaultAtlas.ktx",
                                game_context->vulkan_allocator);

        game_context->AddTexture(default_font_atlas, 98);

        // ===== Init Free Type ===== //

        FT_Error error_code = FT_Init_FreeType(&ft_library);
        if (error_code != 0) {
                std::println("Failed initialising FreeType: {}", error_code);
                exit(1);
        }

        FT_New_Face(ft_library, "Assets/Fonts/Aboreto_Regular.2.ttf", 0, &title_font);
        FT_New_Face(ft_library, "Assets/Fonts/AdwaitaSans-Regular.ttf", 0, &default_font);
}

void TextSystem::Shutdown() {
        title_font_atlas.Destroy(game_context);
        default_font_atlas.Destroy(game_context);

        FT_Done_Face(title_font);
        FT_Done_Face(default_font);
        FT_Done_FreeType(ft_library);
}

// Maybe just collect and write when we render. Easier to track as well.
void TextSystem::AddText(ModelInstance* model_instance, u32 text_letter_count) {
        for (u32 i = 0; i < text_letter_count; i++) {
                game_context->model_system.Draw(model_instance[i]);
        }
}

void RenderedText::SetText(TextSystem* text_system, const std::string& new_text) {
        // ===== TEMP ===== //

        transform.position = { -1, -1, 0 };
        float text_size    = 512.0f;

        FT_F26Dot6 char_width  = u32(text_size) << 6;
        FT_F26Dot6 char_height = u32(text_size) << 6;

        FT_Set_Char_Size(text_system->title_font, char_width, char_height, 0, 0);

        // == //

        text         = new_text;
        u32 text_len = text.length();

        letter_data.clear();
        letter_data.reserve(text_len); // Might not need all that space

        float x_offset = 0;
        float y_offset = 0;

        float window_width  = (float)text_system->game_context->window.width;
        float window_height = (float)text_system->game_context->window.height;

        float space_offset    = 32.0f / window_width;
        float char_offset     = text_size / window_width;
        float new_line_offset = text_size / window_height;

        char prev_letter = 0;
        for (u32 i = 0; i < text_len; i++) {
                char letter = text[i];

                if (letter >= 'a' and letter <= 'z') letter += 'A' - 'a';

                if (letter >= 'A' and letter <= 'Z' or letter == ' ') {
                        FT_Load_Char(text_system->title_font, letter, FT_LOAD_DEFAULT);
                        FT_Glyph_Metrics glyph_metrics = text_system->title_font->glyph->metrics;

                        Transform2D transform{};

                        if (i > 0) {
                                FT_Vector kerning;
                                FT_Get_Kerning(text_system->title_font, letter, prev_letter, FT_KERNING_DEFAULT, &kerning);

                                x_offset += (kerning.x >> 6) / window_width;
                                // transform.position
                        }

                        transform.position = { x_offset, y_offset + (glyph_metrics.horiBearingY >> 6) / window_height };
                        x_offset += (glyph_metrics.horiAdvance >> 6) / window_width;

                        transform.scale = {
                                text_size / window_width,
                                text_size / window_height,
                        };

                        std::println("Advance: {}", glyph_metrics.horiAdvance >> 6);

                        letter_data.emplace_back(letter, transform, default_colour);
                } else {
                        switch (letter) {
                                case ' ':
                                        x_offset += space_offset;
                                        break;
                                case '\n':
                                        x_offset = 0;
                                        y_offset += new_line_offset;
                                        break;
                                case '\t':
                                        x_offset += 4 * space_offset;
                                        break;

                                default:
                                        std::println("[WARN] Character is not supported, skipping: {}", letter);
                        }
                }

                prev_letter = letter;
        }
}

void RenderedText::Draw(TextSystem* text_system) {
        std::vector<ModelInstance> letter_draw_data;
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
                        transform.scale.x * letter_data[i].transform.scale.x,
                        transform.scale.y * letter_data[i].transform.scale.y,
                        transform.scale.z,
                };

                Mat4 letter_mat = glm::translate(Mat4(1.0f), position);
                letter_mat      = glm::scale(letter_mat, scale);


                letter_draw_data[i].model_id = text_system->model_id;

                letter_draw_data[i].transform.position = position;
                letter_draw_data[i].transform.rotation = Quat{};
                letter_draw_data[i].transform.scale    = scale;

                letter_draw_data[i].user_value = letter_data[i].letter - 'A';
        }

        text_system->AddText(letter_draw_data.data(), letter_draw_data.size());
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

ModelID ModelSystem::LoadModel(const std::string& file_path, u32 max_instance_count) {
        ModelID id = GetNewModelID();

        Model& model = models[id].model;
        model        = {};
        model.LoadFromOBJ(game_context, file_path, max_instance_count);

        return id;
}

ModelID ModelSystem::CreateModel(VertexDrawData* vertices, u64 vertex_count, u32* indices, u64 index_count, u32 _max_instance_count) {
        ModelID id = GetNewModelID();

        Model& model = models[id].model;
        model        = {};
        model.Init(game_context, vertices, vertex_count, indices, index_count, _max_instance_count);

        return id;
}

ModelID ModelSystem::CreateModel(u64 vertex_count, u64 index_count, u32 _max_instance_count) {
        ModelID id = GetNewModelID();

        Model& model = models[id].model;
        model        = {};
        model.Init(game_context, vertex_count, index_count, _max_instance_count);

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

        Mat4 model_matrix = glm::translate(Mat4(1.0f), instance.transform.position);
        model_matrix      = glm::scale(model_matrix, instance.transform.scale);

        model.instance_draw_data[model.instance_count] = { model_matrix, instance.user_value };
        model.instance_count++;

        model.last_frame_rendered = game_context->frames_submitted + 2;
}

void ModelSystem::Draw(VkCommandBuffer command_buffer, u32 frame_index) {
        u32 free_index = next_free_index;
        for (u32 model_index = 0; model_index < models.size(); model_index++) {
                // Skip free slots
                if (model_index == free_index) {
                        free_index = models[free_index].next_free_index;
                        continue;
                }

                models[model_index].model.Draw(command_buffer, game_context->pipeline_layout, frame_index);
        }
}