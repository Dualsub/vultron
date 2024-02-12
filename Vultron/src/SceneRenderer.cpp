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

        if (!InitializeLogicalDevice())
        {
            std::cerr << "Faild to logical device." << std::endl;
            return false;
        }

        if (!InitializeSwapChain(window))
        {
            std::cerr << "Faild to swap chain." << std::endl;
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
        auto requiredExtensions = std::vector<const char *>(windowExtensions.size());

        std::copy(windowExtensions.begin(), windowExtensions.end(), requiredExtensions.begin());

        if (c_validationLayersEnabled)
        {
            requiredExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

#if __APPLE__
        requiredExtensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

        createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
        createInfo.ppEnabledExtensionNames = requiredExtensions.data();

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

            VkPhysicalDeviceVulkan12Features deviceFeatures12;
            deviceFeatures12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
            deviceFeatures12.pNext = nullptr;

            VkPhysicalDeviceVulkan13Features deviceFeatures13;
            deviceFeatures13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
            deviceFeatures13.pNext = &deviceFeatures12;

            VkPhysicalDeviceFeatures2 allFeatures;
            allFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            allFeatures.pNext = &deviceFeatures13;
            vkGetPhysicalDeviceFeatures2(device, &allFeatures);
            VkPhysicalDeviceFeatures &deviceFeatures = allFeatures.features;

            VkUtil::QueueFamilies families = VkUtil::QueryQueueFamilies(device, m_surface);

            bool allowed = families.IsComplete() && CheckDeviceExtensionSupport(device);

            bool swapChainAdequate = false;
            if (allowed)
            {
                VkUtil::SwapChainSupport swapChainSupport = VkUtil::QuerySwapChainSupport(device, m_surface);
                swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
                allowed &= swapChainAdequate;
            }

            if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
                deviceFeatures.geometryShader && allowed)
            {
                m_physicalDevice = device;
                break;
            }

            if (allowed)
            {
                m_physicalDevice = device;
                break;
            }
        }

        if (m_physicalDevice == VK_NULL_HANDLE)
        {
            std::cerr << "Could not find suitable device." << std::endl;
            return false;
        }

        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(m_physicalDevice, &deviceProperties);
        std::cout << "Selected ";
        std::cout << deviceProperties.deviceName;
        std::cout << " for rendering.";
        std::cout << std::endl;
        return true;
    }

    bool SceneRenderer::InitializeLogicalDevice()
    {
        VkUtil::QueueFamilies families = VkUtil::QueryQueueFamilies(m_physicalDevice, m_surface);

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

        createInfo.enabledExtensionCount = static_cast<uint32_t>(c_deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = c_deviceExtensions.data();

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

    bool SceneRenderer::InitializeSwapChain(const Window &window)
    {
        VkUtil::SwapChainSupport support = VkUtil::QuerySwapChainSupport(m_physicalDevice, m_surface);

        // Pick format
        VkSurfaceFormatKHR surfaceFormat = support.formats[0];
        for (const auto &availableFormat : support.formats)
        {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                surfaceFormat = availableFormat;
            }
        }

        m_swapChainImageFormat = surfaceFormat.format;

        // Pick present mode
        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
        for (const auto &mode : support.presentModes)
        {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            }
        }

        // Pick extent
        VkExtent2D extent;
        VkSurfaceCapabilitiesKHR &capabilities = support.capabilities;

        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            extent = capabilities.currentExtent;
        }
        else
        {
            const auto [width, height] = window.GetExtent();
            VkExtent2D actualExtent = {width, height};

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            extent = actualExtent;
        }

        m_swapChainExtent = extent;

        // Pick image count
        uint32_t imageCount = support.capabilities.minImageCount + 1;
        if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount)
        {
            imageCount = support.capabilities.maxImageCount;
        }

        //// Create swap chain
        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        VkUtil::QueueFamilies families = VkUtil::QueryQueueFamilies(m_physicalDevice, m_surface);
        uint32_t queueFamilyIndices[] = {families.graphicsFamily.value(), families.presentFamily.value()};

        if (families.graphicsFamily != families.presentFamily)
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;     // Optional
            createInfo.pQueueFamilyIndices = nullptr; // Optional
        }

        createInfo.preTransform = support.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        VK_CHECK(vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain));

        vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);
        m_swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, m_swapChainImages.data());

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

    bool SceneRenderer::CheckDeviceExtensionSupport(VkPhysicalDevice device)
    {
        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

        std::set<std::string> requiredExtensions(c_deviceExtensions.begin(), c_deviceExtensions.end());

        for (const auto &extension : extensions)
        {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
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
        vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
        vkDestroyDevice(m_device, nullptr);

        if (c_validationLayersEnabled)
        {
            DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
        }

        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        vkDestroyInstance(m_instance, nullptr);
    }

}