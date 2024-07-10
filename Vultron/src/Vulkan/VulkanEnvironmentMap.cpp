#include "Vultron/Vulkan/VulkanEnvironmentMap.h"

#include "Vultron/Vulkan/VulkanMaterial.h"
#include "Vultron/Vulkan/VulkanRenderPass.h"
#include "Vultron/Vulkan/VulkanShader.h"
#include "Vultron/Vulkan/VulkanUtils.h"
#include "Vultron/Vulkan/VulkanInitializers.h"

#include "vulkan/vulkan.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include <chrono>
#include <iostream>

namespace Vultron
{
    VulkanEnvironmentMap VulkanEnvironmentMap::CreateFromFile(const VulkanContext &context, VkCommandPool commandPool, VkDescriptorPool descriptorPool, const VulkanMesh &skyboxMesh, VkDescriptorSetLayout environmentLayout, VkDescriptorSetLayout skyboxLayout, VkSampler sampler, const EnvironmentMapCreateInfo &info)
    {
        auto image = VulkanImage::CreateFromFile(
            context,
            commandPool,
            {
                .filepath = info.filepath,
                .imageTransitionQueue = info.imageTransitionQueue,
            });

        // VulkanImage irradiance = VulkanEnvironmentMap::GenerateIrradianceMap(context, commandPool, descriptorPool, skyboxMesh, image);
        // VulkanImage prefiltered = VulkanEnvironmentMap::GeneratePrefilteredMap(context, commandPool, descriptorPool, skyboxMesh, image);

        VulkanEnvironmentMap environmentMap(image, image, image);
        if (!environmentMap.InitializeDescriptorSets(context, descriptorPool, environmentLayout, skyboxLayout, sampler))
        {
            std::cerr << "Failed to initialize descriptor sets for environment map" << std::endl;
            assert(false);
        }

        return environmentMap;
    }

    bool VulkanEnvironmentMap::InitializeDescriptorSets(const VulkanContext &context, VkDescriptorPool descriptorPool, VkDescriptorSetLayout environmentLayout, VkDescriptorSetLayout skyboxLayout, VkSampler sampler)
    {
        m_environmentSet = VkInit::CreateDescriptorSet(
            context.GetDevice(), descriptorPool, environmentLayout,
            {
                {
                    .binding = 0,
                    .type = DescriptorType::CombinedImageSampler,
                    .imageView = m_irradiance.GetImageView(),
                    .sampler = sampler,
                },
                {
                    .binding = 1,
                    .type = DescriptorType::CombinedImageSampler,
                    .imageView = m_prefiltered.GetImageView(),
                    .sampler = sampler,
                },
            });

        m_skyboxSet = VkInit::CreateDescriptorSet(
            context.GetDevice(), descriptorPool, skyboxLayout,
            {
                {
                    .binding = 0,
                    .type = DescriptorType::CombinedImageSampler,
                    .imageView = m_image.GetImageView(),
                    .sampler = sampler,
                },
            });

        return true;
    }

    void VulkanEnvironmentMap::Destroy(const VulkanContext &context)
    {
        m_image.Destroy(context);
        // m_irradiance.Destroy(context);
        // m_prefiltered.Destroy(context);
    }

    VulkanImage VulkanEnvironmentMap::GenerateIrradianceMap(const VulkanContext &context, VkCommandPool commandPool, VkDescriptorPool descriptorPool, const VulkanMesh &skyboxMesh, const VulkanImage &environmentMap)
    {
        auto tStart = std::chrono::high_resolution_clock::now();

        const VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
        const int32_t dim = 64;
        const uint32_t numMips = static_cast<uint32_t>(glm::floor(glm::log2(static_cast<float>(dim)))) + 1;

        // Sampler
        VkSamplerCreateInfo samplerCI{};
        samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCI.magFilter = VK_FILTER_LINEAR;
        samplerCI.minFilter = VK_FILTER_LINEAR;
        samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCI.minLod = 0.0f;
        samplerCI.maxLod = static_cast<float>(numMips);
        samplerCI.maxAnisotropy = 1.0f;
        samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

        VkSampler sampler;
        VK_CHECK(vkCreateSampler(context.GetDevice(), &samplerCI, nullptr, &sampler));

        // Image
        VulkanImage image = VulkanImage::Create(
            context,
            {
                .info = {
                    .width = dim,
                    .height = dim,
                    .depth = 1,
                    .mipLevels = numMips,
                    .format = format,
                    .layers = 6,
                },
                .type = ImageType::Cubemap,
                .aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
                .additionalUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            });

        // Framebuffer, render pass and pipeline
        VulkanRenderPass renderPass = VulkanRenderPass::Create(
            context,
            {
                .attachments = {
                    {
                        .type = VulkanRenderPass::AttachmentType::Color,
                        .format = format,
                        .samples = VK_SAMPLE_COUNT_1_BIT,
                        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    },
                },
                .dependencies = {
                    {
                        .srcSubpass = VK_SUBPASS_EXTERNAL,
                        .dstSubpass = 0,
                        .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
                        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
                    },
                    {
                        .srcSubpass = 0,
                        .dstSubpass = VK_SUBPASS_EXTERNAL,
                        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
                        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
                    },
                },
            });

        VulkanImage offscreenImage = VulkanImage::Create(
            context,
            {
                .info = {
                    .width = dim,
                    .height = dim,
                    .depth = 1,
                    .mipLevels = 1,
                    .format = format,
                },
                .aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
                .additionalUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            });

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass.GetRenderPass();
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &offscreenImage.GetImageView();
        framebufferInfo.width = dim;
        framebufferInfo.height = dim;
        framebufferInfo.layers = 1;

        VkFramebuffer framebuffer;
        VK_CHECK(vkCreateFramebuffer(context.GetDevice(), &framebufferInfo, nullptr, &framebuffer));

        offscreenImage.TransitionLayout(
            context.GetDevice(),
            commandPool,
            context.GetTransferQueue(),
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        VkDescriptorSetLayout descriptorSetLayout = VkInit::CreateDescriptorSetLayout(
            context.GetDevice(),
            {
                {
                    .binding = 0,
                    .type = DescriptorType::CombinedImageSampler,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
            });
        VkDescriptorSet descriptorSet = VkInit::CreateDescriptorSet(
            context.GetDevice(), descriptorPool, descriptorSetLayout,
            {
                {
                    .binding = 0,
                    .type = DescriptorType::CombinedImageSampler,
                    .imageView = environmentMap.GetImageView(),
                    .sampler = sampler,
                },
            });

        // Pipeline layout
        struct PushBlock
        {
            glm::mat4 mvp;
            float deltaPhi = (2.0f * glm::pi<float>()) / 180.0f;
            float deltaTheta = (0.5f * glm::pi<float>()) / 64.0f;
        } pushBlock;

        VulkanShader vertexShader = VulkanShader::CreateFromFile(context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/filtercube.vert.spv"});
        VulkanShader fragmentShader = VulkanShader::CreateFromFile(context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/irradiance.frag.spv"});
        VulkanMaterialPipeline pipeline = VulkanMaterialPipeline::Create(
            context, renderPass,
            {
                .vertexShader = vertexShader,
                .fragmentShader = fragmentShader,
                .descriptorSetLayouts = {descriptorSetLayout},
                .bindings = {},
                .pushConstantRanges = {
                    {
                        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        .offset = 0,
                        .size = sizeof(PushBlock),
                    },
                },
                .vertexDescription = StaticMeshVertex::GetVertexDescription(),
                .cullMode = CullMode::None,
                .depthFunction = DepthFunction::LessOrEqual,
            });

        // Render
        VkClearValue clearValues[1];
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};

        VkRenderPassBeginInfo renderPassBeginInfo{};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.renderPass = renderPass.GetRenderPass();
        renderPassBeginInfo.renderArea.extent.width = dim;
        renderPassBeginInfo.renderArea.extent.height = dim;
        renderPassBeginInfo.clearValueCount = 1;
        renderPassBeginInfo.pClearValues = clearValues;
        renderPassBeginInfo.framebuffer = framebuffer;

        std::array<glm::mat4, 6> captureViews = {
            glm::lookAt(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        };

        VkCommandBuffer commandBuffer = VkUtil::BeginSingleTimeCommands(context.GetDevice(), commandPool);

        VkViewport viewport = {
            .x = 0.0f,
            .y = 0.0f,
            .width = (float)dim,
            .height = (float)dim,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };

        VkRect2D scissor = {
            .offset = {0, 0},
            .extent = {dim, dim},
        };

        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        image.TransitionLayout(
            context.GetDevice(),
            commandBuffer,
            context.GetTransferQueue(),
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        for (uint32_t mip = 0; mip < numMips; mip++)
        {
            for (uint32_t face = 0; face < 6; face++)
            {
                viewport.width = static_cast<float>(dim * std::pow(0.5f, mip));
                viewport.height = static_cast<float>(dim * std::pow(0.5f, mip));
                vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

                vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

                pushBlock.mvp = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 512.0f) * glm::mat4(glm::mat3(captureViews[face]));
                vkCmdPushConstants(commandBuffer, pipeline.GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushBlock), &pushBlock);

                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetPipeline());
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

                MeshDrawInfo meshInfo = skyboxMesh.GetDrawInfo();
                VkBuffer vertexBuffers[] = {meshInfo.vertexBuffer};
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
                vkCmdBindIndexBuffer(commandBuffer, meshInfo.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

                vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(meshInfo.indexCount), 1, 0, 0, 0);

                vkCmdEndRenderPass(commandBuffer);

                offscreenImage.TransitionLayout(
                    context.GetDevice(),
                    commandBuffer,
                    context.GetTransferQueue(),
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

                VkImageCopy copyRegion{};
                copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.srcSubresource.baseArrayLayer = 0;
                copyRegion.srcSubresource.mipLevel = 0;
                copyRegion.srcSubresource.layerCount = 1;
                copyRegion.srcOffset = {0, 0, 0};

                copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.dstSubresource.baseArrayLayer = face;
                copyRegion.dstSubresource.mipLevel = mip;
                copyRegion.dstSubresource.layerCount = 1;
                copyRegion.dstOffset = {0, 0, 0};

                copyRegion.extent = {static_cast<uint32_t>(viewport.width), static_cast<uint32_t>(viewport.height), 1};

                vkCmdCopyImage(commandBuffer, offscreenImage.GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image.GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

                offscreenImage.TransitionLayout(
                    context.GetDevice(),
                    commandBuffer,
                    context.GetTransferQueue(),
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            }
        }

        image.TransitionLayout(
            context.GetDevice(),
            commandBuffer,
            context.GetTransferQueue(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        VkUtil::EndSingleTimeCommands(context.GetDevice(), commandPool, context.GetTransferQueue(), commandBuffer);

        vkDestroySampler(context.GetDevice(), sampler, nullptr);
        offscreenImage.Destroy(context);
        vertexShader.Destroy(context);
        fragmentShader.Destroy(context);
        vkDestroyFramebuffer(context.GetDevice(), framebuffer, nullptr);
        vkDestroyDescriptorSetLayout(context.GetDevice(), descriptorSetLayout, nullptr);
        pipeline.Destroy(context);
        renderPass.Destroy(context);

        auto tEnd = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart).count();
        std::cout << "Irradiance map generation took " << duration << "ms" << std::endl;

        return image;
    }

    VulkanImage VulkanEnvironmentMap::GeneratePrefilteredMap(const VulkanContext &context, VkCommandPool commandPool, VkDescriptorPool descriptorPool, const VulkanMesh &skyboxMesh, const VulkanImage &environmentMap)
    {
        auto tStart = std::chrono::high_resolution_clock::now();

        const VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
        const int32_t dim = 512;
        const uint32_t numMips = static_cast<uint32_t>(glm::floor(glm::log2(static_cast<float>(dim)))) + 1;

        // Sampler
        VkSamplerCreateInfo samplerCI{};
        samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCI.magFilter = VK_FILTER_LINEAR;
        samplerCI.minFilter = VK_FILTER_LINEAR;
        samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCI.minLod = 0.0f;
        samplerCI.maxLod = static_cast<float>(numMips);
        samplerCI.maxAnisotropy = 1.0f;
        samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

        VkSampler sampler;
        VK_CHECK(vkCreateSampler(context.GetDevice(), &samplerCI, nullptr, &sampler));

        // Image
        VulkanImage image = VulkanImage::Create(
            context,
            {
                .info = {
                    .width = dim,
                    .height = dim,
                    .depth = 1,
                    .mipLevels = numMips,
                    .format = format,
                    .layers = 6,
                },
                .type = ImageType::Cubemap,
                .aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
                .additionalUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            });

        // Framebuffer, render pass and pipeline
        VulkanRenderPass renderPass = VulkanRenderPass::Create(
            context,
            {
                .attachments = {
                    {
                        .type = VulkanRenderPass::AttachmentType::Color,
                        .format = format,
                        .samples = VK_SAMPLE_COUNT_1_BIT,
                        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    },
                },
                .dependencies = {
                    {
                        .srcSubpass = VK_SUBPASS_EXTERNAL,
                        .dstSubpass = 0,
                        .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
                        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
                    },
                    {
                        .srcSubpass = 0,
                        .dstSubpass = VK_SUBPASS_EXTERNAL,
                        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
                        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
                    },
                },
            });

        VulkanImage offscreenImage = VulkanImage::Create(
            context,
            {
                .info = {
                    .width = dim,
                    .height = dim,
                    .depth = 1,
                    .mipLevels = 1,
                    .format = format,
                },
                .aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
                .additionalUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            });

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass.GetRenderPass();
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &offscreenImage.GetImageView();
        framebufferInfo.width = dim;
        framebufferInfo.height = dim;
        framebufferInfo.layers = 1;

        VkFramebuffer framebuffer;
        VK_CHECK(vkCreateFramebuffer(context.GetDevice(), &framebufferInfo, nullptr, &framebuffer));

        offscreenImage.TransitionLayout(
            context.GetDevice(),
            commandPool,
            context.GetTransferQueue(),
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        VkDescriptorSetLayout descriptorSetLayout = VkInit::CreateDescriptorSetLayout(
            context.GetDevice(),
            {
                {
                    .binding = 0,
                    .type = DescriptorType::CombinedImageSampler,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
            });
        VkDescriptorSet descriptorSet = VkInit::CreateDescriptorSet(
            context.GetDevice(), descriptorPool, descriptorSetLayout,
            {
                {
                    .binding = 0,
                    .type = DescriptorType::CombinedImageSampler,
                    .imageView = environmentMap.GetImageView(),
                    .sampler = sampler,
                },
            });

        // Pipeline layout
        struct PushBlock
        {
            glm::mat4 mvp;
            float roughness;
            uint32_t numSamples = 32u;
        } pushBlock;

        VulkanShader vertexShader = VulkanShader::CreateFromFile(context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/filtercube.vert.spv"});
        VulkanShader fragmentShader = VulkanShader::CreateFromFile(context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/prefilter.frag.spv"});
        VulkanMaterialPipeline pipeline = VulkanMaterialPipeline::Create(
            context, renderPass,
            {
                .vertexShader = vertexShader,
                .fragmentShader = fragmentShader,
                .descriptorSetLayouts = {descriptorSetLayout},
                .bindings = {},
                .pushConstantRanges = {
                    {
                        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        .offset = 0,
                        .size = sizeof(PushBlock),
                    },
                },
                .vertexDescription = StaticMeshVertex::GetVertexDescription(),
                .cullMode = CullMode::None,
                .depthFunction = DepthFunction::LessOrEqual,
            });

        // Render
        VkClearValue clearValues[1];
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};

        VkRenderPassBeginInfo renderPassBeginInfo{};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.renderPass = renderPass.GetRenderPass();
        renderPassBeginInfo.renderArea.extent.width = dim;
        renderPassBeginInfo.renderArea.extent.height = dim;
        renderPassBeginInfo.clearValueCount = 1;
        renderPassBeginInfo.pClearValues = clearValues;
        renderPassBeginInfo.framebuffer = framebuffer;

        std::array<glm::mat4, 6> captureViews = {
            glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
        };

        VkCommandBuffer commandBuffer = VkUtil::BeginSingleTimeCommands(context.GetDevice(), commandPool);

        VkViewport viewport = {
            .x = 0.0f,
            .y = 0.0f,
            .width = (float)dim,
            .height = (float)dim,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };

        VkRect2D scissor = {
            .offset = {0, 0},
            .extent = {dim, dim},
        };

        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        image.TransitionLayout(
            context.GetDevice(),
            commandBuffer,
            context.GetTransferQueue(),
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        for (uint32_t mip = 0; mip < numMips; mip++)
        {
            pushBlock.roughness = static_cast<float>(mip) / static_cast<float>(numMips - 1);
            for (uint32_t face = 0; face < 6; face++)
            {
                viewport.width = static_cast<float>(dim * std::pow(0.5f, mip));
                viewport.height = static_cast<float>(dim * std::pow(0.5f, mip));
                vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

                vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

                pushBlock.mvp = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 512.0f) * glm::mat4(glm::mat3(captureViews[face]));
                vkCmdPushConstants(commandBuffer, pipeline.GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushBlock), &pushBlock);

                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetPipeline());
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

                MeshDrawInfo meshInfo = skyboxMesh.GetDrawInfo();
                VkBuffer vertexBuffers[] = {meshInfo.vertexBuffer};
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
                vkCmdBindIndexBuffer(commandBuffer, meshInfo.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

                vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(meshInfo.indexCount), 1, 0, 0, 0);

                vkCmdEndRenderPass(commandBuffer);

                offscreenImage.TransitionLayout(
                    context.GetDevice(),
                    commandBuffer,
                    context.GetTransferQueue(),
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

                VkImageCopy copyRegion{};
                copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.srcSubresource.baseArrayLayer = 0;
                copyRegion.srcSubresource.mipLevel = 0;
                copyRegion.srcSubresource.layerCount = 1;
                copyRegion.srcOffset = {0, 0, 0};

                copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.dstSubresource.baseArrayLayer = face;
                copyRegion.dstSubresource.mipLevel = mip;
                copyRegion.dstSubresource.layerCount = 1;
                copyRegion.dstOffset = {0, 0, 0};

                copyRegion.extent = {static_cast<uint32_t>(viewport.width), static_cast<uint32_t>(viewport.height), 1};

                vkCmdCopyImage(commandBuffer, offscreenImage.GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image.GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

                offscreenImage.TransitionLayout(
                    context.GetDevice(),
                    commandBuffer,
                    context.GetTransferQueue(),
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            }
        }

        image.TransitionLayout(
            context.GetDevice(),
            commandBuffer,
            context.GetTransferQueue(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        VkUtil::EndSingleTimeCommands(context.GetDevice(), commandPool, context.GetTransferQueue(), commandBuffer);

        vkDestroySampler(context.GetDevice(), sampler, nullptr);
        offscreenImage.Destroy(context);
        vertexShader.Destroy(context);
        fragmentShader.Destroy(context);
        vkDestroyFramebuffer(context.GetDevice(), framebuffer, nullptr);
        vkDestroyDescriptorSetLayout(context.GetDevice(), descriptorSetLayout, nullptr);
        pipeline.Destroy(context);
        renderPass.Destroy(context);

        auto tEnd = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart).count();
        std::cout << "Prefiltered map generation took " << duration << "ms" << std::endl;

        return image;
    }

}