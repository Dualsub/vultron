#pragma once

#include "Vultron/Window.h"
#include "Vultron/Vulkan/Utils.h"

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
        VkInstance m_instance;
        VkPhysicalDevice m_physicalDevice;
        VkDevice m_device;

        VkQueue m_graphicsQueue;
        uint32_t m_graphicsQueueFamily;
        VkQueue m_presentQueue;

        VkSurfaceKHR m_surface;

        VkSwapchainKHR m_swapChain;
        std::vector<VkImage> m_swapChainImages;
        std::vector<VkImageView> m_swapChainImageViews;
        VkFormat m_swapChainImageFormat;
        VkExtent2D m_swapChainExtent;

        VmaAllocator m_allocator;

        VkDebugUtilsMessengerEXT m_debugMessenger;

        FrameData m_frames[c_frameOverlap];
        uint32_t m_currentFrameIndex = 0;

        bool InitializeInstance(const Window &window);
        bool InitializeSurface(const Window &window);
        bool InitializePhysicalDevice();
        bool InitializeLogicalDevice();
        bool InitializeSwapChain(uint32_t width, uint32_t height);
        bool InitializeImageViews();
        bool InitializeGraphicsPipeline();
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
