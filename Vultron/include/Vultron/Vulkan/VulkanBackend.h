#pragma once

#include "Vultron/Window.h"
#include "Vultron/Vulkan/Utils.h"
#include "Vultron/Vulkan/VulkanBuffer.h"
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
        std::shared_ptr<VulkanShader> m_vertexShader;
        std::shared_ptr<VulkanShader> m_fragmentShader;
        std::unique_ptr<VulkanBuffer> m_vertexBuffer;

        bool InitializeInstance(const Window &window);
        bool InitializeSurface(const Window &window);
        bool InitializePhysicalDevice();
        bool InitializeLogicalDevice();
        bool InitializeSwapChain(uint32_t width, uint32_t height);
        bool InitializeImageViews();
        bool InitializeRenderPass();
        bool InitializeGraphicsPipeline();
        bool InitializeFramebuffers();
        bool InitializeCommandPool();
        bool InitializeCommandBuffer();
        bool InitializeSyncObjects();
        bool InitializeAllocator();
        bool InitializeGeometry();

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
