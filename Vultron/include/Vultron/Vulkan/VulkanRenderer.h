#pragma once

#define GLM_FORCE_ALIGNED_GENTYPES

#include "Vultron/Core/Core.h"
#include "Vultron/Types.h"
#include "Vultron/Window.h"
#include "Vultron/Vulkan/VulkanTypes.h"
#include "Vultron/Vulkan/VulkanUtils.h"
#include "Vultron/Vulkan/VulkanContext.h"
#include "Vultron/Vulkan/VulkanMaterial.h"
#include "Vultron/Vulkan/VulkanBuffer.h"
#include "Vultron/Vulkan/VulkanImage.h"
#include "Vultron/Vulkan/VulkanMesh.h"
#include "Vultron/Vulkan/VulkanRenderPass.h"
#include "Vultron/Vulkan/VulkanResourcePool.h"
#include "Vultron/Vulkan/VulkanShader.h"
#include "Vultron/Vulkan/VulkanSwapchain.h"

#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"

#include <array>
#include <iostream>
#include <optional>

namespace Vultron
{
    struct TexturedMaterial
    {
        RenderHandle texture;

        std::vector<DescriptorSetBinding> GetBindings(const ResourcePool &pool, VkSampler sampler) const
        {
            const auto &image = pool.GetImage(texture);
            return {
                {
                    .binding = 0,
                    .type = DescriptorType::CombinedImageSampler,
                    .imageView = image.GetImageView(),
                    .sampler = sampler,
                },
            };
        }
    };

    struct PBRMaterial
    {
        RenderHandle albedo;
        RenderHandle normal;
        RenderHandle metallicRoughnessAO;

        std::vector<DescriptorSetBinding> GetBindings(const ResourcePool &pool, VkSampler sampler) const
        {
            const auto &albedoImage = pool.GetImage(albedo);
            const auto &normalImage = pool.GetImage(normal);
            const auto &metallicRoughnessAOImage = pool.GetImage(metallicRoughnessAO);
            return {
                {
                    .binding = 0,
                    .type = DescriptorType::CombinedImageSampler,
                    .imageView = albedoImage.GetImageView(),
                    .sampler = sampler,
                },
                {
                    .binding = 1,
                    .type = DescriptorType::CombinedImageSampler,
                    .imageView = normalImage.GetImageView(),
                    .sampler = sampler,
                },
                {
                    .binding = 2,
                    .type = DescriptorType::CombinedImageSampler,
                    .imageView = metallicRoughnessAOImage.GetImageView(),
                    .sampler = sampler,
                },
            };
        }
    };

    struct FrameData
    {
        VkSemaphore imageAvailableSemaphore;
        VkSemaphore renderFinishedSemaphore;
        VkFence inFlightFence;

        VkCommandBuffer commandBuffer;

        // Global scene data resources
        VulkanBuffer instanceBuffer;
        uint32_t instanceCount = 0;
        VulkanBuffer uniformBuffer;
        VkDescriptorSet descriptorSet;
    };

    struct InstanceData
    {
        glm::mat4 model;
    };

    struct UniformBufferData
    {
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec3 viewPos;
        float _padding1;
        glm::vec3 lightDir;
        float _padding2;
        glm::vec3 lightColor;
        float _padding3;
    };

    static_assert(sizeof(UniformBufferData) % 16 == 0);

    constexpr size_t c_maxInstances = 100;
    constexpr uint32_t c_frameOverlap = 2;

    constexpr uint32_t c_maxSets = static_cast<uint32_t>(c_maxInstances * 2);
    constexpr uint32_t c_maxUniformBuffers = 2 * c_maxSets;
    constexpr uint32_t c_maxStorageBuffers = 2 * c_maxSets;
    constexpr uint32_t c_maxCombinedImageSamplers = 2 * c_maxSets;

    class VulkanRenderer
    {
    private:
        VulkanContext m_context;

        // Swap chain
        VulkanSwapchain m_swapchain;

        // Render pass
        VulkanRenderPass m_renderPass;

        // Material pipeline
        VulkanMaterialPipeline m_pbrPipeline;
        VkDescriptorSetLayout m_descriptorSetLayout;
        VkDescriptorPool m_descriptorPool;

        // Material instance resources
        UniformBufferData m_uniformBufferData{};
        VulkanImage m_depthImage;
        VkSampler m_textureSampler;
        VkSampler m_depthSampler;

        // Command pool
        VkCommandPool m_commandPool;

        // Debugging
        VkDebugUtilsMessengerEXT m_debugMessenger;

        // Frame data
        FrameData m_frames[c_frameOverlap];
        uint32_t m_currentFrameIndex = 0;

        // Assets, will be removed in the future
        VulkanShader m_vertexShader;
        VulkanShader m_fragmentShader;

        // Permanent resources
        ResourcePool m_resourcePool;

        // Render pass
        bool InitializeRenderPass();
        bool InitializeFramebuffers();

        // Material pipeline
        bool InitializeDescriptorSetLayout();
        bool InitializeGraphicsPipeline();

        // Command pool
        bool InitializeCommandPool();
        bool InitializeCommandBuffer();

        bool InitializeSyncObjects();

        // Permanent resources
        bool InitializeSamplers();
        bool InitializeDepthBuffer();
        bool InitializeDescriptorPool();

        // Material instance resources
        bool InitializeUniformBuffers();
        bool InitializeInstanceBuffer();
        bool InitializeDescriptorSets();

        // Assets, will be removed in the future
        bool InitializeTestResources();

        // Swapchain
        void RecreateSwapchain(uint32_t width, uint32_t height);

        // Validation/debugging
        bool InitializeDebugMessenger();

        // Command buffer
        void WriteCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, const std::vector<RenderBatch> &batches);

    public:
        VulkanRenderer() = default;
        ~VulkanRenderer() = default;

        bool Initialize(const Window &window);
        void Draw(const std::vector<RenderBatch> &batches, const std::vector<glm::mat4> &instances);
        void Shutdown();

        RenderHandle LoadMesh(const std::string &filepath);
        RenderHandle LoadImage(const std::string &filepath);

        template <typename T>
        RenderHandle CreateMaterial(const T &materialCreateInfo)
        {
            std::vector<DescriptorSetBinding> bindings = materialCreateInfo.GetBindings(m_resourcePool, m_textureSampler);
            auto materialInstance = VulkanMaterialInstance::Create(
                m_context, m_descriptorPool, m_pbrPipeline,
                {
                    bindings,
                });

            return m_resourcePool.AddMaterialInstance(materialInstance);
        }
    };

}
