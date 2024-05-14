#pragma once

#include "Vultron/Core/Core.h"
#include "Vultron/Window.h"

#include "vulkan/vulkan.h"
#include "vk_mem_alloc.h"

#include <array>

namespace Vultron
{
    const std::vector<const char *> c_deviceExtensions =
        {
#if __APPLE__
            "VK_KHR_portability_subset",
#endif
            VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
            VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    constexpr std::array<const char *, 1> c_validationLayers = {"VK_LAYER_KHRONOS_validation"};

#if VLT_ENABLE_VALIDATION_LAYERS
    constexpr bool c_validationLayersEnabled = true;
#else
    constexpr bool c_validationLayersEnabled = false;
#endif

    class VulkanContext
    {
    private:
        VkInstance m_instance;
        VkPhysicalDevice m_physicalDevice;
        VkPhysicalDeviceProperties m_deviceProperties;
        VkDevice m_device;
        uint32_t m_graphicsQueueFamily;
        VkQueue m_graphicsQueue;
        VkQueue m_presentQueue;
        VkQueue m_computeQueue;
        VkSurfaceKHR m_surface;
        VmaAllocator m_allocator;

        bool InitializeInstance(const Window &window);
        bool InitializeSurface(const Window &window);
        bool InitializePhysicalDevice();
        bool InitializeLogicalDevice();
        bool InitializeAllocator();

        // Device context
        bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
        bool CheckValidationLayerSupport();

    public:
        VulkanContext() = default;
        ~VulkanContext() = default;

        bool Initialize(const Window &window);
        void Destroy();

        inline VkInstance GetInstance() const { return m_instance; }
        inline VkPhysicalDevice GetPhysicalDevice() const { return m_physicalDevice; }
        inline VkPhysicalDeviceProperties GetDeviceProperties() const { return m_deviceProperties; }
        inline VkDevice GetDevice() const { return m_device; }
        inline uint32_t GetGraphicsQueueFamily() const { return m_graphicsQueueFamily; }
        inline VkQueue GetGraphicsQueue() const { return m_graphicsQueue; }
        inline VkQueue GetPresentQueue() const { return m_presentQueue; }
        inline VkQueue GetComputeQueue() const { return m_computeQueue; }
        inline VkSurfaceKHR GetSurface() const { return m_surface; }
        inline VmaAllocator GetAllocator() const { return m_allocator; }
    };
}