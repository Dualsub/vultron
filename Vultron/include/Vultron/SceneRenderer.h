#pragma once

#include "Vultron/Window.h"
#include "Vultron/Vulkan/Utils.h"
#include "Vultron/Vulkan/VulkanShader.h"

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"

#include <array>
#include <optional>

namespace Vultron
{
    struct FrameData
    {
    };

    constexpr unsigned int c_frameOverlap = 2;

    constexpr std::array<const char *, 1> c_deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    constexpr std::array<const char *, 1> c_validationLayers = {"VK_LAYER_KHRONOS_validation"};
    constexpr bool c_validationLayersEnabled = true;

    class SceneRenderer
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
        VkCommandBuffer m_commandBuffer;

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
        bool InitializeAllocator();

        bool CheckDeviceExtensionSupport(VkPhysicalDevice device);

        // Validation/debugging
        bool InitializeDebugMessenger();
        bool CheckValidationLayerSupport();

        void Draw();

    public:
        SceneRenderer() = default;
        ~SceneRenderer() = default;

        bool Initialize(const Window &window);

        void BeginFrame();
        void EndFrame();

        void Shutdown();
    };

}
