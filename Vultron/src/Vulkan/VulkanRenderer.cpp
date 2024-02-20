#include "Vultron/SceneRenderer.h"

#include "Vultron/Vulkan/Debug.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cstdint>
#include <chrono>
#include <iostream>
#include <limits>
#include <vector>
#include <set>
#include <string>

#ifndef VLT_ASSETS_DIR
#define VLT_ASSETS_DIR "assets"
#endif

namespace Vultron
{
    constexpr uint32_t c_numInstances = 10000;
    constexpr uint32_t c_numPerRow = 100;
    constexpr float c_spacing = 3.0f;

    std::array<InstanceData, c_numInstances> instanceBufferData;

    bool VulkanRenderer::Initialize(const Window &window)
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

        const auto [width, height] = window.GetExtent();
        if (!InitializeSwapChain(width, height))
        {
            std::cerr << "Faild to initialize swap chain." << std::endl;
            return false;
        }

        if (!InitializeImageViews())
        {
            std::cerr << "Faild to initialize image views." << std::endl;
            return false;
        }

        if (!InitializeRenderPass())
        {
            std::cerr << "Faild to initialize render pass." << std::endl;
            return false;
        }

        if (!InitializeDescriptorSetLayout())
        {
            std::cerr << "Faild to initialize descriptor set layout." << std::endl;
            return false;
        }

        if (!InitializeGraphicsPipeline())
        {
            std::cerr << "Faild to initialize graphics pipeline." << std::endl;
            return false;
        }

        if (!InitializeCommandPool())
        {
            std::cerr << "Faild to initialize command pool." << std::endl;
            return false;
        }

        if (!InitializeCommandBuffer())
        {
            std::cerr << "Faild to initialize command buffer." << std::endl;
            return false;
        }

        if (!InitializeDepthBuffer())
        {
            std::cerr << "Faild to initialize depth buffer." << std::endl;
            return false;
        }

        if (!InitializeFramebuffers())
        {
            std::cerr << "Faild to initialize framebuffers." << std::endl;
            return false;
        }

        if (!InitializeSyncObjects())
        {
            std::cerr << "Faild to initialize sync objects." << std::endl;
            return false;
        }

        if (!InitializeTestResources())
        {
            std::cerr << "Faild to initialize geometry." << std::endl;
            return false;
        }

        if (!InitializeSamplers())
        {
            std::cerr << "Faild to initialize sampler." << std::endl;
            return false;
        }

        if (!InitializeUniformBuffers())
        {
            std::cerr << "Faild to initialize uniform buffers." << std::endl;
            return false;
        }

        if (!InitializeInstanceBuffer())
        {
            std::cerr << "Faild to initialize instance buffer." << std::endl;
            return false;
        }

        if (!InitializeDescriptorPool())
        {
            std::cerr << "Faild to initialize descriptor pool." << std::endl;
            return false;
        }

        if (!InitializeDescriptorSets())
        {
            std::cerr << "Faild to initialize descriptor sets." << std::endl;
            return false;
        }

        return true;
    }

    bool VulkanRenderer::InitializeInstance(const Window &window)
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

    bool VulkanRenderer::InitializeSurface(const Window &window)
    {
        Window::CreateVulkanSurface(window, m_instance, &m_surface);
        return true;
    }

    bool VulkanRenderer::InitializePhysicalDevice()
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

    bool VulkanRenderer::InitializeLogicalDevice()
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
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }

        VK_CHECK(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device));

        assert(families.graphicsFamily.has_value() && "No graphics family index.");

        vkGetDeviceQueue(m_device, families.graphicsFamily.value(), 0, &m_graphicsQueue);
        vkGetDeviceQueue(m_device, families.presentFamily.value(), 0, &m_presentQueue);

        return true;
    }

    bool VulkanRenderer::InitializeSwapChain(uint32_t width, uint32_t height)
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

        if (capabilities.currentExtent.width != (std::numeric_limits<uint32_t>::max)())
        {
            extent = capabilities.currentExtent;
        }
        else
        {
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
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

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

    bool VulkanRenderer::InitializeImageViews()
    {
        m_swapChainImageViews.resize(m_swapChainImages.size());

        for (size_t i = 0; i < m_swapChainImages.size(); i++)
        {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = m_swapChainImages[i];

            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = m_swapChainImageFormat;

            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            VK_CHECK(vkCreateImageView(m_device, &createInfo, nullptr, &m_swapChainImageViews[i]));
        }

        return true;
    }

    bool VulkanRenderer::InitializeRenderPass()
    {
        // Color attachment
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = m_swapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Depth attachment
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = VK_FORMAT_D32_SFLOAT;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcAccessMask = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        VK_CHECK(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass));

        return true;
    }

    bool VulkanRenderer::InitializeDescriptorSetLayout()
    {
        std::array<VkDescriptorSetLayoutBinding, 3> layoutBindings = {};

        VkDescriptorSetLayoutBinding &uniformBinding = layoutBindings[0];
        uniformBinding.binding = 0;
        uniformBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uniformBinding.descriptorCount = 1;
        uniformBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding &instanceBinding = layoutBindings[1];
        instanceBinding.binding = 1;
        instanceBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        instanceBinding.descriptorCount = 1;
        instanceBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutBinding &samplerBinding = layoutBindings[2];
        samplerBinding.binding = 2;
        samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerBinding.descriptorCount = 1;
        samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        createInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
        createInfo.pBindings = layoutBindings.data();

        VK_CHECK(vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_descriptorSetLayout));

        return true;
    }

    bool VulkanRenderer::InitializeGraphicsPipeline()
    {
        // Shader
        m_vertexShader = VulkanShader::CreateFromFile({.device = m_device, .filepath = std::string(VLT_ASSETS_DIR) + "/shaders/triangle.vert.spv"});
        m_fragmentShader = VulkanShader::CreateFromFile({.device = m_device, .filepath = std::string(VLT_ASSETS_DIR) + "/shaders/triangle.frag.spv"});

        VkPipelineShaderStageCreateInfo shaderStages[] = {
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = m_vertexShader.GetShaderModule(),
                .pName = "main",
            },
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = m_fragmentShader.GetShaderModule(),
                .pName = "main",
            }};

        auto vertexBindingDesc = StaticMeshVertex::GetBindingDescription();
        auto vertexAttributeDescs = StaticMeshVertex::GetAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &vertexBindingDesc;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributeDescs.size());
        vertexInputInfo.pVertexAttributeDescriptions = vertexAttributeDescs.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)m_swapChainExtent.width;
        viewport.height = (float)m_swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = m_swapChainExtent;

        const std::array<VkDynamicState, 2> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR};

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout));

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_pipelineLayout;
        pipelineInfo.renderPass = m_renderPass;
        pipelineInfo.subpass = 0;

        VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline));

        return true;
    }

    bool VulkanRenderer::InitializeFramebuffers()
    {
        m_swapChainFramebuffers.resize(m_swapChainImageViews.size());

        for (size_t i = 0; i < m_swapChainImageViews.size(); i++)
        {
            std::array<VkImageView, 2> attachments = {
                m_swapChainImageViews[i],
                m_depthImage.GetImageView()};

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = m_swapChainExtent.width;
            framebufferInfo.height = m_swapChainExtent.height;
            framebufferInfo.layers = 1;

            VK_CHECK(vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_swapChainFramebuffers[i]));
        }

        return true;
    }

    bool VulkanRenderer::InitializeCommandPool()
    {
        VkUtil::QueueFamilies families = VkUtil::QueryQueueFamilies(m_physicalDevice, m_surface);

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = families.graphicsFamily.value();
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VK_CHECK(vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool));

        return true;
    }

    bool VulkanRenderer::InitializeCommandBuffer()
    {
        for (size_t i = 0; i < c_frameOverlap; i++)
        {
            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.commandPool = m_commandPool;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = 1;

            VK_CHECK(vkAllocateCommandBuffers(m_device, &allocInfo, &m_frames[i].commandBuffer));
        }

        return true;
    }

    bool VulkanRenderer::InitializeSyncObjects()
    {
        for (size_t i = 0; i < c_frameOverlap; i++)
        {
            VkSemaphoreCreateInfo semaphoreInfo{};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

            VK_CHECK(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_frames[i].imageAvailableSemaphore));
            VK_CHECK(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_frames[i].renderFinishedSemaphore));

            VkFenceCreateInfo fenceInfo{};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

            VK_CHECK(vkCreateFence(m_device, &fenceInfo, nullptr, &m_frames[i].inFlightFence));
        }

        return true;
    }

    bool VulkanRenderer::InitializeAllocator()
    {
        VmaAllocatorCreateInfo allocatorCreateInfo = {};
        allocatorCreateInfo.physicalDevice = m_physicalDevice;
        allocatorCreateInfo.device = m_device;
        allocatorCreateInfo.instance = m_instance;
        allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

        VK_CHECK(vmaCreateAllocator(&allocatorCreateInfo, &m_allocator));

        return true;
    }

    bool VulkanRenderer::InitializeTestResources()
    {
        m_mesh = VulkanMesh::CreateFromFile(
            {.device = m_device,
             .commandPool = m_commandPool,
             .queue = m_graphicsQueue,
             .allocator = m_allocator,
             .filepath = std::string(VLT_ASSETS_DIR) + "/meshes/DamagedHelmet.dat"});

        m_texture = VulkanImage::CreateFromFile(
            {.device = m_device,
             .commandPool = m_commandPool,
             .queue = m_graphicsQueue,
             .allocator = m_allocator,
             .format = VK_FORMAT_R8G8B8A8_SRGB,
             .filepath = std::string(VLT_ASSETS_DIR) + "/textures/helmet_albedo.jpg"});

        return true;
    }

    bool VulkanRenderer::InitializeSamplers()
    {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = m_deviceProperties.limits.maxSamplerAnisotropy;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        VK_CHECK(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_textureSampler));

        return true;
    }

    bool VulkanRenderer::InitializeDepthBuffer()
    {
        VkFormat depthFormat = VkUtil::FindDepthFormat(m_physicalDevice);

        m_depthImage = VulkanImage::Create(
            {.device = m_device,
             .commandPool = m_commandPool,
             .queue = m_graphicsQueue,
             .allocator = m_allocator,
             .data = nullptr,
             .info = {
                 .width = m_swapChainExtent.width,
                 .height = m_swapChainExtent.height,
                 .depth = 1,
                 .format = depthFormat,
             },
             .aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT,
             .additionalUsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT});

        m_depthImage.TransitionLayout(m_device, m_commandPool, m_graphicsQueue, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

        return true;
    }

    bool VulkanRenderer::InitializeUniformBuffers()
    {
        size_t size = sizeof(UniformBufferData);

        for (size_t i = 0; i < c_frameOverlap; i++)
        {
            VulkanBuffer &uniformBuffer = m_frames[i].uniformBuffer;
            uniformBuffer = VulkanBuffer::Create({.allocator = m_allocator, .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, .size = size, .allocationUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});
            uniformBuffer.Map(m_allocator);
        }

        m_uniformBufferData.lightDir = glm::vec3(0.0f, 0.0f, 1.0f);
        m_uniformBufferData.proj = glm::perspective(glm::radians(45.0f), (float)m_swapChainExtent.width / (float)m_swapChainExtent.height, 0.1f, 10000.0f);
        m_uniformBufferData.proj[1][1] *= -1;
        return true;
    }

    bool VulkanRenderer::InitializeInstanceBuffer()
    {
        // Initial matrix should be rotated around Y axis
        const glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        for (uint32_t i = 0; i < c_numInstances; i++)
        {
            const float x = (i % c_numPerRow) * c_spacing - (c_numPerRow * c_spacing) / 2.0f;
            const float y = (i / c_numPerRow) * c_spacing * -1.0f;
            const glm::mat4 model = rot * glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f)) * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f + i), glm::vec3(0.0f, 0.0f, 1.0f));
            instanceBufferData[i] = {model};
        }

        const size_t size = sizeof(InstanceData) * instanceBufferData.size();
        for (size_t i = 0; i < c_frameOverlap; i++)
        {
            m_frames[i].instanceCount = static_cast<uint32_t>(instanceBufferData.size());
            VulkanBuffer &instanceBuffer = m_frames[i].instanceBuffer;
            instanceBuffer = VulkanBuffer::Create({.allocator = m_allocator, .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, .size = size, .allocationUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});
            instanceBuffer.Map(m_allocator);
            instanceBuffer.CopyData(instanceBufferData.data(), size);
        }

        return true;
    }

    bool VulkanRenderer::InitializeDescriptorPool()
    {
        std::array<VkDescriptorPoolSize, 3> poolSizes = {};

        VkDescriptorPoolSize &uniformPoolSize = poolSizes[0];
        uniformPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uniformPoolSize.descriptorCount = static_cast<uint32_t>(c_frameOverlap);

        VkDescriptorPoolSize &instancePoolSize = poolSizes[1];
        instancePoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        instancePoolSize.descriptorCount = static_cast<uint32_t>(c_frameOverlap);

        VkDescriptorPoolSize &samplerPoolSize = poolSizes[2];
        samplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerPoolSize.descriptorCount = static_cast<uint32_t>(c_frameOverlap);

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(c_frameOverlap);

        VK_CHECK(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool));

        return true;
    }

    bool VulkanRenderer::InitializeDescriptorSets()
    {
        std::array<VkDescriptorSetLayout, c_frameOverlap> layouts = {m_descriptorSetLayout, m_descriptorSetLayout};
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(c_frameOverlap);
        allocInfo.pSetLayouts = layouts.data();

        std::array<VkDescriptorSet, c_frameOverlap> descriptorSets;
        VK_CHECK(vkAllocateDescriptorSets(m_device, &allocInfo, descriptorSets.data()));

        for (size_t i = 0; i < c_frameOverlap; i++)
        {
            m_frames[i].descriptorSet = descriptorSets[i];

            VulkanBuffer &uniformBuffer = m_frames[i].uniformBuffer;
            VkDescriptorBufferInfo uniformBufferInfo{};
            uniformBufferInfo.buffer = uniformBuffer.GetBuffer();
            uniformBufferInfo.range = uniformBuffer.GetSize();
            uniformBufferInfo.offset = 0;

            VulkanBuffer &instanceBuffer = m_frames[i].instanceBuffer;
            VkDescriptorBufferInfo instanceBufferInfo{};
            instanceBufferInfo.buffer = instanceBuffer.GetBuffer();
            instanceBufferInfo.range = instanceBuffer.GetSize();
            instanceBufferInfo.offset = 0;

            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = m_texture.GetImageView();
            imageInfo.sampler = m_textureSampler;

            std::array<VkWriteDescriptorSet, 3> descriptorWrites = {};
            VkDescriptorSet &descriptorSet = m_frames[i].descriptorSet;

            VkWriteDescriptorSet &uniformBufferWrite = descriptorWrites[0];
            uniformBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            uniformBufferWrite.dstSet = descriptorSet;
            uniformBufferWrite.dstBinding = 0;
            uniformBufferWrite.dstArrayElement = 0;
            uniformBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uniformBufferWrite.descriptorCount = 1;
            uniformBufferWrite.pBufferInfo = &uniformBufferInfo;

            VkWriteDescriptorSet &instanceBufferWrite = descriptorWrites[1];
            instanceBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            instanceBufferWrite.dstSet = descriptorSet;
            instanceBufferWrite.dstBinding = 1;
            instanceBufferWrite.dstArrayElement = 0;
            instanceBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            instanceBufferWrite.descriptorCount = 1;
            instanceBufferWrite.pBufferInfo = &instanceBufferInfo;

            VkWriteDescriptorSet &samplerWrite = descriptorWrites[2];
            samplerWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            samplerWrite.dstSet = descriptorSet;
            samplerWrite.dstBinding = 2;
            samplerWrite.dstArrayElement = 0;
            samplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            samplerWrite.descriptorCount = 1;
            samplerWrite.pImageInfo = &imageInfo;

            vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }

        return true;
    }

    bool VulkanRenderer::InitializeDebugMessenger()
    {
        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = VulkanDebugCallback;
        VK_CHECK(CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger));

        return true;
    }

    void VulkanRenderer::RecreateSwapChain(uint32_t width, uint32_t height)
    {
        vkDeviceWaitIdle(m_device);

        for (auto framebuffer : m_swapChainFramebuffers)
        {
            vkDestroyFramebuffer(m_device, framebuffer, nullptr);
        }

        for (const auto &frame : m_frames)
        {
            vkFreeCommandBuffers(m_device, m_commandPool, 1, &frame.commandBuffer);
        }

        vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);

        for (auto imageView : m_swapChainImageViews)
        {
            vkDestroyImageView(m_device, imageView, nullptr);
        }

        vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);

        InitializeSwapChain(width, height);
        InitializeImageViews();
        InitializeRenderPass();
        InitializeGraphicsPipeline();
        InitializeFramebuffers();
        InitializeCommandBuffer();
    }

    bool VulkanRenderer::CheckDeviceExtensionSupport(VkPhysicalDevice device)
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

    bool VulkanRenderer::CheckValidationLayerSupport()
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

    void VulkanRenderer::BeginFrame()
    {
    }

    void VulkanRenderer::EndFrame()
    {
        Draw();
    }

    void VulkanRenderer::WriteCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
    {
        const FrameData &frame = m_frames[m_currentFrameIndex];

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;                  // Optional
        beginInfo.pInheritanceInfo = nullptr; // Optional

        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_renderPass;
        renderPassInfo.framebuffer = m_swapChainFramebuffers[imageIndex];

        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = m_swapChainExtent;

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)m_swapChainExtent.width;
        viewport.height = (float)m_swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = m_swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        VkBuffer vertexBuffers[] = {m_mesh.GetVertexBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, m_mesh.GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &frame.descriptorSet, 0, nullptr);

        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(m_mesh.GetIndexCount()), frame.instanceCount, 0, 0, 0);

        vkCmdEndRenderPass(commandBuffer);

        VK_CHECK(vkEndCommandBuffer(commandBuffer));
    }

    void VulkanRenderer::Draw()
    {
        constexpr uint32_t timeout = (std::numeric_limits<uint32_t>::max)();
        const uint32_t currentFrame = m_currentFrameIndex;
        const FrameData &frame = m_frames[currentFrame];

        vkWaitForFences(m_device, 1, &frame.inFlightFence, VK_TRUE, timeout);
        vkResetFences(m_device, 1, &frame.inFlightFence);

        uint32_t imageIndex;
        VK_CHECK(vkAcquireNextImageKHR(m_device, m_swapChain, timeout, frame.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex));
        vkResetCommandBuffer(frame.commandBuffer, 0);
        WriteCommandBuffer(frame.commandBuffer, imageIndex);

        static std::chrono::high_resolution_clock::time_point lastTime = std::chrono::high_resolution_clock::now();
        const auto currentTime = std::chrono::high_resolution_clock::now();
        const float dt = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
        lastTime = currentTime;

        // Uniform buffer
        UniformBufferData ubo = m_uniformBufferData;
        const glm::vec3 viewPos = glm::vec3(0.0f, 20.0f, 10.0f);
        const glm::vec3 viewDir = glm::normalize(glm::vec3(0.0f, -1.0f, -0.3f));
        ubo.view = glm::lookAt(viewPos, viewPos + viewDir, glm::vec3(0.0f, 0.0f, 1.0f));
        frame.uniformBuffer.CopyData(&ubo, sizeof(ubo));

        // Instance buffer
        for (uint32_t i = 0; i < c_numInstances; i++)
        {
            instanceBufferData[i].model *= glm::rotate(glm::mat4(1.0f), dt * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        }
        frame.instanceBuffer.CopyData(instanceBufferData.data(), sizeof(InstanceData) * instanceBufferData.size());

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {frame.imageAvailableSemaphore};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &frame.commandBuffer;

        VkSemaphore signalSemaphores[] = {frame.renderFinishedSemaphore};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, frame.inFlightFence));

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {m_swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        VK_CHECK(vkQueuePresentKHR(m_presentQueue, &presentInfo));

        m_currentFrameIndex = (currentFrame + 1) % c_frameOverlap;
    }

    void VulkanRenderer::Shutdown()
    {
        vkDeviceWaitIdle(m_device);

        for (size_t i = 0; i < c_frameOverlap; i++)
        {
            vkDestroySemaphore(m_device, m_frames[i].imageAvailableSemaphore, nullptr);
            vkDestroySemaphore(m_device, m_frames[i].renderFinishedSemaphore, nullptr);
            vkDestroyFence(m_device, m_frames[i].inFlightFence, nullptr);

            vkFreeCommandBuffers(m_device, m_commandPool, 1, &m_frames[i].commandBuffer);

            m_frames[i].uniformBuffer.Unmap(m_allocator);
            m_frames[i].uniformBuffer.Destroy(m_allocator);

            m_frames[i].instanceBuffer.Unmap(m_allocator);
            m_frames[i].instanceBuffer.Destroy(m_allocator);
        }

        vkDestroySampler(m_device, m_textureSampler, nullptr);

        m_depthImage.Destroy(m_device, m_allocator);
        m_texture.Destroy(m_device, m_allocator);
        m_mesh.Destroy(m_allocator);
        m_vertexShader.Destroy(m_device);
        m_fragmentShader.Destroy(m_device);

        vmaDestroyAllocator(m_allocator);

        vkDestroyCommandPool(m_device, m_commandPool, nullptr);

        for (auto framebuffer : m_swapChainFramebuffers)
        {
            vkDestroyFramebuffer(m_device, framebuffer, nullptr);
        }

        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
        vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);

        for (auto imageView : m_swapChainImageViews)
        {
            vkDestroyImageView(m_device, imageView, nullptr);
        }

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