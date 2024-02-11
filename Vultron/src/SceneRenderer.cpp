#include "Vultron/SceneRenderer.h"

#include "Vultron/Vulkan/Debug.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <vector>
#include <iostream>
#include <set>

namespace Vultron
{
    bool SceneRenderer::Initialize(const Window &window)
    {
        if (c_validationLayersEnabled && !CheckValidationLayerSupport())
        {
            std::cerr << "Validation layer not supported." << std::endl;
            return false;
        }

        if (!InitializeInstance(window))
        {
            std::cerr << "Faild to initialize instance." << std::endl;
            return false;
        }

        if (!InitializeSurface(window))
        {
            std::cerr << "Faild to surface." << std::endl;
            return false;
        }

        if (c_validationLayersEnabled && !InitializeDebugMessenger())
        {
            std::cerr << "Faild to initialize debug messages." << std::endl;
            return false;
        }

        if (!InitializePhysicalDevice())
        {
            std::cerr << "Faild to physical device." << std::endl;
            return false;
        }

        // if (!InitializeAllocator())
        // {
        //     std::cerr << "Faild to initialize instance" << std::endl;
        //     return false;
        // }

        return true;
    }

    bool SceneRenderer::InitializeInstance(const Window &window)
    {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Vultron";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        std::vector<const char *> windowExtensions = window.GetWindowExtensions();
        if (c_validationLayersEnabled)
        {
            windowExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        createInfo.enabledExtensionCount = static_cast<uint32_t>(windowExtensions.size());
        createInfo.ppEnabledExtensionNames = windowExtensions.data();

        if (c_validationLayersEnabled)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(c_validationLayers.size());
            createInfo.ppEnabledLayerNames = c_validationLayers.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }

        VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_instance));

        return true;
    }

    bool SceneRenderer::InitializeSurface(const Window &window)
    {
        Window::CreateVulkanSurface(window, m_instance, &m_surface);
        return true;
    }

    bool SceneRenderer::InitializePhysicalDevice()
    {
        m_physicalDevice = VK_NULL_HANDLE;

        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

        assert(deviceCount > 0 && "No devices.");

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

        for (const auto &device : devices)
        {
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(device, &deviceProperties);

            VkPhysicalDeviceFeatures deviceFeatures;
            vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

            QueueFamilies families = QueryQueueFamilies(device, m_surface);

            if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
                deviceFeatures.geometryShader && families.IsComplete())
            {
                m_physicalDevice = device;
                std::cout << "Selected ";
                std::cout << deviceProperties.deviceName;
                std::cout << " for rendering.";
                std::cout << std::endl;
                break;
            }
        }

        return m_physicalDevice != VK_NULL_HANDLE;
    }

    bool SceneRenderer::InitializeLogicalDevice()
    {
        QueueFamilies families = QueryQueueFamilies(m_physicalDevice, m_surface);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {families.graphicsFamily.value(), families.presentFamily.value()};

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        createInfo.pEnabledFeatures = &deviceFeatures;

        createInfo.enabledExtensionCount = 0;

        if (c_validationLayersEnabled)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(c_validationLayers.size());
            createInfo.ppEnabledLayerNames = c_validationLayers.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }

        VK_CHECK(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device));

        vkGetDeviceQueue(m_device, families.graphicsFamily.value(), 0, &m_graphicsQueue);
        vkGetDeviceQueue(m_device, families.presentFamily.value(), 0, &m_presentQueue);

        return true;
    }

    bool SceneRenderer::InitializeAllocator()
    {
        VmaVulkanFunctions vulkanFunctions = {};
        vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo allocatorCreateInfo = {};
        allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
        allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
        allocatorCreateInfo.physicalDevice = m_physicalDevice;
        allocatorCreateInfo.device = m_device;
        allocatorCreateInfo.instance = m_instance;
        allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;

        VK_CHECK(vmaCreateAllocator(&allocatorCreateInfo, &m_allocator));

        return true;
    }

    bool SceneRenderer::InitializeDebugMessenger()
    {
        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = VulkanDebugCallback;
        VK_CHECK(CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger));

        return true;
    }

    bool SceneRenderer::CheckValidationLayerSupport()
    {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char *layerName : c_validationLayers)
        {
            bool layerFound = false;

            for (const auto &layerProperties : availableLayers)
            {
                if (std::strcmp(layerName, layerProperties.layerName) == 0)
                {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound)
            {
                return false;
            }
        }

        return true;
    }

    void SceneRenderer::BeginFrame()
    {
    }

    void SceneRenderer::EndFrame()
    {
    }

    void SceneRenderer::Shutdown()
    {
        vkDestroyDevice(m_device, nullptr);

        if (c_validationLayersEnabled)
        {
            DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
        }

        vkDestroyInstance(m_instance, nullptr);
    }

}