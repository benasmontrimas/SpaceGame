#include "Model.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <volk/volk.h>

#include <ktx.h>
#include <ktxvulkan.h>

#include <print>

void Model::LoadFromOBJ(const std::string& file_name, const VmaAllocator& allocator) {
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
                MeshData mesh_data;

                mesh_data.vertices.reserve(shapes[mesh_index].mesh.indices.size());
                mesh_data.indices.reserve(shapes[mesh_index].mesh.indices.size());

                for (tinyobj::index_t& index : shapes[mesh_index].mesh.indices) {
                        Vertex v{ .position = { attrib.vertices[index.vertex_index * 3 + 0], -attrib.vertices[index.vertex_index * 3 + 1],
                                                attrib.vertices[index.vertex_index * 3 + 2] },
                                  .normal   = { attrib.normals[index.normal_index * 3 + 0], -attrib.normals[index.normal_index * 3 + 1],
                                                attrib.normals[index.normal_index * 3 + 2] },
                                  .uv       = { attrib.texcoords[index.texcoord_index * 2 + 0], 1.0 - attrib.texcoords[index.texcoord_index * 2 + 1] } };

                        mesh_data.vertices.push_back(v);
                        mesh_data.indices.push_back((u32)mesh_data.indices.size());
                }

                VkDeviceSize vertex_buffer_size{ sizeof(Vertex) * mesh_data.vertices.size() };
                VkDeviceSize index_buffer_size{ sizeof(u32) * mesh_data.indices.size() };

                VkBufferCreateInfo buffer_info{ .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                                .size  = vertex_buffer_size + index_buffer_size,
                                                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT };

                VmaAllocationCreateInfo vertex_buffer_allocation_create_info{
                        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
                        .usage = VMA_MEMORY_USAGE_AUTO,
                };

                Mesh mesh{};

                VkResult res = vmaCreateBuffer(allocator, &buffer_info, &vertex_buffer_allocation_create_info, &mesh.buffer, &mesh.buffer_allocation,
                                               &mesh.buffer_allocation_info);

                if (res != VK_SUCCESS) {
                        std::println("Failed to create vertex buffer");
                        exit(1);
                }

                memcpy(mesh.buffer_allocation_info.pMappedData, mesh_data.vertices.data(), vertex_buffer_size);
                memcpy(((char*)mesh.buffer_allocation_info.pMappedData) + vertex_buffer_size, mesh_data.indices.data(), index_buffer_size);

                mesh.index_count   = (u64)mesh_data.indices.size();
                mesh.vertices_size = (u64)vertex_buffer_size;

                meshes.push_back(mesh);
        }
}

void Model::Destroy(const VmaAllocator& allocator) {
        for (auto& mesh : meshes) {
                vmaDestroyBuffer(allocator, mesh.buffer, mesh.buffer_allocation);
                // DestroyGPUBuffer(vulkan_device, planet_density_buffer, vulkan_allocator);
        }
}

void Texture::Load(VkDevice device, VkCommandPool command_pool, VkQueue queue, const std::string& file_name, const VmaAllocator& allocator) {
        VkResult res;

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
