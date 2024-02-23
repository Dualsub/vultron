#pragma once

#include "Vultron/Core/Core.h"
#include "Vultron/Types.h"
#include "Vultron/Window.h"
#include "Vultron/Vulkan/VulkanUtils.h"
#include "Vultron/Vulkan/VulkanContext.h"
#include "Vultron/Vulkan/VulkanSwapchain.h"
#include "Vultron/Vulkan/VulkanBuffer.h"
#include "Vultron/Vulkan/VulkanImage.h"
#include "Vultron/Vulkan/VulkanMesh.h"
#include "Vultron/Vulkan/VulkanShader.h"
#include "Vultron/Vulkan/VulkanResourcePool.h"

#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"

#include <array>
#include <optional>

namespace Vultron
{
    struct FrameData
    {
        VkSemaphore imageAvailableSemaphore;
        VkSemaphore renderFinishedSemaphore;
        VkFence inFlightFence;

        VkCommandBuffer commandBuffer;

        // Global resources
        VulkanBuffer instanceBuffer;
        uint32_t instanceCount = 0;

        // Material instance resources
        VulkanBuffer uniformBuffer;
        VkDescriptorSet descriptorSet;
    };

    struct InstanceData
    {
        glm::mat4 model;
    };

    struct UniformBufferData
    {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec3 lightDir;
        float _padding1;
        glm::vec3 viewPos;
        float _padding2;
    };

    static_assert(sizeof(UniformBufferData) % 16 == 0);

    constexpr size_t c_maxInstances = 2000;
    constexpr uint32_t c_frameOverlap = 2;

    class VulkanRenderer
    {
    private:
        VulkanContext m_context;

        // Swap chain
        VulkanSwapchain m_swapchain;

        // Render pass
        VkRenderPass m_renderPass;

        // Material pipeline
        VkPipelineLayout m_pipelineLayout;
        VkPipeline m_graphicsPipeline;
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
        VulkanImage m_texture;

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
        bool InitializeTestResources();

        // Permanent resources
        bool InitializeSamplers();
        bool InitializeDepthBuffer();

        // Material instance resources
        bool InitializeDescriptorPool();
        bool InitializeUniformBuffers();
        bool InitializeInstanceBuffer();
        bool InitializeDescriptorSets();

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
    };

}
