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
    class SceneRenderer
    {
    private:
        const std::array<const char *, 1> c_validationLayers = {
            "VK_LAYER_KHRONOS_validation"};
        const bool c_validationLayersEnabled = true;

        VkInstance m_instance;
        VkPhysicalDevice m_physicalDevice;
        VkDevice m_device;
        VkQueue m_graphicsQueue;
        VkQueue m_presentQueue;
        VkSurfaceKHR m_surface;
        VmaAllocator m_allocator;

        VkDebugUtilsMessengerEXT m_debugMessenger;

        bool InitializeInstance(const Window &window);
        bool InitializeSurface(const Window &window);
        bool InitializePhysicalDevice();
        bool InitializeLogicalDevice();
        bool InitializeAllocator();

        // Validation/debugging
        bool InitializeDebugMessenger();
        bool CheckValidationLayerSupport();

    public:
        SceneRenderer() = default;
        ~SceneRenderer() = default;

        bool Initialize(const Window &window);

        void BeginFrame();
        void EndFrame();

        void Shutdown();
    };

}
