#include "Vultron/Vulkan/VulkanRenderer.h"

#include "Vultron/Vulkan/VulkanUtils.h"
#include "Vultron/Vulkan/VulkanInitializers.h"
#include "Vultron/Vulkan/Debug.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cstdint>
#include <chrono>
#include <iostream>
#include <limits>
#include <vector>
#include <set>
#include <string>
#include <random>

namespace Vultron
{
    bool VulkanRenderer::Initialize(const Window &window)
    {
        if (!m_context.Initialize(window))
        {
            std::cerr << "Faild to initialize context." << std::endl;
            return false;
        }

        if (c_validationLayersEnabled && !InitializeDebugMessenger())
        {
            std::cerr << "Faild to initialize debug messages." << std::endl;
            return false;
        }

        const auto [width, height] = window.GetExtent();
        if (!m_swapchain.Initialize(m_context, width, height))
        {
            std::cerr << "Faild to initialize swap chain." << std::endl;
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

        if (!InitializeSkeletalBuffers())
        {
            std::cerr << "Faild to initialize skeletal buffers." << std::endl;
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

        if (!InitializeShadowMap())
        {
            std::cerr << "Faild to initialize shadow framebuffers." << std::endl;
            return false;
        }

        if (!InitializeSyncObjects())
        {
            std::cerr << "Faild to initialize sync objects." << std::endl;
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

        if (!InitializeInstanceBuffers())
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

        if (!InitializeTestResources())
        {
            std::cerr << "Faild to initialize geometry." << std::endl;
            return false;
        }

        return true;
    }

    void VulkanRenderer::PostInitialize()
    {
        struct BoneUploaded
        {
            glm::mat4 offset = glm::mat4(1.0f);
            int32_t parentID = -1;
            float padding[3];
        };

        std::vector<BoneUploaded> bones{};
        bones.reserve(m_bones.size());
        for (size_t i = 0; i < m_bones.size(); i++)
        {
            bones.push_back({.offset = glm::transpose(m_bones[i].offset), .parentID = m_bones[i].parentID});
        }

        if (bones.size() > 0)
        {
            m_boneBuffer.UploadStaged(m_context.GetDevice(), m_commandPool, m_context.GetGraphicsQueue(), m_context.GetAllocator(), bones.data(), sizeof(bones[0]) * bones.size());
        }

        if (m_animationFrames.size() > 0)
        {
            m_animationFrameBuffer.UploadStaged(m_context.GetDevice(), m_commandPool, m_context.GetGraphicsQueue(), m_context.GetAllocator(), m_animationFrames.data(), sizeof(m_animationFrames[0]) * m_animationFrames.size());
        }
    }

    bool VulkanRenderer::InitializeRenderPass()
    {
        m_renderPass = VulkanRenderPass::Create(
            m_context,
            {.attachments = {
                 // Color attachment
                 {
                     .format = m_swapchain.GetImageFormat(),
                 },
                 // Depth attachment
                 {
                     .type = VulkanRenderPass::AttachmentType::Depth,
                     .format = VK_FORMAT_D32_SFLOAT,
                     .samples = VK_SAMPLE_COUNT_1_BIT,
                     .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                     .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                     .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                     .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                     .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                     .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}}});

        m_shadowPass = VulkanRenderPass::Create(
            m_context,
            {
                .attachments = {
                    // Depth attachment
                    {
                        .type = VulkanRenderPass::AttachmentType::Depth,
                        .format = VK_FORMAT_D32_SFLOAT,
                        .samples = VK_SAMPLE_COUNT_1_BIT,
                        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}},
                .dependencies = {
                    {
                        .srcSubpass = VK_SUBPASS_EXTERNAL,
                        .dstSubpass = 0,
                        .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                        .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
                        .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
                    },
                    {
                        .srcSubpass = 0,
                        .dstSubpass = VK_SUBPASS_EXTERNAL,
                        .srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                        .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
                    },
                },
            });

        return true;
    }

    bool VulkanRenderer::InitializeDescriptorSetLayout()
    {
        m_descriptorSetLayout = VkInit::CreateDescriptorSetLayout(
            m_context.GetDevice(),
            {{
                 // Scene data
                 .binding = 0,
                 .type = DescriptorType::UniformBuffer,
                 .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
             },
             {
                 // Instance data
                 .binding = 1,
                 .type = DescriptorType::StorageBuffer,
                 .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
             },
             {
                 // Shadow map
                 .binding = 2,
                 .type = DescriptorType::CombinedImageSampler,
                 .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
             }});

        m_skeletalSetLayout = VkInit::CreateDescriptorSetLayout(
            m_context.GetDevice(),
            {
                {
                    // Scene data
                    .binding = 0,
                    .type = DescriptorType::UniformBuffer,
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                },
                {
                    // Instance data
                    .binding = 1,
                    .type = DescriptorType::StorageBuffer,
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                },
                {
                    // Shadow map
                    .binding = 2,
                    .type = DescriptorType::CombinedImageSampler,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
                {
                    // Bone data
                    .binding = 3,
                    .type = DescriptorType::UniformBuffer,
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                },
                {
                    // Animation data
                    .binding = 4,
                    .type = DescriptorType::StorageBuffer,
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                },
                {
                    // Animation instance data
                    .binding = 5,
                    .type = DescriptorType::UniformBuffer,
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                },
            });

        return true;
    }

    bool VulkanRenderer::InitializeGraphicsPipeline()
    {
        // Shader
        m_staticVertexShader = VulkanShader::CreateFromFile({.device = m_context.GetDevice(), .filepath = std::string(VLT_ASSETS_DIR) + "/shaders/triangle.vert.spv"});
        m_skeletalVertexShader = VulkanShader::CreateFromFile({.device = m_context.GetDevice(), .filepath = std::string(VLT_ASSETS_DIR) + "/shaders/skeletal.vert.spv"});
        m_fragmentShader = VulkanShader::CreateFromFile({.device = m_context.GetDevice(), .filepath = std::string(VLT_ASSETS_DIR) + "/shaders/triangle.frag.spv"});

        auto materialBindings = std::vector<DescriptorSetLayoutBinding>{
            // Albedo
            {
                .binding = 0,
                .type = DescriptorType::CombinedImageSampler,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
            // Normal
            {
                .binding = 1,
                .type = DescriptorType::CombinedImageSampler,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
            // Roughness, metallic, AO
            {
                .binding = 2,
                .type = DescriptorType::CombinedImageSampler,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
        };

        m_staticPipeline = VulkanMaterialPipeline::Create(
            m_context, m_renderPass,
            {
                .vertexShader = m_staticVertexShader,
                .fragmentShader = m_fragmentShader,
                .sceneDescriptorSetLayout = m_descriptorSetLayout,
                .bindings = materialBindings,
                .vertexDescription = StaticMeshVertex::GetVertexDescription(),
            });

        m_skeletalPipeline = VulkanMaterialPipeline::Create(
            m_context, m_renderPass,
            {
                .vertexShader = m_skeletalVertexShader,
                .fragmentShader = m_fragmentShader,
                .sceneDescriptorSetLayout = m_skeletalSetLayout,
                .bindings = materialBindings,
                .vertexDescription = SkeletalMeshVertex::GetVertexDescription(),
            });

        // Shadow

        m_staticShadowVertexShader = VulkanShader::CreateFromFile({.device = m_context.GetDevice(), .filepath = std::string(VLT_ASSETS_DIR) + "/shaders/shadow.vert.spv"});
        m_shadowFragmentShader = VulkanShader::CreateFromFile({.device = m_context.GetDevice(), .filepath = std::string(VLT_ASSETS_DIR) + "/shaders/shadow.frag.spv"});

        m_staticShadowPipeline = VulkanMaterialPipeline::Create(
            m_context, m_shadowPass,
            {
                .vertexShader = m_staticShadowVertexShader,
                .fragmentShader = m_shadowFragmentShader,
                .sceneDescriptorSetLayout = m_descriptorSetLayout,
                .bindings = {},
                .vertexDescription = StaticMeshVertex::GetVertexDescription(),
            });

        return true;
    }

    bool VulkanRenderer::InitializeSkeletalBuffers()
    {
        m_boneBuffer = VulkanBuffer::Create({.allocator = m_context.GetAllocator(), .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, .size = sizeof(SkeletonBone) * c_maxBones, .allocationUsage = VMA_MEMORY_USAGE_GPU_ONLY});
        m_animationFrameBuffer = VulkanBuffer::Create({.allocator = m_context.GetAllocator(), .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, .size = sizeof(AnimationFrame) * c_maxAnimationFrames, .allocationUsage = VMA_MEMORY_USAGE_GPU_ONLY});

        return true;
    }

    bool VulkanRenderer::InitializeFramebuffers()
    {
        std::vector<VkFramebuffer> &framebuffers = m_swapchain.GetFramebuffers();
        const std::vector<VkImageView> &imageViews = m_swapchain.GetImageViews();
        framebuffers.resize(imageViews.size());

        for (size_t i = 0; i < imageViews.size(); i++)
        {
            std::array<VkImageView, 2> attachments = {
                imageViews[i],
                m_depthImage.GetImageView()};

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_renderPass.GetRenderPass();
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = m_swapchain.GetExtent().width;
            framebufferInfo.height = m_swapchain.GetExtent().height;
            framebufferInfo.layers = 1;

            VK_CHECK(vkCreateFramebuffer(m_context.GetDevice(), &framebufferInfo, nullptr, &framebuffers[i]));
        }

        return true;
    }

    bool VulkanRenderer::InitializeShadowMap()
    {
        m_shadowMap = VulkanImage::Create(
            {.device = m_context.GetDevice(),
             .commandPool = m_commandPool,
             .queue = m_context.GetGraphicsQueue(),
             .allocator = m_context.GetAllocator(),
             .info = {
                 .width = 2048,
                 .height = 2048,
                 .depth = 1,
                 .mipLevels = 1,
                 .format = VK_FORMAT_D32_SFLOAT,
             },
             .aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT,
             .additionalUsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT});

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_shadowPass.GetRenderPass();
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &m_shadowMap.GetImageView();
        framebufferInfo.width = m_shadowMap.GetInfo().width;
        framebufferInfo.height = m_shadowMap.GetInfo().height;
        framebufferInfo.layers = 1;

        VK_CHECK(vkCreateFramebuffer(m_context.GetDevice(), &framebufferInfo, nullptr, &m_shadowFramebuffer));

        return true;
    }

    bool VulkanRenderer::InitializeCommandPool()
    {
        VkUtil::QueueFamilies families = VkUtil::QueryQueueFamilies(m_context.GetPhysicalDevice(), m_context.GetSurface());

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = families.graphicsFamily.value();
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VK_CHECK(vkCreateCommandPool(m_context.GetDevice(), &poolInfo, nullptr, &m_commandPool));

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

            VK_CHECK(vkAllocateCommandBuffers(m_context.GetDevice(), &allocInfo, &m_frames[i].commandBuffer));
        }

        return true;
    }

    bool VulkanRenderer::InitializeSyncObjects()
    {
        for (size_t i = 0; i < c_frameOverlap; i++)
        {
            VkSemaphoreCreateInfo semaphoreInfo{};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

            VK_CHECK(vkCreateSemaphore(m_context.GetDevice(), &semaphoreInfo, nullptr, &m_frames[i].imageAvailableSemaphore));
            VK_CHECK(vkCreateSemaphore(m_context.GetDevice(), &semaphoreInfo, nullptr, &m_frames[i].renderFinishedSemaphore));

            VkFenceCreateInfo fenceInfo{};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

            VK_CHECK(vkCreateFence(m_context.GetDevice(), &fenceInfo, nullptr, &m_frames[i].inFlightFence));
        }

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
        samplerInfo.maxAnisotropy = m_context.GetDeviceProperties().limits.maxSamplerAnisotropy;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 10.0f;

        VK_CHECK(vkCreateSampler(m_context.GetDevice(), &samplerInfo, nullptr, &m_textureSampler));

        VkSamplerCreateInfo shadowSamplerInfo{};
        shadowSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        shadowSamplerInfo.magFilter = VK_FILTER_LINEAR;
        shadowSamplerInfo.minFilter = VK_FILTER_LINEAR;
        shadowSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        shadowSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        shadowSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        shadowSamplerInfo.anisotropyEnable = VK_FALSE;
        shadowSamplerInfo.maxAnisotropy = 1.0f;
        shadowSamplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        shadowSamplerInfo.unnormalizedCoordinates = VK_FALSE;
        shadowSamplerInfo.compareEnable = VK_FALSE;
        shadowSamplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        shadowSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        shadowSamplerInfo.mipLodBias = 0.0f;
        shadowSamplerInfo.minLod = 0.0f;
        shadowSamplerInfo.maxLod = 1.0f;

        VK_CHECK(vkCreateSampler(m_context.GetDevice(), &shadowSamplerInfo, nullptr, &m_shadowSampler));

        return true;
    }

    bool VulkanRenderer::InitializeDepthBuffer()
    {
        VkFormat depthFormat = VkUtil::FindDepthFormat(m_context.GetPhysicalDevice());

        m_depthImage = VulkanImage::Create(
            {.device = m_context.GetDevice(),
             .commandPool = m_commandPool,
             .queue = m_context.GetGraphicsQueue(),
             .allocator = m_context.GetAllocator(),
             .info = {
                 .width = m_swapchain.GetExtent().width,
                 .height = m_swapchain.GetExtent().height,
                 .depth = 1,
                 .format = depthFormat,
             },
             .aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT,
             .additionalUsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT});

        m_depthImage.TransitionLayout(m_context.GetDevice(), m_commandPool, m_context.GetGraphicsQueue(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

        return true;
    }

    bool VulkanRenderer::InitializeUniformBuffers()
    {
        size_t size = sizeof(UniformBufferData);

        for (size_t i = 0; i < c_frameOverlap; i++)
        {
            VulkanBuffer &uniformBuffer = m_frames[i].uniformBuffer;
            uniformBuffer = VulkanBuffer::Create({.allocator = m_context.GetAllocator(), .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, .size = size, .allocationUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});
            uniformBuffer.Map(m_context.GetAllocator());
        }

        m_uniformBufferData.proj = glm::perspective(glm::radians(35.0f), (float)m_swapchain.GetExtent().width / (float)m_swapchain.GetExtent().height, 0.1f, 10000.0f);
        // m_uniformBufferData.proj = glm::ortho(-1000.0f, 1000.0f, -1000.0f, 1000.0f, 0.1f, 1000.0f);
        m_uniformBufferData.proj[1][1] *= -1;

        m_uniformBufferData.lightDir = glm::normalize(glm::vec3(0.0f, -1.0f, 1.0f));
        m_uniformBufferData.lightColor = glm::vec3(1.0f, 1.0f, 1.0f) * 1.0f;
        glm::vec3 lightPos = glm::vec3(0.0f, 500.0f, -500.0f);
        glm::mat4 lightView = glm::lookAt(lightPos, lightPos + m_uniformBufferData.lightDir, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 lightProjection = glm::ortho(-1000.0f, 1000.0f, -1000.0f, 1000.0f, 0.1f, 1000.0f);
        lightProjection[1][1] *= -1;
        m_uniformBufferData.lightViewProjection = lightProjection * lightView;

        return true;
    }

    bool VulkanRenderer::InitializeInstanceBuffers()
    {
        constexpr size_t instancesSize = sizeof(InstanceData) * c_maxInstances;
        constexpr size_t skeletalInstancesSize = sizeof(SkeletalInstanceData) * c_maxSkeletalInstances;
        constexpr size_t animationInstancesSize = sizeof(AnimationInstanceData) * c_maxAnimationInstances;
        for (size_t i = 0; i < c_frameOverlap; i++)
        {
            VulkanBuffer &instanceBuffer = m_frames[i].staticInstanceBuffer;
            instanceBuffer = VulkanBuffer::Create({.allocator = m_context.GetAllocator(), .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, .size = instancesSize, .allocationUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});
            instanceBuffer.Map(m_context.GetAllocator());

            VulkanBuffer &skeletalInstanceBuffer = m_frames[i].skeletalInstanceBuffer;
            skeletalInstanceBuffer = VulkanBuffer::Create({.allocator = m_context.GetAllocator(), .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, .size = skeletalInstancesSize, .allocationUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});
            skeletalInstanceBuffer.Map(m_context.GetAllocator());

            VulkanBuffer &animationInstanceBuffer = m_frames[i].animationInstanceBuffer;
            animationInstanceBuffer = VulkanBuffer::Create({.allocator = m_context.GetAllocator(), .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, .size = animationInstancesSize, .allocationUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});
            animationInstanceBuffer.Map(m_context.GetAllocator());
        }

        return true;
    }

    bool VulkanRenderer::InitializeDescriptorPool()
    {
        std::array<VkDescriptorPoolSize, 3> poolSizes = {
            {
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, c_maxUniformBuffers},
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, c_maxStorageBuffers},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, c_maxCombinedImageSamplers},
            }};

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = c_maxSets;

        VK_CHECK(vkCreateDescriptorPool(m_context.GetDevice(), &poolInfo, nullptr, &m_descriptorPool));

        return true;
    }

    bool VulkanRenderer::InitializeDescriptorSets()
    {
        for (size_t i = 0; i < c_frameOverlap; i++)
        {
            std::vector<DescriptorSetBinding> bindings = std::vector<DescriptorSetBinding>{
                {
                    .binding = 0,
                    .type = DescriptorType::UniformBuffer,
                    .buffer = m_frames[i].uniformBuffer.GetBuffer(),
                    .size = m_frames[i].uniformBuffer.GetSize(),
                },
                {
                    .binding = 1,
                    .type = DescriptorType::StorageBuffer,
                    .buffer = m_frames[i].staticInstanceBuffer.GetBuffer(),
                    .size = m_frames[i].staticInstanceBuffer.GetSize(),
                },
                {
                    .binding = 2,
                    .type = DescriptorType::CombinedImageSampler,
                    .imageView = m_shadowMap.GetImageView(),
                    .sampler = m_shadowSampler,
                    .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                },
            };

            m_frames[i].staticDescriptorSet = VkInit::CreateDescriptorSet(m_context.GetDevice(), m_descriptorPool, m_descriptorSetLayout, bindings);

            bindings = {
                {
                    .binding = 0,
                    .type = DescriptorType::UniformBuffer,
                    .buffer = m_frames[i].uniformBuffer.GetBuffer(),
                    .size = m_frames[i].uniformBuffer.GetSize(),
                },
                {
                    .binding = 1,
                    .type = DescriptorType::StorageBuffer,
                    .buffer = m_frames[i].skeletalInstanceBuffer.GetBuffer(),
                    .size = m_frames[i].skeletalInstanceBuffer.GetSize(),
                },
                {
                    .binding = 2,
                    .type = DescriptorType::CombinedImageSampler,
                    .imageView = m_shadowMap.GetImageView(),
                    .sampler = m_shadowSampler,
                    .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                },
                {
                    .binding = 3,
                    .type = DescriptorType::UniformBuffer,
                    .buffer = m_boneBuffer.GetBuffer(),
                    .size = m_boneBuffer.GetSize(),
                },
                {
                    .binding = 4,
                    .type = DescriptorType::StorageBuffer,
                    .buffer = m_animationFrameBuffer.GetBuffer(),
                    .size = m_animationFrameBuffer.GetSize(),
                },
                {
                    .binding = 5,
                    .type = DescriptorType::UniformBuffer,
                    .buffer = m_frames[i].animationInstanceBuffer.GetBuffer(),
                    .size = m_frames[i].animationInstanceBuffer.GetSize(),
                },
            };

            m_frames[i].skeletalDescriptorSet = VkInit::CreateDescriptorSet(m_context.GetDevice(), m_descriptorPool, m_skeletalSetLayout, bindings);
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
        VK_CHECK(CreateDebugUtilsMessengerEXT(m_context.GetInstance(), &createInfo, nullptr, &m_debugMessenger));

        return true;
    }

    bool VulkanRenderer::InitializeTestResources()
    {
        return true;
    }

    void VulkanRenderer::RecreateSwapchain(uint32_t width, uint32_t height)
    {
        // vkDeviceWaitIdle(m_context.GetDevice());

        // for (auto framebuffer : m_swapchain.GetFramebuffers())
        // {
        //     vkDestroyFramebuffer(m_context.GetDevice(), framebuffer, nullptr);
        // }

        // for (const auto &frame : m_frames)
        // {
        //     vkFreeCommandBuffers(m_context.GetDevice(), m_commandPool, 1, &frame.commandBuffer);
        // }

        // vkDestroyPipeline(m_context.GetDevice(), m_graphicsPipeline, nullptr);
        // vkDestroyPipelineLayout(m_context.GetDevice(), m_pipelineLayout, nullptr);
        // vkDestroyRenderPass(m_context.GetDevice(), m_renderPass.GetRenderPass(), nullptr);

        // for (auto imageView : m_swapChainImageViews)
        // {
        //     vkDestroyImageView(m_context.GetDevice(), imageView, nullptr);
        // }

        // vkDestroySwapchainKHR(m_context.GetDevice(), m_swapChain, nullptr);

        // InitializeSwapChain(width, height);
        // InitializeImageViews();
        // InitializeRenderPass();
        // InitializeGraphicsPipeline();
        // InitializeFramebuffers();
        // InitializeCommandBuffer();
    }

    void VulkanRenderer::WriteCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, const std::vector<RenderBatch> &staticBatches, const std::vector<RenderBatch> &skeletalBatches)
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;                  // Optional
        beginInfo.pInheritanceInfo = nullptr; // Optional

        const FrameData &frame = m_frames[m_currentFrameIndex];

        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

        { // Shadow pass
            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = m_shadowPass.GetRenderPass();
            renderPassInfo.framebuffer = m_shadowFramebuffer;

            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = {2048, 2048};

            std::array<VkClearValue, 1> clearValues{};
            clearValues[0].depthStencil = {1.0f, 0};

            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            DrawPipeline<VulkanMesh>(commandBuffer, frame.staticDescriptorSet, m_staticShadowPipeline, staticBatches);

            vkCmdEndRenderPass(commandBuffer);
        }

        { // Render pass
            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = m_renderPass.GetRenderPass();
            renderPassInfo.framebuffer = m_swapchain.GetFramebuffers()[imageIndex];

            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = m_swapchain.GetExtent();

            std::array<VkClearValue, 2> clearValues{};
            // Nice light blue clear color
            clearValues[0].color = {{0.07f, 0.07f, 0.07f, 1.0f}};
            clearValues[1].depthStencil = {1.0f, 0};

            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            DrawPipeline<VulkanMesh>(commandBuffer, frame.staticDescriptorSet, m_staticPipeline, staticBatches);
            DrawPipeline<VulkanSkeletalMesh>(commandBuffer, frame.skeletalDescriptorSet, m_skeletalPipeline, skeletalBatches);

            vkCmdEndRenderPass(commandBuffer);
        }
        VK_CHECK(vkEndCommandBuffer(commandBuffer));
    }

    template <typename MeshType>
    void VulkanRenderer::DrawPipeline(VkCommandBuffer commandBuffer, VkDescriptorSet descriptorSet, const VulkanMaterialPipeline &pipeline, const std::vector<RenderBatch> &batches)
    {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetPipeline());

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)m_swapchain.GetExtent().width;
        viewport.height = (float)m_swapchain.GetExtent().height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = m_swapchain.GetExtent();
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        VkDescriptorSet descriptorSets[] = {descriptorSet};
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetPipelineLayout(), 0, 1, descriptorSets, 0, nullptr);

        for (const auto &batch : batches)
        {
            MeshDrawInfo mesh = m_resourcePool.GetMeshDrawInfo<MeshType>(batch.mesh);
            const VulkanMaterialInstance &material = m_resourcePool.GetMaterialInstance(batch.material);

            VkBuffer vertexBuffers[] = {mesh.vertexBuffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            if (pipeline.ShouldBindMaterial())
            {
                VkDescriptorSet descriptorSets[] = {material.GetDescriptorSet()};
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetPipelineLayout(), 1, 1, descriptorSets, 0, nullptr);
            }

            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(mesh.indexCount), batch.instanceCount, 0, 0, batch.firstInstance);
        }
    }

    void VulkanRenderer::Draw(const std::vector<RenderBatch> &staticBatches, const std::vector<glm::mat4> &staticInstances, const std::vector<RenderBatch> &skeletalBatches, const std::vector<SkeletalInstanceData> &skeletalInstances, const std::vector<AnimationInstanceData> &animationInstances)
    {
        constexpr uint32_t timeout = (std::numeric_limits<uint32_t>::max)();
        const uint32_t currentFrame = m_currentFrameIndex;
        const FrameData &frame = m_frames[currentFrame];

        vkWaitForFences(m_context.GetDevice(), 1, &frame.inFlightFence, VK_TRUE, timeout);
        vkResetFences(m_context.GetDevice(), 1, &frame.inFlightFence);

        uint32_t imageIndex;
        VK_CHECK(vkAcquireNextImageKHR(m_context.GetDevice(), m_swapchain.GetSwapchain(), timeout, frame.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex));
        vkResetCommandBuffer(frame.commandBuffer, 0);
        WriteCommandBuffer(frame.commandBuffer, imageIndex, staticBatches, skeletalBatches);

        static std::chrono::high_resolution_clock::time_point lastTime = std::chrono::high_resolution_clock::now();
        const auto currentTime = std::chrono::high_resolution_clock::now();
        const float dt = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
        lastTime = currentTime;

        // Uniform buffer
        UniformBufferData ubo = m_uniformBufferData;
        // Rortate forward vector to get view direction
        const glm::vec3 viewPos = m_camera.position;
        const glm::vec3 viewDir = m_camera.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        ubo.view = glm::lookAt(viewPos, viewPos + viewDir, glm::vec3(0.0f, 1.0f, 0.0f));
        ubo.viewPos = viewPos;

        // glm::vec3 lightPos = glm::vec3(0.0f, 500.0f, -500.0f);
        // glm::mat4 lightView = glm::lookAt(lightPos, lightPos + m_uniformBufferData.lightDir, glm::vec3(0.0f, 1.0f, 0.0f));
        // ubo.view = lightView;
        // ubo.viewPos = lightPos;

        frame.uniformBuffer.CopyData(&ubo, sizeof(ubo));

        // Instance buffer
        const size_t size = sizeof(InstanceData) * staticInstances.size();
        frame.staticInstanceBuffer.CopyData(staticInstances.data(), size);

        // Skeletal instance buffer
        const size_t skeletalSize = sizeof(SkeletalInstanceData) * skeletalInstances.size();
        frame.skeletalInstanceBuffer.CopyData(skeletalInstances.data(), skeletalSize);

        // Animation instance buffer
        const size_t animationSize = sizeof(AnimationInstanceData) * animationInstances.size();
        frame.animationInstanceBuffer.CopyData(animationInstances.data(), animationSize);

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

        VK_CHECK(vkQueueSubmit(m_context.GetGraphicsQueue(), 1, &submitInfo, frame.inFlightFence));

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {m_swapchain.GetSwapchain()};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        VK_CHECK(vkQueuePresentKHR(m_context.GetPresentQueue(), &presentInfo));

        m_currentFrameIndex = (currentFrame + 1) % c_frameOverlap;
    }

    void VulkanRenderer::Shutdown()
    {
        vkDeviceWaitIdle(m_context.GetDevice());

        for (size_t i = 0; i < c_frameOverlap; i++)
        {
            vkDestroySemaphore(m_context.GetDevice(), m_frames[i].imageAvailableSemaphore, nullptr);
            vkDestroySemaphore(m_context.GetDevice(), m_frames[i].renderFinishedSemaphore, nullptr);
            vkDestroyFence(m_context.GetDevice(), m_frames[i].inFlightFence, nullptr);

            vkFreeCommandBuffers(m_context.GetDevice(), m_commandPool, 1, &m_frames[i].commandBuffer);

            m_frames[i].uniformBuffer.Unmap(m_context.GetAllocator());
            m_frames[i].uniformBuffer.Destroy(m_context.GetAllocator());

            m_frames[i].staticInstanceBuffer.Unmap(m_context.GetAllocator());
            m_frames[i].staticInstanceBuffer.Destroy(m_context.GetAllocator());

            m_frames[i].skeletalInstanceBuffer.Unmap(m_context.GetAllocator());
            m_frames[i].skeletalInstanceBuffer.Destroy(m_context.GetAllocator());

            m_frames[i].animationInstanceBuffer.Unmap(m_context.GetAllocator());
            m_frames[i].animationInstanceBuffer.Destroy(m_context.GetAllocator());
        }

        vkDestroySampler(m_context.GetDevice(), m_textureSampler, nullptr);
        vkDestroySampler(m_context.GetDevice(), m_shadowSampler, nullptr);

        m_depthImage.Destroy(m_context);
        m_shadowMap.Destroy(m_context);
        m_resourcePool.Destroy(m_context);
        m_staticVertexShader.Destroy(m_context);
        m_skeletalVertexShader.Destroy(m_context);
        m_fragmentShader.Destroy(m_context);
        m_staticShadowVertexShader.Destroy(m_context);
        m_shadowFragmentShader.Destroy(m_context);

        vkDestroyCommandPool(m_context.GetDevice(), m_commandPool, nullptr);

        vkDestroyDescriptorPool(m_context.GetDevice(), m_descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(m_context.GetDevice(), m_descriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_context.GetDevice(), m_skeletalSetLayout, nullptr);

        m_boneBuffer.Destroy(m_context.GetAllocator());
        m_animationFrameBuffer.Destroy(m_context.GetAllocator());
        m_animationInstanceBuffer.Destroy(m_context.GetAllocator());
        m_skeletalInstanceBuffer.Destroy(m_context.GetAllocator());

        m_staticPipeline.Destroy(m_context);
        m_skeletalPipeline.Destroy(m_context);
        m_staticShadowPipeline.Destroy(m_context);

        m_renderPass.Destroy(m_context);
        m_shadowPass.Destroy(m_context);

        vkDestroyFramebuffer(m_context.GetDevice(), m_shadowFramebuffer, nullptr);

        m_swapchain.Destroy(m_context);

        if (c_validationLayersEnabled)
        {
            DestroyDebugUtilsMessengerEXT(m_context.GetInstance(), m_debugMessenger, nullptr);
        }

        m_context.Destroy();
    }

    RenderHandle VulkanRenderer::LoadMesh(const std::string &filepath)
    {
        VulkanMesh mesh = VulkanMesh::CreateFromFile(
            {.device = m_context.GetDevice(),
             .commandPool = m_commandPool,
             .queue = m_context.GetGraphicsQueue(),
             .allocator = m_context.GetAllocator(),
             .filepath = filepath});

        return m_resourcePool.AddMesh(std::move(mesh));
    }

    RenderHandle VulkanRenderer::LoadSkeletalMesh(const std::string &filepath)
    {
        VulkanSkeletalMesh mesh = VulkanSkeletalMesh::CreateFromFile(m_context, m_commandPool, m_bones, {.filepath = filepath});

        return m_resourcePool.AddSkeletalMesh(std::move(mesh));
    }

    RenderHandle VulkanRenderer::LoadAnimation(const std::string &filepath)
    {
        return m_resourcePool.AddAnimation(VulkanAnimation::CreateFromFile(m_animationFrames, {.filepath = filepath}));
    }

    RenderHandle VulkanRenderer::LoadImage(const std::string &filepath)
    {
        VulkanImage image = VulkanImage::CreateFromFile(
            {.device = m_context.GetDevice(),
             .commandPool = m_commandPool,
             .queue = m_context.GetGraphicsQueue(),
             .allocator = m_context.GetAllocator(),
             .filepath = filepath});

        return m_resourcePool.AddImage(std::move(image));
    }
}