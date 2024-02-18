#include "Vultron/SceneRenderer.h"

#include "Vultron/Vulkan/Debug.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <cstdint>
#include <algorithm>
#include <vector>
#include <iostream>
#include <set>
#include <limits>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Vultron
{
    bool VulkanBackend::Initialize(const Window &window)
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

        if (!InitializeFramebuffers())
        {
            std::cerr << "Faild to initialize framebuffers." << std::endl;
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

        if (!InitializeSyncObjects())
        {
            std::cerr << "Faild to initialize sync objects." << std::endl;
            return false;
        }

        if (!InitializeGeometry())
        {
            std::cerr << "Faild to initialize geometry." << std::endl;
            return false;
        }

        if (!InitializeUniformBuffers())
        {
            std::cerr << "Faild to initialize uniform buffers." << std::endl;
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

    bool VulkanBackend::InitializeInstance(const Window &window)
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

    bool VulkanBackend::InitializeSurface(const Window &window)
    {
        Window::CreateVulkanSurface(window, m_instance, &m_surface);
        return true;
    }

    bool VulkanBackend::InitializePhysicalDevice()
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

    bool VulkanBackend::InitializeLogicalDevice()
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
        VkPhysicalDeviceFeatures2 deviceFeatures2{};
        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{};
        bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
        bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;
        deviceFeatures2.pNext = &bufferDeviceAddressFeatures;

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

    bool VulkanBackend::InitializeSwapChain(uint32_t width, uint32_t height)
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

    bool VulkanBackend::InitializeImageViews()
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

    bool VulkanBackend::InitializeRenderPass()
    {
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

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        VK_CHECK(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass));

        return true;
    }

    bool VulkanBackend::InitializeDescriptorSetLayout()
    {
        VkDescriptorSetLayoutBinding layoutBinding = {};
        layoutBinding.binding = 0;
        layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        layoutBinding.descriptorCount = 1;
        layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        createInfo.bindingCount = 1;
        createInfo.pBindings = &layoutBinding;

        VK_CHECK(vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_descriptorSetLayout));

        return true;
    }

    bool VulkanBackend::InitializeGraphicsPipeline()
    {
        // Shader
        m_vertexShader = VulkanShader::CreateFromFile(m_device, "C:/dev/repos/vultron/Vultron/assets/shaders/triangle_vert.spv");
        m_fragmentShader = VulkanShader::CreateFromFile(m_device, "C:/dev/repos/vultron/Vultron/assets/shaders/triangle_frag.spv");

        VkPipelineShaderStageCreateInfo shaderStages[] = {
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = m_vertexShader->GetShaderModule(),
                .pName = "main",
            },
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = m_fragmentShader->GetShaderModule(),
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
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_pipelineLayout;
        pipelineInfo.renderPass = m_renderPass;
        pipelineInfo.subpass = 0;

        VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline));

        return true;
    }

    bool VulkanBackend::InitializeFramebuffers()
    {
        m_swapChainFramebuffers.resize(m_swapChainImageViews.size());

        for (size_t i = 0; i < m_swapChainImageViews.size(); i++)
        {
            VkImageView attachments[] = {
                m_swapChainImageViews[i],
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = m_swapChainExtent.width;
            framebufferInfo.height = m_swapChainExtent.height;
            framebufferInfo.layers = 1;

            VK_CHECK(vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_swapChainFramebuffers[i]));
        }

        return true;
    }

    bool VulkanBackend::InitializeCommandPool()
    {
        VkUtil::QueueFamilies families = VkUtil::QueryQueueFamilies(m_physicalDevice, m_surface);

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = families.graphicsFamily.value();
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VK_CHECK(vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool));

        return true;
    }

    bool VulkanBackend::InitializeCommandBuffer()
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

    bool VulkanBackend::InitializeSyncObjects()
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

    bool VulkanBackend::InitializeAllocator()
    {
        VmaAllocatorCreateInfo allocatorCreateInfo = {};
        allocatorCreateInfo.physicalDevice = m_physicalDevice;
        allocatorCreateInfo.device = m_device;
        allocatorCreateInfo.instance = m_instance;
        allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

        VK_CHECK(vmaCreateAllocator(&allocatorCreateInfo, &m_allocator));

        return true;
    }

    bool VulkanBackend::InitializeGeometry()
    {
        const std::vector<StaticMeshVertex> vertices = {
            {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}}};

        const std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};

        m_mesh = VulkanMesh::Create(m_device, m_commandPool, m_graphicsQueue, m_allocator, vertices, indices);

        return true;
    }

    bool VulkanBackend::InitializeUniformBuffers()
    {
        size_t size = sizeof(UniformBuffer);

        for (size_t i = 0; i < c_frameOverlap; i++)
        {
            m_uniformBuffers[i] = VulkanBuffer::Create(m_allocator, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, size, VMA_MEMORY_USAGE_CPU_TO_GPU);
            m_uniformBuffers[i].Map(m_allocator);
        }

        return true;
    }

    bool VulkanBackend::InitializeDescriptorPool()
    {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = static_cast<uint32_t>(c_frameOverlap);

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = static_cast<uint32_t>(c_frameOverlap);

        VK_CHECK(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool));

        return true;
    }

    bool VulkanBackend::InitializeDescriptorSets()
    {
        std::array<VkDescriptorSetLayout, c_frameOverlap> layouts = {m_descriptorSetLayout, m_descriptorSetLayout};
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(c_frameOverlap);
        allocInfo.pSetLayouts = layouts.data();

        VK_CHECK(vkAllocateDescriptorSets(m_device, &allocInfo, m_descriptorSets));

        for (size_t i = 0; i < c_frameOverlap; i++)
        {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = m_uniformBuffers[i].GetBuffer();
            bufferInfo.offset = 0;
            bufferInfo.range = m_uniformBuffers[i].GetSize();

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = m_descriptorSets[i];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;

            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;

            descriptorWrite.pBufferInfo = &bufferInfo;

            vkUpdateDescriptorSets(m_device, 1, &descriptorWrite, 0, nullptr);
        }

        return true;
    }

    bool VulkanBackend::InitializeDebugMessenger()
    {
        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = VulkanDebugCallback;
        VK_CHECK(CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger));

        return true;
    }

    void VulkanBackend::RecreateSwapChain(uint32_t width, uint32_t height)
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

    bool VulkanBackend::CheckDeviceExtensionSupport(VkPhysicalDevice device)
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

    bool VulkanBackend::CheckValidationLayerSupport()
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

    void VulkanBackend::BeginFrame()
    {
    }

    void VulkanBackend::EndFrame()
    {
        Draw();
    }

    void VulkanBackend::WriteCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
    {
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

        VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

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

        VkBuffer vertexBuffers[] = {m_mesh->GetVertexBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, m_mesh->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSets[m_currentFrameIndex], 0, nullptr);

        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(m_mesh->GetIndexCount()), 1, 0, 0, 0);

        vkCmdEndRenderPass(commandBuffer);

        VK_CHECK(vkEndCommandBuffer(commandBuffer));
    }

    void VulkanBackend::Draw()
    {
        constexpr uint32_t timeout = (std::numeric_limits<uint32_t>::max)();
        const uint32_t currentFrame = m_currentFrameIndex;

        vkWaitForFences(m_device, 1, &m_frames[currentFrame].inFlightFence, VK_TRUE, timeout);
        vkResetFences(m_device, 1, &m_frames[currentFrame].inFlightFence);

        uint32_t imageIndex;
        VK_CHECK(vkAcquireNextImageKHR(m_device, m_swapChain, timeout, m_frames[currentFrame].imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex));
        vkResetCommandBuffer(m_frames[currentFrame].commandBuffer, 0);
        WriteCommandBuffer(m_frames[currentFrame].commandBuffer, imageIndex);

        static std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
        std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(now - start).count();

        UniformBuffer ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj = glm::perspective(glm::radians(45.0f), (float)m_swapChainExtent.width / (float)m_swapChainExtent.height, 0.1f, 10.0f);
        ubo.proj[1][1] *= -1;

        std::memcpy(m_uniformBuffers[currentFrame].GetMapped<UniformBuffer>(), &ubo, sizeof(ubo));

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {m_frames[currentFrame].imageAvailableSemaphore};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_frames[currentFrame].commandBuffer;

        VkSemaphore signalSemaphores[] = {m_frames[currentFrame].renderFinishedSemaphore};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_frames[currentFrame].inFlightFence));

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

    void VulkanBackend::Shutdown()
    {
        vkDeviceWaitIdle(m_device);

        for (size_t i = 0; i < c_frameOverlap; i++)
        {
            vkDestroySemaphore(m_device, m_frames[i].imageAvailableSemaphore, nullptr);
            vkDestroySemaphore(m_device, m_frames[i].renderFinishedSemaphore, nullptr);
            vkDestroyFence(m_device, m_frames[i].inFlightFence, nullptr);

            vkFreeCommandBuffers(m_device, m_commandPool, 1, &m_frames[i].commandBuffer);
        }

        for (size_t i = 0; i < c_frameOverlap; i++)
        {
            m_uniformBuffers[i].Unmap(m_allocator);
            m_uniformBuffers[i].Destroy(m_allocator);
        }

        m_mesh->Destroy(m_allocator);
        m_vertexShader->Destroy(m_device);
        m_fragmentShader->Destroy(m_device);

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