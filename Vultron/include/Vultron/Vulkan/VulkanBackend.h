#pragma once

#include "Vultron/Core/Core.h"
#include "Vultron/Window.h"
#include "Vultron/Vulkan/VulkanUtils.h"
#include "Vultron/Vulkan/VulkanBuffer.h"
#include "Vultron/Vulkan/VulkanImage.h"
#include "Vultron/Vulkan/VulkanMesh.h"
#include "Vultron/Vulkan/VulkanShader.h"

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
    };

    struct UniformBuffer
    {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
    };

    static_assert(sizeof(UniformBuffer) % 16 == 0);

    constexpr unsigned int c_frameOverlap = 2;

    const std::vector<const char *> c_deviceExtensions =
        {
#if __APPLE__
            "VK_KHR_portability_subset",
#endif
            VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    constexpr std::array<const char *, 1> c_validationLayers = {"VK_LAYER_KHRONOS_validation"};
    constexpr bool c_validationLayersEnabled = true;

    class VulkanBackend
    {
    private:
        // Vulkan
        VkInstance m_instance;
        VkPhysicalDevice m_physicalDevice;
        VkPhysicalDeviceProperties m_deviceProperties;
        VkDevice m_device;

        // Queues
        VkQueue m_graphicsQueue;
        uint32_t m_graphicsQueueFamily;
        VkQueue m_presentQueue;

        // Swap chain
        VkSurfaceKHR m_surface;
        VkSwapchainKHR m_swapChain;
        std::vector<VkImage> m_swapChainImages;
        std::vector<VkImageView> m_swapChainImageViews;
        std::vector<VkFramebuffer> m_swapChainFramebuffers;
        VkFormat m_swapChainImageFormat;
        VkExtent2D m_swapChainExtent;

        // Render pass
        VkRenderPass m_renderPass;

        // Pipeline
        VkPipelineLayout m_pipelineLayout;
        VkPipeline m_graphicsPipeline;

        // Samplers
        VkSampler m_textureSampler;

        // Descriptor set
        VkDescriptorSetLayout m_descriptorSetLayout;
        VkDescriptorPool m_descriptorPool;
        VkDescriptorSet m_descriptorSets[c_frameOverlap];

        // Uniform buffer
        VulkanBuffer m_uniformBuffers[c_frameOverlap];

        // Command pool
        VkCommandPool m_commandPool;

        // Allocator
        VmaAllocator m_allocator;

        // Debugging
        VkDebugUtilsMessengerEXT m_debugMessenger;

        // Frame data
        FrameData m_frames[c_frameOverlap];
        uint32_t m_currentFrameIndex = 0;

        // Assets, will be removed in the future
        Ptr<VulkanShader> m_vertexShader;
        Ptr<VulkanShader> m_fragmentShader;
        Ptr<VulkanMesh> m_mesh;
        Ptr<VulkanImage> m_texture;

        bool InitializeInstance(const Window &window);
        bool InitializeSurface(const Window &window);
        bool InitializePhysicalDevice();
        bool InitializeLogicalDevice();
        bool InitializeSwapChain(uint32_t width, uint32_t height);
        bool InitializeImageViews();
        bool InitializeRenderPass();
        bool InitializeDescriptorSetLayout();
        bool InitializeGraphicsPipeline();
        bool InitializeFramebuffers();
        bool InitializeCommandPool();
        bool InitializeCommandBuffer();
        bool InitializeSyncObjects();
        bool InitializeAllocator();
        // TODO: Remove this in the future
        bool InitializeTestResources();
        bool InitializeSamplers();
        bool InitializeUniformBuffers();
        bool InitializeDescriptorPool();
        bool InitializeDescriptorSets();

        void RecreateSwapChain(uint32_t width, uint32_t height);

        bool CheckDeviceExtensionSupport(VkPhysicalDevice device);

        // Validation/debugging
        bool InitializeDebugMessenger();
        bool CheckValidationLayerSupport();

        void WriteCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
        void Draw();

    public:
        VulkanBackend() = default;
        ~VulkanBackend() = default;

        bool Initialize(const Window &window);

        void BeginFrame();
        void EndFrame();

        void Shutdown();
    };

}
