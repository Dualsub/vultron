#include "Vultron/Vulkan/VulkanRenderer.h"

#include "Vultron/Vulkan/VulkanUtils.h"
#include "Vultron/Vulkan/VulkanInitializers.h"
#include "Vultron/Vulkan/Debug.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cstdint>
#include <chrono>
#include <iostream>
#include <limits>
#include <vector>
#include <set>
#include <string>
#include <random>

namespace Vultron
{
    bool VulkanRenderer::Initialize(const Window &window)
    {
        if (!m_context.Initialize(window))
        {
            std::cerr << "Faild to initialize context." << std::endl;
            return false;
        }

        if (c_validationLayersEnabled && !InitializeDebugMessenger())
        {
            std::cerr << "Faild to initialize debug messages." << std::endl;
            return false;
        }

        const auto [width, height] = window.GetExtent();
        if (!m_swapchain.Initialize(m_context, width, height))
        {
            std::cerr << "Faild to initialize swap chain." << std::endl;
            return false;
        }

        if (!InitializeRenderPass())
        {
            std::cerr << "Faild to initialize render pass." << std::endl;
            return false;
        }

        if (!InitializeDescriptorSetLayout())
        {
            std::cerr << "Faild to initialize descriptor set layout." << std::endl;
            return false;
        }

        if (!InitializeGraphicsPipeline())
        {
            std::cerr << "Faild to initialize graphics pipeline." << std::endl;
            return false;
        }

        if (!InitializeCommandPool())
        {
            std::cerr << "Faild to initialize command pool." << std::endl;
            return false;
        }

        if (!InitializeCommandBuffer())
        {
            std::cerr << "Faild to initialize command buffer." << std::endl;
            return false;
        }

        if (!InitializeDepthBuffer())
        {
            std::cerr << "Faild to initialize depth buffer." << std::endl;
            return false;
        }

        if (!InitializeFramebuffers())
        {
            std::cerr << "Faild to initialize framebuffers." << std::endl;
            return false;
        }

        if (!InitializeSyncObjects())
        {
            std::cerr << "Faild to initialize sync objects." << std::endl;
            return false;
        }

        if (!InitializeSamplers())
        {
            std::cerr << "Faild to initialize sampler." << std::endl;
            return false;
        }

        if (!InitializeUniformBuffers())
        {
            std::cerr << "Faild to initialize uniform buffers." << std::endl;
            return false;
        }

        if (!InitializeInstanceBuffer())
        {
            std::cerr << "Faild to initialize instance buffer." << std::endl;
            return false;
        }

        if (!InitializeDescriptorPool())
        {
            std::cerr << "Faild to initialize descriptor pool." << std::endl;
            return false;
        }

        if (!InitializeDescriptorSets())
        {
            std::cerr << "Faild to initialize descriptor sets." << std::endl;
            return false;
        }

        if (!InitializeTestResources())
        {
            std::cerr << "Faild to initialize geometry." << std::endl;
            return false;
        }

        return true;
    }

    bool VulkanRenderer::InitializeRenderPass()
    {
        m_renderPass = VulkanRenderPass::Create(
            m_context,
            {.attachments = {
                 // Color attachment
                 {
                     .format = m_swapchain.GetImageFormat(),
                 },
                 // Depth attachment
                 {
                     .type = VulkanRenderPass::AttachmentType::Depth,
                     .format = VK_FORMAT_D32_SFLOAT,
                     .samples = VK_SAMPLE_COUNT_1_BIT,
                     .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                     .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                     .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                     .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                     .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                     .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}}});

        return true;
    }

    bool VulkanRenderer::InitializeDescriptorSetLayout()
    {
        m_descriptorSetLayout = VkInit::CreateDescriptorSetLayout(
            m_context.GetDevice(),
            {
                {
                    .binding = 0,
                    .type = DescriptorType::UniformBuffer,
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                },
                {
                    .binding = 1,
                    .type = DescriptorType::StorageBuffer,
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                },
            });

        return true;
    }

    bool VulkanRenderer::InitializeGraphicsPipeline()
    {
        // Shader
        m_vertexShader = VulkanShader::CreateFromFile({.device = m_context.GetDevice(), .filepath = std::string(VLT_ASSETS_DIR) + "/shaders/triangle.vert.spv"});
        m_fragmentShader = VulkanShader::CreateFromFile({.device = m_context.GetDevice(), .filepath = std::string(VLT_ASSETS_DIR) + "/shaders/triangle.frag.spv"});

        m_materialPipeline = VulkanMaterialPipeline::Create(
            m_context, m_swapchain, m_renderPass,
            {
                .vertexShader = m_vertexShader,
                .fragmentShader = m_fragmentShader,
                .sceneDescriptorSetLayout = m_descriptorSetLayout,
                .bindings = {
                    {
                        .binding = 0,
                        .type = DescriptorType::CombinedImageSampler,
                        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                    },
                },
            });

        return true;
    }

    bool VulkanRenderer::InitializeFramebuffers()
    {
        std::vector<VkFramebuffer> &framebuffers = m_swapchain.GetFramebuffers();
        const std::vector<VkImageView> &imageViews = m_swapchain.GetImageViews();
        framebuffers.resize(imageViews.size());

        for (size_t i = 0; i < imageViews.size(); i++)
        {
            std::array<VkImageView, 2> attachments = {
                imageViews[i],
                m_depthImage.GetImageView()};

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_renderPass.GetRenderPass();
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = m_swapchain.GetExtent().width;
            framebufferInfo.height = m_swapchain.GetExtent().height;
            framebufferInfo.layers = 1;

            VK_CHECK(vkCreateFramebuffer(m_context.GetDevice(), &framebufferInfo, nullptr, &framebuffers[i]));
        }

        return true;
    }

    bool VulkanRenderer::InitializeCommandPool()
    {
        VkUtil::QueueFamilies families = VkUtil::QueryQueueFamilies(m_context.GetPhysicalDevice(), m_context.GetSurface());

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = families.graphicsFamily.value();
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VK_CHECK(vkCreateCommandPool(m_context.GetDevice(), &poolInfo, nullptr, &m_commandPool));

        return true;
    }

    bool VulkanRenderer::InitializeCommandBuffer()
    {
        for (size_t i = 0; i < c_frameOverlap; i++)
        {
            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.commandPool = m_commandPool;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = 1;

            VK_CHECK(vkAllocateCommandBuffers(m_context.GetDevice(), &allocInfo, &m_frames[i].commandBuffer));
        }

        return true;
    }

    bool VulkanRenderer::InitializeSyncObjects()
    {
        for (size_t i = 0; i < c_frameOverlap; i++)
        {
            VkSemaphoreCreateInfo semaphoreInfo{};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

            VK_CHECK(vkCreateSemaphore(m_context.GetDevice(), &semaphoreInfo, nullptr, &m_frames[i].imageAvailableSemaphore));
            VK_CHECK(vkCreateSemaphore(m_context.GetDevice(), &semaphoreInfo, nullptr, &m_frames[i].renderFinishedSemaphore));

            VkFenceCreateInfo fenceInfo{};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

            VK_CHECK(vkCreateFence(m_context.GetDevice(), &fenceInfo, nullptr, &m_frames[i].inFlightFence));
        }

        return true;
    }

    bool VulkanRenderer::InitializeSamplers()
    {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = m_context.GetDeviceProperties().limits.maxSamplerAnisotropy;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 10.0f;

        VK_CHECK(vkCreateSampler(m_context.GetDevice(), &samplerInfo, nullptr, &m_textureSampler));

        return true;
    }

    bool VulkanRenderer::InitializeDepthBuffer()
    {
        VkFormat depthFormat = VkUtil::FindDepthFormat(m_context.GetPhysicalDevice());

        m_depthImage = VulkanImage::Create(
            {.device = m_context.GetDevice(),
             .commandPool = m_commandPool,
             .queue = m_context.GetGraphicsQueue(),
             .allocator = m_context.GetAllocator(),
             .info = {
                 .width = m_swapchain.GetExtent().width,
                 .height = m_swapchain.GetExtent().height,
                 .depth = 1,
                 .format = depthFormat,
             },
             .aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT,
             .additionalUsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT});

        m_depthImage.TransitionLayout(m_context.GetDevice(), m_commandPool, m_context.GetGraphicsQueue(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

        return true;
    }

    bool VulkanRenderer::InitializeUniformBuffers()
    {
        size_t size = sizeof(UniformBufferData);

        for (size_t i = 0; i < c_frameOverlap; i++)
        {
            VulkanBuffer &uniformBuffer = m_frames[i].uniformBuffer;
            uniformBuffer = VulkanBuffer::Create({.allocator = m_context.GetAllocator(), .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, .size = size, .allocationUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});
            uniformBuffer.Map(m_context.GetAllocator());
        }

        m_uniformBufferData.lightDir = glm::vec3(0.0f, 0.0f, 1.0f);
        m_uniformBufferData.proj = glm::perspective(glm::radians(45.0f), (float)m_swapchain.GetExtent().width / (float)m_swapchain.GetExtent().height, 0.1f, 10000.0f);
        m_uniformBufferData.proj[1][1] *= -1;
        return true;
    }

    bool VulkanRenderer::InitializeInstanceBuffer()
    {
        constexpr size_t size = sizeof(InstanceData) * c_maxInstances;
        for (size_t i = 0; i < c_frameOverlap; i++)
        {
            VulkanBuffer &instanceBuffer = m_frames[i].instanceBuffer;
            instanceBuffer = VulkanBuffer::Create({.allocator = m_context.GetAllocator(), .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, .size = size, .allocationUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});
            instanceBuffer.Map(m_context.GetAllocator());
        }

        return true;
    }

    bool VulkanRenderer::InitializeDescriptorPool()
    {
        std::array<VkDescriptorPoolSize, 3> poolSizes = {
            {
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, c_maxUniformBuffers},
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, c_maxStorageBuffers},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, c_maxCombinedImageSamplers},
            }};

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = c_maxSets;

        VK_CHECK(vkCreateDescriptorPool(m_context.GetDevice(), &poolInfo, nullptr, &m_descriptorPool));

        return true;
    }

    bool VulkanRenderer::InitializeDescriptorSets()
    {
        for (size_t i = 0; i < c_frameOverlap; i++)
        {
            std::vector<DescriptorSetBinding> bindings = {
                {
                    .binding = 0,
                    .type = DescriptorType::UniformBuffer,
                    .buffer = m_frames[i].uniformBuffer.GetBuffer(),
                    .size = m_frames[i].uniformBuffer.GetSize(),
                },
                {
                    .binding = 1,
                    .type = DescriptorType::StorageBuffer,
                    .buffer = m_frames[i].instanceBuffer.GetBuffer(),
                    .size = m_frames[i].instanceBuffer.GetSize(),
                },
            };

            m_frames[i].descriptorSet = VkInit::CreateDescriptorSet(m_context.GetDevice(), m_descriptorPool, m_descriptorSetLayout, bindings);
        }

        return true;
    }

    bool VulkanRenderer::InitializeDebugMessenger()
    {
        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = VulkanDebugCallback;
        VK_CHECK(CreateDebugUtilsMessengerEXT(m_context.GetInstance(), &createInfo, nullptr, &m_debugMessenger));

        return true;
    }

    bool VulkanRenderer::InitializeTestResources()
    {
        return true;
    }

    void VulkanRenderer::RecreateSwapchain(uint32_t width, uint32_t height)
    {
        // vkDeviceWaitIdle(m_context.GetDevice());

        // for (auto framebuffer : m_swapchain.GetFramebuffers())
        // {
        //     vkDestroyFramebuffer(m_context.GetDevice(), framebuffer, nullptr);
        // }

        // for (const auto &frame : m_frames)
        // {
        //     vkFreeCommandBuffers(m_context.GetDevice(), m_commandPool, 1, &frame.commandBuffer);
        // }

        // vkDestroyPipeline(m_context.GetDevice(), m_graphicsPipeline, nullptr);
        // vkDestroyPipelineLayout(m_context.GetDevice(), m_pipelineLayout, nullptr);
        // vkDestroyRenderPass(m_context.GetDevice(), m_renderPass.GetRenderPass(), nullptr);

        // for (auto imageView : m_swapChainImageViews)
        // {
        //     vkDestroyImageView(m_context.GetDevice(), imageView, nullptr);
        // }

        // vkDestroySwapchainKHR(m_context.GetDevice(), m_swapChain, nullptr);

        // InitializeSwapChain(width, height);
        // InitializeImageViews();
        // InitializeRenderPass();
        // InitializeGraphicsPipeline();
        // InitializeFramebuffers();
        // InitializeCommandBuffer();
    }

    void VulkanRenderer::WriteCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, const std::vector<RenderBatch> &batches)
    {
        const FrameData &frame = m_frames[m_currentFrameIndex];

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;                  // Optional
        beginInfo.pInheritanceInfo = nullptr; // Optional

        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_renderPass.GetRenderPass();
        renderPassInfo.framebuffer = m_swapchain.GetFramebuffers()[imageIndex];

        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = m_swapchain.GetExtent();

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_materialPipeline.GetPipeline());

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)m_swapchain.GetExtent().width;
        viewport.height = (float)m_swapchain.GetExtent().height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = m_swapchain.GetExtent();
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        VkDescriptorSet descriptorSets[] = {frame.descriptorSet};
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_materialPipeline.GetPipelineLayout(), 0, 1, descriptorSets, 0, nullptr);

        for (const auto &batch : batches)
        {
            const VulkanMesh &mesh = m_resourcePool.GetMesh(batch.mesh);
            const VulkanMaterialInstance &material = m_resourcePool.GetMaterialInstance(batch.material);

            VkBuffer vertexBuffers[] = {mesh.GetVertexBuffer()};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, mesh.GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

            VkDescriptorSet descriptorSets[] = {material.GetDescriptorSet()};
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_materialPipeline.GetPipelineLayout(), 1, 1, descriptorSets, 0, nullptr);

            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(mesh.GetIndexCount()), batch.instanceCount, 0, 0, batch.firstInstance);
        }

        vkCmdEndRenderPass(commandBuffer);

        VK_CHECK(vkEndCommandBuffer(commandBuffer));
    }

    void VulkanRenderer::Draw(const std::vector<RenderBatch> &batches, const std::vector<glm::mat4> &instances)
    {
        constexpr uint32_t timeout = (std::numeric_limits<uint32_t>::max)();
        const uint32_t currentFrame = m_currentFrameIndex;
        const FrameData &frame = m_frames[currentFrame];

        vkWaitForFences(m_context.GetDevice(), 1, &frame.inFlightFence, VK_TRUE, timeout);
        vkResetFences(m_context.GetDevice(), 1, &frame.inFlightFence);

        uint32_t imageIndex;
        VK_CHECK(vkAcquireNextImageKHR(m_context.GetDevice(), m_swapchain.GetSwapchain(), timeout, frame.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex));
        vkResetCommandBuffer(frame.commandBuffer, 0);
        WriteCommandBuffer(frame.commandBuffer, imageIndex, batches);

        static std::chrono::high_resolution_clock::time_point lastTime = std::chrono::high_resolution_clock::now();
        const auto currentTime = std::chrono::high_resolution_clock::now();
        const float dt = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
        lastTime = currentTime;

        // Uniform buffer
        UniformBufferData ubo = m_uniformBufferData;
        const glm::vec3 viewPos = glm::vec3(0.0f, 4.0f, 3.0f);
        const glm::vec3 viewDir = glm::normalize(glm::vec3(0.0f, -1.0f, -0.3f));
        ubo.view = glm::lookAt(viewPos, viewPos + viewDir, glm::vec3(0.0f, 0.0f, 1.0f));
        frame.uniformBuffer.CopyData(&ubo, sizeof(ubo));

        // Instance buffer
        const size_t size = sizeof(InstanceData) * instances.size();
        frame.instanceBuffer.CopyData(instances.data(), size);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {frame.imageAvailableSemaphore};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &frame.commandBuffer;

        VkSemaphore signalSemaphores[] = {frame.renderFinishedSemaphore};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        VK_CHECK(vkQueueSubmit(m_context.GetGraphicsQueue(), 1, &submitInfo, frame.inFlightFence));

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {m_swapchain.GetSwapchain()};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        VK_CHECK(vkQueuePresentKHR(m_context.GetPresentQueue(), &presentInfo));

        m_currentFrameIndex = (currentFrame + 1) % c_frameOverlap;
    }

    void VulkanRenderer::Shutdown()
    {
        vkDeviceWaitIdle(m_context.GetDevice());

        for (size_t i = 0; i < c_frameOverlap; i++)
        {
            vkDestroySemaphore(m_context.GetDevice(), m_frames[i].imageAvailableSemaphore, nullptr);
            vkDestroySemaphore(m_context.GetDevice(), m_frames[i].renderFinishedSemaphore, nullptr);
            vkDestroyFence(m_context.GetDevice(), m_frames[i].inFlightFence, nullptr);

            vkFreeCommandBuffers(m_context.GetDevice(), m_commandPool, 1, &m_frames[i].commandBuffer);

            m_frames[i].uniformBuffer.Unmap(m_context.GetAllocator());
            m_frames[i].uniformBuffer.Destroy(m_context.GetAllocator());

            m_frames[i].instanceBuffer.Unmap(m_context.GetAllocator());
            m_frames[i].instanceBuffer.Destroy(m_context.GetAllocator());
        }

        vkDestroySampler(m_context.GetDevice(), m_textureSampler, nullptr);

        m_depthImage.Destroy(m_context);
        m_resourcePool.Destroy(m_context);
        m_vertexShader.Destroy(m_context);
        m_fragmentShader.Destroy(m_context);

        vkDestroyCommandPool(m_context.GetDevice(), m_commandPool, nullptr);

        vkDestroyDescriptorPool(m_context.GetDevice(), m_descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(m_context.GetDevice(), m_descriptorSetLayout, nullptr);
        m_materialPipeline.Destroy(m_context);

        m_renderPass.Destroy(m_context);
        m_swapchain.Destroy(m_context);

        if (c_validationLayersEnabled)
        {
            DestroyDebugUtilsMessengerEXT(m_context.GetInstance(), m_debugMessenger, nullptr);
        }

        m_context.Destroy();
    }

    RenderHandle VulkanRenderer::LoadMesh(const std::string &filepath)
    {
        VulkanMesh mesh = VulkanMesh::CreateFromFile(
            {.device = m_context.GetDevice(),
             .commandPool = m_commandPool,
             .queue = m_context.GetGraphicsQueue(),
             .allocator = m_context.GetAllocator(),
             .filepath = filepath});

        return m_resourcePool.AddMesh(std::move(mesh));
    }

    RenderHandle VulkanRenderer::LoadImage(const std::string &filepath)
    {
        VulkanImage image = VulkanImage::CreateFromFile(
            {.device = m_context.GetDevice(),
             .commandPool = m_commandPool,
             .queue = m_context.GetGraphicsQueue(),
             .allocator = m_context.GetAllocator(),
             .filepath = filepath});

        return m_resourcePool.AddImage(std::move(image));
    }
}