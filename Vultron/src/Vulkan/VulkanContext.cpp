#include "Vultron/Vulkan/VulkanContext.h"

#include "Vultron/Vulkan/VulkanUtils.h"

#include <iostream>
#include <set>

namespace Vultron
{

    bool VulkanContext::Initialize(const Window &window)
    {
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

        if (!InitializePhysicalDevice())
        {
            std::cerr << "Faild to initialize physical device." << std::endl;
            return false;
        }

        if (!InitializeLogicalDevice())
        {
            std::cerr << "Faild to initialize logical device." << std::endl;
            return false;
        }

        if (!InitializeAllocator())
        {
            std::cerr << "Faild to initialize instance." << std::endl;
            return false;
        }

        return true;
    }

    bool VulkanContext::InitializeInstance(const Window &window)
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

    bool VulkanContext::InitializeSurface(const Window &window)
    {
        Window::CreateVulkanSurface(window, m_instance, &m_surface);
        return true;
    }

    bool VulkanContext::InitializePhysicalDevice()
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

            VkPhysicalDeviceShaderDrawParameterFeatures drawParamsFeatures;
            drawParamsFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETER_FEATURES;
            drawParamsFeatures.pNext = nullptr;
            drawParamsFeatures.shaderDrawParameters = VK_TRUE;

            VkPhysicalDeviceVulkan12Features deviceFeatures12;
            deviceFeatures12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
            deviceFeatures12.pNext = &drawParamsFeatures;

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
            allowed &= (deviceFeatures.samplerAnisotropy == VK_TRUE);

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

        vkGetPhysicalDeviceProperties(m_physicalDevice, &m_deviceProperties);
        std::cout << "Selected ";
        std::cout << m_deviceProperties.deviceName;
        std::cout << " for rendering.";
        std::cout << std::endl;
        return true;
    }

    bool VulkanContext::InitializeLogicalDevice()
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

        VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{};
        bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
        bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;
        bufferDeviceAddressFeatures.pNext = nullptr;

        VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures{};
        descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
        descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        descriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
        descriptorIndexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
        descriptorIndexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
        descriptorIndexingFeatures.pNext = &bufferDeviceAddressFeatures;

        VkPhysicalDeviceFeatures2 deviceFeatures2{};
        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures2.features.samplerAnisotropy = VK_TRUE;
        deviceFeatures2.pNext = &descriptorIndexingFeatures;

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pNext = &deviceFeatures2;

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        createInfo.enabledExtensionCount = static_cast<uint32_t>(c_deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = c_deviceExtensions.data();

        if (c_validationLayersEnabled)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(c_validationLayers.size());
            createInfo.ppEnabledLayerNames = c_validationLayers.data();
            std::cout << "Validation layers enabled." << std::endl;
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }

        VK_CHECK(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device));

        assert(families.graphicsFamily.has_value() && "No graphics family index.");

        vkGetDeviceQueue(m_device, families.graphicsFamily.value(), 0, &m_graphicsQueue);
        vkGetDeviceQueue(m_device, families.presentFamily.value(), 0, &m_presentQueue);
        vkGetDeviceQueue(m_device, families.graphicsFamily.value(), 0, &m_computeQueue);

        return true;
    }

    bool VulkanContext::InitializeAllocator()
    {
        VmaAllocatorCreateInfo allocatorCreateInfo = {};
        allocatorCreateInfo.physicalDevice = m_physicalDevice;
        allocatorCreateInfo.device = m_device;
        allocatorCreateInfo.instance = m_instance;
        allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

        VK_CHECK(vmaCreateAllocator(&allocatorCreateInfo, &m_allocator));

        return true;
    }

    bool VulkanContext::CheckDeviceExtensionSupport(VkPhysicalDevice device)
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

    bool VulkanContext::CheckValidationLayerSupport()
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

    void VulkanContext::Destroy()
    {
        vmaDestroyAllocator(m_allocator);
        vkDestroyDevice(m_device, nullptr);
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        vkDestroyInstance(m_instance, nullptr);
    }
}