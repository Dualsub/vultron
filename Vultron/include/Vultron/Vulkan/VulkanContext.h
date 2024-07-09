#pragma once

#include "Vultron/Window.h"
#include "Vultron/Core/Core.h"
#include "Vultron/Vulkan/VulkanTypes.h"

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
        QueueFamilies m_queueFamilies;
        VkQueue m_graphicsQueue;
        VkQueue m_presentQueue;
        VkQueue m_computeQueue;
        VkQueue m_transferQueue;
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
        inline uint32_t GetGraphicsQueueFamily() const { return m_queueFamilies.graphicsFamily.value_or(0); }
        inline uint32_t GetPresentQueueFamily() const { return m_queueFamilies.presentFamily.value_or(0); }
        inline uint32_t GetComputeQueueFamily() const { return m_queueFamilies.computeFamily.value_or(0); }
        inline uint32_t GetTransferQueueFamily() const { return m_queueFamilies.transferFamily.value_or(0); }
        inline VkQueue GetGraphicsQueue() const { return m_graphicsQueue; }
        inline VkQueue GetPresentQueue() const { return m_presentQueue; }
        inline VkQueue GetComputeQueue() const { return m_computeQueue; }
        inline VkQueue GetTransferQueue() const { return m_transferQueue; }
        inline VkSurfaceKHR GetSurface() const { return m_surface; }
        inline VmaAllocator GetAllocator() const { return m_allocator; }
    };
}