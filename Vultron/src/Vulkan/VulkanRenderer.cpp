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
#include <thread>

namespace Vultron
{

    glm::mat4 ComputeLightProjectionMatrix(const glm::mat4 &camProj, const glm::mat4 &camView, const glm::vec3 &lightDir)
    {
        glm::vec3 frustumCorners[8] = {
            glm::vec3(-1.0f, 1.0f, 0.0f),
            glm::vec3(1.0f, 1.0f, 0.0f),
            glm::vec3(1.0f, -1.0f, 0.0f),
            glm::vec3(-1.0f, -1.0f, 0.0f),
            glm::vec3(-1.0f, 1.0f, 1.0f),
            glm::vec3(1.0f, 1.0f, 1.0f),
            glm::vec3(1.0f, -1.0f, 1.0f),
            glm::vec3(-1.0f, -1.0f, 1.0f),
        };

        glm::mat4 invCam = glm::inverse(camProj * camView);
        for (uint32_t i = 0; i < 8; i++)
        {
            glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[i], 1.0f);
            frustumCorners[i] = invCorner / invCorner.w;
        }

        // Get frustum center
        glm::vec3 frustumCenter = glm::vec3(0.0f);
        for (uint32_t i = 0; i < 8; i++)
        {
            frustumCenter += frustumCorners[i];
        }
        frustumCenter /= 8.0f;

        float radius = 0.0f;
        for (uint32_t i = 0; i < 8; i++)
        {
            float distance = glm::length(frustumCorners[i] - frustumCenter);
            radius = glm::max(radius, distance);
        }

        radius = std::ceil(radius * 16.0f) / 16.0f;

        glm::vec3 maxExtents = glm::vec3(radius);
        glm::vec3 minExtents = -maxExtents;

        glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);
        glm::mat4 lightProjMatrix = lightOrthoMatrix * lightViewMatrix;

        return lightProjMatrix;
    }

    void UpdateShadowCascades(
        const glm::mat4 &camProj,
        const glm::mat4 &camView,
        float nearPlane,
        float farPlane,
        const glm::vec3 &lightDir,
        std::array<glm::mat4, c_numShadowCascades> &lightViewProjections,
        std::array<float, c_numShadowCascades> &cascadeSplits)
    {
        constexpr float c_cascadeSplitLambda = 0.95f;

        float clipRange = farPlane - nearPlane;

        float minZ = nearPlane;
        float maxZ = nearPlane + clipRange;

        float range = maxZ - minZ;
        float ratio = maxZ / minZ;

        std::array<float, c_numShadowCascades> splits{};
        // Calculate split depths based on view camera frustum
        // Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
        for (uint32_t i = 0; i < c_numShadowCascades; i++)
        {
            float p = (i + 1) / static_cast<float>(c_numShadowCascades);
            float log = minZ * std::pow(ratio, p);
            float uniform = minZ + range * p;
            float d = c_cascadeSplitLambda * (log - uniform) + uniform;
            splits[i] = (d - nearPlane) / clipRange;
        }

        // Calculate orthographic projection matrix for each cascade
        glm::mat4 invCam = glm::inverse(camProj * camView);
        float lastSplitDist = 0.0;
        for (uint32_t i = 0; i < c_numShadowCascades; i++)
        {
            float splitDist = splits[i];

            glm::vec3 frustumCorners[8] = {
                glm::vec3(-1.0f, 1.0f, 0.0f),
                glm::vec3(1.0f, 1.0f, 0.0f),
                glm::vec3(1.0f, -1.0f, 0.0f),
                glm::vec3(-1.0f, -1.0f, 0.0f),
                glm::vec3(-1.0f, 1.0f, 1.0f),
                glm::vec3(1.0f, 1.0f, 1.0f),
                glm::vec3(1.0f, -1.0f, 1.0f),
                glm::vec3(-1.0f, -1.0f, 1.0f),
            };

            // Project frustum corners into world space
            for (uint32_t j = 0; j < 8; j++)
            {
                glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[j], 1.0f);
                frustumCorners[j] = invCorner / invCorner.w;
            }

            for (uint32_t j = 0; j < 4; j++)
            {
                glm::vec3 dist = frustumCorners[j + 4] - frustumCorners[j];
                frustumCorners[j + 4] = frustumCorners[j] + (dist * splitDist);
                frustumCorners[j] = frustumCorners[j] + (dist * lastSplitDist);
            }

            // Get frustum center
            glm::vec3 frustumCenter = glm::vec3(0.0f);
            for (uint32_t j = 0; j < 8; j++)
            {
                frustumCenter += frustumCorners[j];
            }
            frustumCenter /= 8.0f;

            float radius = 0.0f;
            for (uint32_t j = 0; j < 8; j++)
            {
                float distance = glm::length(frustumCorners[j] - frustumCenter);
                radius = glm::max(radius, distance);
            }
            radius = std::ceil(radius * 16.0f) / 16.0f;

            glm::vec3 maxExtents = glm::vec3(radius);
            glm::vec3 minExtents = -maxExtents;

            glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);

            // Store split distance and matrix in cascade
            cascadeSplits[i] = (nearPlane + splitDist * clipRange) * -1.0f;
            lightViewProjections[i] = lightOrthoMatrix * lightViewMatrix;

            lastSplitDist = splits[i];
        }
    }

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

        if (!InitializeCommandPools())
        {
            std::cerr << "Faild to initialize command pool." << std::endl;
            return false;
        }

        if (!InitializeCommandBuffer())
        {
            std::cerr << "Faild to initialize command buffer." << std::endl;
            return false;
        }

        if (!InitializeDescriptorPool())
        {
            std::cerr << "Faild to initialize descriptor pool." << std::endl;
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

        if (!InitializeDepthBuffer())
        {
            std::cerr << "Faild to initialize depth buffer." << std::endl;
            return false;
        }

        if (!InitializeSamplers())
        {
            std::cerr << "Faild to initialize sampler." << std::endl;
            return false;
        }

        if (!InitializeFramebuffers())
        {
            std::cerr << "Faild to initialize framebuffers." << std::endl;
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

        if (!InitializeBloomMipChain())
        {
            std::cerr << "Faild to initialize bloom mip chain." << std::endl;
            return false;
        }

        if (!InitializeBloomPipeline())
        {
            std::cerr << "Faild to initialize bloom pipeline." << std::endl;
            return false;
        }

        if (!InitializeSkeletalComputePipeline())
        {
            std::cerr << "Faild to initialize skeletal compute pipeline." << std::endl;
            return false;
        }

        if (!InitializeSkeletalBuffers())
        {
            std::cerr << "Faild to initialize skeletal buffers." << std::endl;
            return false;
        }

        if (!InitializeParticlePipeline())
        {
            std::cerr << "Faild to initialize particle pipeline." << std::endl;
            return false;
        }

        if (!InitializeParticleBuffers())
        {
            std::cerr << "Faild to initialize particle buffers." << std::endl;
            return false;
        }

        if (!InitializeRibbonPipeline())
        {
            std::cerr << "Faild to initialize ribbon pipeline." << std::endl;
            return false;
        }

        if (!InitializeRibbonBuffers())
        {
            std::cerr << "Faild to initialize ribbon buffers." << std::endl;
            return false;
        }

        if (!InitializeLineBuffers())
        {
            std::cerr << "Faild to initialize line buffers." << std::endl;
            return false;
        }

        if (!InitializeLinePipeline())
        {
            std::cerr << "Faild to initialize line pipeline." << std::endl;
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

        if (!InitializeResources())
        {
            std::cerr << "Faild to initialize geometry." << std::endl;
            return false;
        }

        if (!InitializeEnvironmentMap())
        {
            std::cerr << "Faild to initialize environment map." << std::endl;
            return false;
        }

        if (!InitializeDescriptorSets())
        {
            std::cerr << "Faild to initialize descriptor sets." << std::endl;
            return false;
        }

        return true;
    }

    void VulkanRenderer::PostInitialize()
    {
        std::vector<SkeletalBoneData> bones{};
        bones.reserve(m_bones.size());
        for (size_t i = 0; i < m_bones.size(); i++)
        {
            bones.push_back({.offset = glm::transpose(m_bones[i].offset), .parentID = m_bones[i].parentID});
        }

        if (bones.size() > c_maxBones)
        {
            std::cerr << "Too many bones in the scene. Max supported bones: " << c_maxBones << std::endl;
            std::cerr << "Current bones: " << bones.size() << std::endl;
            return;
        }

        if (bones.size() > 0)
        {
            m_boneBuffer.UploadStaged(m_context.GetDevice(), m_commandPool, m_context.GetGraphicsQueue(), m_context.GetAllocator(), bones.data(), sizeof(bones[0]) * bones.size());
        }

        if (m_animationFrames.size() > c_maxAnimationFrames)
        {
            std::cerr << "Too many animation frames in the scene. Max supported frames: " << c_maxAnimationFrames << std::endl;
            std::cerr << "Current frames: " << m_animationFrames.size() << std::endl;
            return;
        }

        if (m_animationFrames.size() > 0)
        {
            m_animationFrameBuffer.UploadStaged(m_context.GetDevice(), m_commandPool, m_context.GetGraphicsQueue(), m_context.GetAllocator(), m_animationFrames.data(), sizeof(m_animationFrames[0]) * m_animationFrames.size());
        }
    }

    bool VulkanRenderer::InitializeRenderPass()
    {
        m_scenePass = VulkanRenderPass::Create(
            m_context,
            {
                .attachments = {
                    // Color attachment
                    {
                        // HDR format
                        .format = c_sceneImageFormat,
                        // Layout for texture to be used in bloom pass
                        .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
                    },
                    // Outline attachment
                    {
                        .format = VK_FORMAT_R32_SFLOAT,
                        .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
                    },
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
                        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    },
                },
                .dependencies = {
                    {
                        .srcSubpass = VK_SUBPASS_EXTERNAL,
                        .dstSubpass = 0,
                        .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                        .srcAccessMask = VK_ACCESS_NONE_KHR,
                        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
                    },
                    {
                        .srcSubpass = 0,
                        .dstSubpass = VK_SUBPASS_EXTERNAL,
                        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                        .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
                        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
                    },
                },
            });

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
                        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                    },
                },
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

        m_compositePass = VulkanRenderPass::Create(
            m_context,
            {
                .attachments = {
                    // Color attachment
                    {
                        .format = m_swapchain.GetImageFormat(),
                    },
                },
            });

        return true;
    }

    bool VulkanRenderer::InitializeDescriptorSetLayout()
    {
        m_staticSetLayout = VkInit::CreateDescriptorSetLayout(
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
                    // BRDF LUT
                    .binding = 3,
                    .type = DescriptorType::CombinedImageSampler,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
            });

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
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
                },
                {
                    // Shadow map
                    .binding = 2,
                    .type = DescriptorType::CombinedImageSampler,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
                {
                    // BRDF LUT
                    .binding = 3,
                    .type = DescriptorType::CombinedImageSampler,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
                {
                    // Bone Output
                    .binding = 6,
                    .type = DescriptorType::StorageBuffer,
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
                },
            });

        m_spriteSetLayout = VkInit::CreateDescriptorSetLayout(
            m_context.GetDevice(),
            {
                {
                    .binding = 0,
                    .type = DescriptorType::StorageBuffer,
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                },
            });

        m_skyboxSetLayout = VkInit::CreateDescriptorSetLayout(
            m_context.GetDevice(),
            {
                {
                    // Scene data for skybox vertex shader
                    .binding = 0,
                    .type = DescriptorType::UniformBuffer,
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                },
            });

        m_environmentSetLayout = VkInit::CreateDescriptorSetLayout(
            m_context.GetDevice(),
            {
                {
                    // Irradiance map
                    .binding = 0,
                    .type = DescriptorType::CombinedImageSampler,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
                {
                    // Prefiltered map
                    .binding = 1,
                    .type = DescriptorType::CombinedImageSampler,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
                {
                    // Probe data
                    .binding = 2,
                    .type = DescriptorType::StorageBuffer,
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                },
            });

        m_particleSetLayout = VkInit::CreateDescriptorSetLayout(
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
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
                },
                {
                    // Shadow map
                    .binding = 2,
                    .type = DescriptorType::CombinedImageSampler,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
                {
                    // BRDF LUT
                    .binding = 3,
                    .type = DescriptorType::CombinedImageSampler,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
            });

        m_ribbonSetLayout = VkInit::CreateDescriptorSetLayout(
            m_context.GetDevice(),
            {
                {
                    // Scene data
                    .binding = 0,
                    .type = DescriptorType::UniformBuffer,
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                },
                {
                    // Shadow map
                    .binding = 2,
                    .type = DescriptorType::CombinedImageSampler,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
                {
                    // BRDF LUT
                    .binding = 3,
                    .type = DescriptorType::CombinedImageSampler,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
            });

        m_bloomSetLayout = VkInit::CreateDescriptorSetLayout(
            m_context.GetDevice(),
            {
                {
                    .binding = 0,
                    .type = DescriptorType::CombinedImageSampler,
                    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                },
                {
                    .binding = 1,
                    .type = DescriptorType::StorageImage,
                    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                },
            });

        m_compositeSetLayout = VkInit::CreateDescriptorSetLayout(
            m_context.GetDevice(),
            {
                {
                    // Scene image
                    .binding = 0,
                    .type = DescriptorType::CombinedImageSampler,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
                {
                    // Bloom image
                    .binding = 1,
                    .type = DescriptorType::CombinedImageSampler,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
                {
                    // Depth image
                    .binding = 2,
                    .type = DescriptorType::CombinedImageSampler,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
            });

        return true;
    }

    bool VulkanRenderer::InitializeGraphicsPipeline()
    {
        // Shader
        m_staticVertexShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/scene.vert.spv"});
        m_skeletalVertexShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/skeletal.vert.spv"});
        m_fragmentShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/scene.frag.spv"});

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
            // AO, Roughness, Metalness(ARM)
            {
                .binding = 2,
                .type = DescriptorType::CombinedImageSampler,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
            // Emissive
            {
                .binding = 3,
                .type = DescriptorType::CombinedImageSampler,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
        };

        VkPushConstantRange materialParameters = {
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(PBRMaterial::Parameters),
        };

        m_staticPipeline = VulkanMaterialPipeline::Create(
            m_context, m_scenePass,
            {
                .vertexShader = m_staticVertexShader,
                .fragmentShader = m_fragmentShader,
                .descriptorSetLayouts = {m_staticSetLayout, m_environmentSetLayout},
                .bindings = materialBindings,
                .pushConstantRanges = {materialParameters},
                .vertexDescription = StaticMeshVertex::GetVertexDescription(),
            });

        m_skeletalPipeline = VulkanMaterialPipeline::Create(
            m_context, m_scenePass,
            {
                .vertexShader = m_skeletalVertexShader,
                .fragmentShader = m_fragmentShader,
                .descriptorSetLayouts = {m_skeletalSetLayout, m_environmentSetLayout},
                .bindings = materialBindings,
                .pushConstantRanges = {materialParameters},
                .vertexDescription = SkeletalMeshVertex::GetVertexDescription(),
            });

        // Shadow

        m_staticShadowVertexShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/shadow.vert.spv"});
        m_skeletalShadowVertexShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/shadow_skeletal.vert.spv"});
        m_shadowFragmentShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/shadow.frag.spv"});

        m_staticShadowPipeline = VulkanMaterialPipeline::Create(
            m_context, m_shadowPass,
            {
                .vertexShader = m_staticShadowVertexShader,
                .fragmentShader = m_shadowFragmentShader,
                .descriptorSetLayouts = {m_staticSetLayout},
                .bindings = {},
                .pushConstantRanges = {
                    {
                        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                        .offset = 0,
                        .size = sizeof(uint32_t),
                    },
                },
                .vertexDescription = StaticMeshVertex::GetVertexDescription(),
                .cullMode = CullMode::Front,
                .outputToSceneImage = false,
            });

        m_skeletalShadowPipeline = VulkanMaterialPipeline::Create(
            m_context, m_shadowPass,
            {
                .vertexShader = m_skeletalShadowVertexShader,
                .fragmentShader = m_shadowFragmentShader,
                .descriptorSetLayouts = {m_skeletalSetLayout},
                .bindings = {},
                .pushConstantRanges = {
                    {
                        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                        .offset = 0,
                        .size = sizeof(uint32_t),
                    },
                },
                .vertexDescription = SkeletalMeshVertex::GetVertexDescription(),
                .cullMode = CullMode::Front,
                .outputToSceneImage = false,
            });

        // Sprite
        m_spriteVertexShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/sprite.vert.spv"});
        m_spriteFragmentShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/sprite.frag.spv"});

        m_spritePipeline = VulkanMaterialPipeline::Create(
            m_context, m_compositePass,
            {
                .vertexShader = m_spriteVertexShader,
                .fragmentShader = m_spriteFragmentShader,
                .descriptorSetLayouts = {m_spriteSetLayout},
                .bindings = {
                    {
                        .binding = 0,
                        .type = DescriptorType::CombinedImageSampler,
                        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                    },
                },
                .pushConstantRanges = {
                    {
                        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                        .offset = 0,
                        .size = sizeof(glm::uvec2),
                    },
                },
                .vertexDescription = SpriteVertex::GetVertexDescription(),
                .cullMode = CullMode::Front,
                .depthFunction = DepthFunction::Always,
                .outputToSceneImage = false,
            });

        // SDF
        m_sdfFragmentShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/sdf.frag.spv"});

        m_sdfPipeline = VulkanMaterialPipeline::Create(
            m_context, m_compositePass,
            {
                .vertexShader = m_spriteVertexShader,
                .fragmentShader = m_sdfFragmentShader,
                .descriptorSetLayouts = {m_spriteSetLayout},
                .bindings = {
                    {
                        .binding = 0,
                        .type = DescriptorType::CombinedImageSampler,
                        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                    },
                },
                .pushConstantRanges = {
                    {
                        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                        .offset = 0,
                        .size = sizeof(glm::uvec2),
                    },
                },
                .vertexDescription = SpriteVertex::GetVertexDescription(),
                .cullMode = CullMode::Front,
                .depthFunction = DepthFunction::Always,
                .outputToSceneImage = false,
            });

        // Skybox
        m_skyboxVertexShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/skybox.vert.spv"});
        m_skyboxFragmentShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/skybox.frag.spv"});

        m_skyboxPipeline = VulkanMaterialPipeline::Create(
            m_context, m_scenePass,
            {
                .vertexShader = m_skyboxVertexShader,
                .fragmentShader = m_skyboxFragmentShader,
                .descriptorSetLayouts = {m_skyboxSetLayout},
                .bindings = {
                    {
                        .binding = 0,
                        .type = DescriptorType::CombinedImageSampler,
                        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                    },
                },
                .vertexDescription = StaticMeshVertex::GetVertexDescription(),
                .cullMode = CullMode::Front,
                .depthFunction = DepthFunction::LessOrEqual,
            });

        // Composite
        m_compositeVertexShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/composite.vert.spv"});
        m_compositeFragmentShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/composite.frag.spv"});

        m_compositePipeline = VulkanMaterialPipeline::Create(
            m_context, m_compositePass,
            {
                .vertexShader = m_compositeVertexShader,
                .fragmentShader = m_compositeFragmentShader,
                .descriptorSetLayouts = {m_compositeSetLayout},
                .bindings = {},
                .pushConstantRanges = {
                    {
                        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                        .offset = 0,
                        .size = sizeof(BloomSettings) + 2 * sizeof(float),
                    },
                },
                .vertexDescription = {},
                .cullMode = CullMode::Front,
                .blendEnable = false,
                .depthFunction = DepthFunction::Always,
                .outputToSceneImage = false,
            });

        return true;
    }

    bool VulkanRenderer::InitializeSkeletalComputePipeline()
    {
        m_skeletalComputeShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/skeletal.comp.spv"});

        // Create descriptor set layout
        m_skeletalComputePipeline = VulkanComputePipeline::Create(
            m_context,
            {
                .shader = m_skeletalComputeShader,
                .bindings = {
                    {
                        // Instance data
                        .binding = 0,
                        .type = DescriptorType::StorageBuffer,
                        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
                    },
                    {
                        // Bone data
                        .binding = 1,
                        .type = DescriptorType::UniformBuffer,
                        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                    },
                    {
                        // Animation data
                        .binding = 2,
                        .type = DescriptorType::StorageBuffer,
                        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                    },
                    {
                        // Animation instance data
                        .binding = 3,
                        .type = DescriptorType::UniformBuffer,
                        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                    },
                    {
                        // Output bone data
                        .binding = 4,
                        .type = DescriptorType::StorageBuffer,
                        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
                    },
                },
            });

        return true;
    }

    bool VulkanRenderer::InitializeSkeletalBuffers()
    {
        m_boneBuffer = VulkanBuffer::Create({.allocator = m_context.GetAllocator(), .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, .size = sizeof(SkeletalBoneData) * c_maxBones, .allocationUsage = VMA_MEMORY_USAGE_GPU_ONLY});
        m_animationFrameBuffer = VulkanBuffer::Create({.allocator = m_context.GetAllocator(), .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, .size = sizeof(AnimationFrame) * c_maxAnimationFrames, .allocationUsage = VMA_MEMORY_USAGE_GPU_ONLY});

        return true;
    }

    bool VulkanRenderer::InitializeParticlePipeline()
    {
        m_particleEmitterShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/particle_emit.comp.spv"});
        m_particleEmitterPipeline = VulkanComputePipeline::Create(
            m_context,
            {
                .shader = m_particleEmitterShader,
                .bindings = {
                    {
                        // Uniform data
                        .binding = 0,
                        .type = DescriptorType::UniformBuffer,
                        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                    },
                    {
                        // Particle emitter data
                        .binding = 1,
                        .type = DescriptorType::StorageBuffer,
                        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                    },
                    {
                        // Particle instance data
                        .binding = 2,
                        .type = DescriptorType::StorageBuffer,
                        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                    },
                },
            });

        m_particleUpdateShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/particle_update.comp.spv"});
        m_particleUpdatePipeline = VulkanComputePipeline::Create(
            m_context,
            {
                .shader = m_particleUpdateShader,
                .bindings = {
                    {
                        // Uniform data
                        .binding = 0,
                        .type = DescriptorType::UniformBuffer,
                        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                    },
                    {
                        // In particle instance data
                        .binding = 1,
                        .type = DescriptorType::StorageBuffer,
                        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                    },
                    {
                        // Out particle instance data
                        .binding = 2,
                        .type = DescriptorType::StorageBuffer,
                        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                    },
                },
            });

        m_particleSortShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/particle_sort.comp.spv"});
        m_particleSortPipeline = VulkanComputePipeline::Create(
            m_context,
            {
                .shader = m_particleSortShader,
                .bindings = {
                    {
                        // Particle instance data
                        .binding = 0,
                        .type = DescriptorType::StorageBuffer,
                        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                    },
                },
                .pushConstantRanges = {
                    {
                        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                        .offset = 0,
                        .size = 4 * sizeof(uint32_t) + sizeof(glm::mat4),
                    },
                },
            });

        m_particleVertexShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/particle.vert.spv"});
        m_particlePipeline = VulkanMaterialPipeline::Create(
            m_context, m_scenePass,
            {
                .vertexShader = m_particleVertexShader,
                .fragmentShader = m_fragmentShader,
                .descriptorSetLayouts = {m_particleSetLayout, m_environmentSetLayout},
                .bindings = {
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
                    // ARM
                    {
                        .binding = 2,
                        .type = DescriptorType::CombinedImageSampler,
                        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                    },
                    // Emissive
                    {
                        .binding = 3,
                        .type = DescriptorType::CombinedImageSampler,
                        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                    },
                },
                .pushConstantRanges = {
                    {
                        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                        .offset = 0,
                        .size = sizeof(PBRMaterial::Parameters),
                    },
                },
                .vertexDescription = StaticMeshVertex::GetVertexDescription(),
                .depthWriteEnable = false,
            });

        return true;
    }

    bool VulkanRenderer::InitializeParticleBuffers()
    {
        return true;
    }

    bool VulkanRenderer::InitializeRibbonPipeline()
    {
        m_ribbonVertexShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/ribbon.vert.spv"});
        m_ribbonPipeline = VulkanMaterialPipeline::Create(
            m_context, m_scenePass,
            {
                .vertexShader = m_ribbonVertexShader,
                .fragmentShader = m_fragmentShader,
                .descriptorSetLayouts = {m_ribbonSetLayout, m_environmentSetLayout},
                .bindings = {
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
                    // ARM
                    {
                        .binding = 2,
                        .type = DescriptorType::CombinedImageSampler,
                        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                    },
                    // Emissive
                    {
                        .binding = 3,
                        .type = DescriptorType::CombinedImageSampler,
                        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                    },
                },
                .pushConstantRanges = {
                    {
                        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                        .offset = 0,
                        .size = sizeof(PBRMaterial::Parameters),
                    },
                },
                .vertexDescription = RibbonVertex::GetVertexDescription(),
                .cullMode = CullMode::None,
                .depthWriteEnable = false,
            });

        return true;
    }

    bool VulkanRenderer::InitializeRibbonBuffers()
    {
        for (size_t i = 0; i < c_frameOverlap; i++)
        {
            m_frames[i].ribbonVertexBuffer = VulkanBuffer::Create({.allocator = m_context.GetAllocator(), .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, .size = sizeof(RibbonVertex) * c_maxRibbonVertices, .allocationUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});
            m_frames[i].ribbonVertexBuffer.Map(m_context.GetAllocator());

            m_frames[i].ribbonIndexBuffer = VulkanBuffer::Create({.allocator = m_context.GetAllocator(), .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, .size = sizeof(uint32_t) * c_maxRibbonVertices, .allocationUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});
            m_frames[i].ribbonIndexBuffer.Map(m_context.GetAllocator());
        }

        return true;
    }

    bool VulkanRenderer::InitializeLineBuffers()
    {
        for (size_t i = 0; i < c_frameOverlap; i++)
        {
            m_frames[i].lineVertexBuffer = VulkanBuffer::Create({.allocator = m_context.GetAllocator(), .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, .size = sizeof(LineVertex) * 2 * c_maxLines, .allocationUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});
            m_frames[i].lineVertexBuffer.Map(m_context.GetAllocator());
        }

        return true;
    }

    bool VulkanRenderer::InitializeLinePipeline()
    {
        m_lineVertexShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/line.vert.spv"});
        m_lineFragmentShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/line.frag.spv"});

        m_linePipeline = VulkanMaterialPipeline::Create(
            m_context, m_scenePass,
            {
                .vertexShader = m_lineVertexShader,
                .fragmentShader = m_lineFragmentShader,
                .descriptorSetLayouts = {m_skyboxSetLayout},
                .bindings = {},
                .vertexDescription = LineVertex::GetVertexDescription(),
                .cullMode = CullMode::None,
                .depthWriteEnable = false,
                .topology = Topology::LineList,
            });

        return true;
    }

    bool VulkanRenderer::InitializeFramebuffers()
    {
        std::vector<VkFramebuffer> &framebuffers = m_swapchain.GetFramebuffers();
        const std::vector<VkImageView> &imageViews = m_swapchain.GetImageViews();
        framebuffers.resize(imageViews.size());

        for (size_t i = 0; i < imageViews.size(); i++)
        {
            std::array<VkImageView, 1> attachments = {imageViews[i]};

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_compositePass.GetRenderPass();
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = m_swapchain.GetExtent().width;
            framebufferInfo.height = m_swapchain.GetExtent().height;
            framebufferInfo.layers = 1;

            VK_CHECK(vkCreateFramebuffer(m_context.GetDevice(), &framebufferInfo, nullptr, &framebuffers[i]));
        }

        m_sceneImage = VulkanImage::Create(
            m_context,
            {
                .info = {
                    .width = m_swapchain.GetExtent().width,
                    .height = m_swapchain.GetExtent().height,
                    .depth = 1,
                    .mipLevels = 1,
                    .format = c_sceneImageFormat,
                },
                .aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
                .additionalUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            });

        std::array<VkImageView, 3> attachments = {m_sceneImage.GetImageView(), m_depthOutlineImage.GetImageView(), m_depthImage.GetImageView()};

        VkFramebufferCreateInfo sceneFramebufferInfo{};
        sceneFramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        sceneFramebufferInfo.renderPass = m_scenePass.GetRenderPass();
        sceneFramebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        sceneFramebufferInfo.pAttachments = attachments.data();
        sceneFramebufferInfo.width = m_swapchain.GetExtent().width;
        sceneFramebufferInfo.height = m_swapchain.GetExtent().height;
        sceneFramebufferInfo.layers = 1;

        VK_CHECK(vkCreateFramebuffer(m_context.GetDevice(), &sceneFramebufferInfo, nullptr, &m_sceneFramebuffer));

        return true;
    }

    bool VulkanRenderer::InitializeShadowMap()
    {
        m_shadowMap = VulkanImage::Create(
            m_context,
            {
                .info = {
                    .width = 2048 * 2,
                    .height = 2048 * 2,
                    .depth = 1,
                    .mipLevels = 1,
                    .format = VK_FORMAT_D32_SFLOAT,
                    .layers = c_numShadowCascades,
                },
                .type = ImageType::Texture2DArray,
                .aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT,
                .additionalUsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            });

        for (size_t i = 0; i < c_numShadowCascades; i++)
        {
            // Create image view
            VkImageViewCreateInfo imageViewInfo{};
            imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            imageViewInfo.image = m_shadowMap.GetImage();
            imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            imageViewInfo.format = VK_FORMAT_D32_SFLOAT;
            imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            imageViewInfo.subresourceRange.baseMipLevel = 0;
            imageViewInfo.subresourceRange.levelCount = 1;
            imageViewInfo.subresourceRange.baseArrayLayer = i;
            imageViewInfo.subresourceRange.layerCount = 1;

            VK_CHECK(vkCreateImageView(m_context.GetDevice(), &imageViewInfo, nullptr, &m_shadowCascadeImageViews[i]));

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_shadowPass.GetRenderPass();
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = &m_shadowCascadeImageViews[i];
            framebufferInfo.width = m_shadowMap.GetInfo().width;
            framebufferInfo.height = m_shadowMap.GetInfo().height;
            framebufferInfo.layers = 1;

            VK_CHECK(vkCreateFramebuffer(m_context.GetDevice(), &framebufferInfo, nullptr, &m_shadowCascadeFramebuffers[i]));
        }

        return true;
    }

    bool VulkanRenderer::InitializeBloomMipChain()
    {
        // Initialize bloom pipeline
        const uint32_t width = m_swapchain.GetExtent().width;
        const uint32_t height = m_swapchain.GetExtent().height;
        const uint32_t maxSwapchainMipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
        const uint32_t mipLevels = std::min(maxSwapchainMipLevels, c_maxBloomMipLevels);

        m_bloomMipChain.resize(mipLevels);
        for (uint32_t i = 0; i < mipLevels; i++)
        {
            m_bloomMipChain[i] = VulkanImage::Create(
                m_context,
                {
                    .info = {
                        .width = width >> (i + 1),
                        .height = height >> (i + 1),
                        .depth = 1,
                        .mipLevels = 1,
                        .format = c_sceneImageFormat,
                    },
                    .aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
                    .additionalUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                });

            m_bloomMipChain[i].TransitionLayout(m_context.GetDevice(), m_commandPool, m_context.GetGraphicsQueue(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        }

        // Descriptor sets for downsampling and upsampling compute shaders for the mipchain, i.e. mipLevels - 1 sets for each,
        // but we use the scene image as the first input for downsampling which adds one more set
        m_bloomDownsampleSets.resize(mipLevels);
        for (uint32_t i = 0; i < mipLevels; i++)
        {
            m_bloomDownsampleSets[i] = VkInit::CreateDescriptorSet(
                m_context.GetDevice(), m_descriptorPool, m_bloomSetLayout,
                {
                    {
                        .binding = 0,
                        .type = DescriptorType::CombinedImageSampler,
                        .imageView = i == 0 ? m_sceneImage.GetImageView() : m_bloomMipChain[i - 1].GetImageView(),
                        .sampler = m_bloomSampler,
                        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                    },
                    {
                        .binding = 1,
                        .type = DescriptorType::StorageImage,
                        .imageView = m_bloomMipChain[i].GetImageView(),
                        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                    },
                });
        }

        const uint32_t upsampleSets = mipLevels - 1;
        m_bloomUpsampleSets.resize(upsampleSets);
        // Iterate in reverse order because we upsample from the smallest mip level
        for (uint32_t i = 0; i < upsampleSets; i++)
        {
            m_bloomUpsampleSets[i] = VkInit::CreateDescriptorSet(
                m_context.GetDevice(), m_descriptorPool, m_bloomSetLayout,
                {
                    {
                        .binding = 0,
                        .type = DescriptorType::CombinedImageSampler,
                        .imageView = m_bloomMipChain[mipLevels - i - 1].GetImageView(),
                        .sampler = m_bloomSampler,
                        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                    },
                    {
                        .binding = 1,
                        .type = DescriptorType::StorageImage,
                        .imageView = m_bloomMipChain[mipLevels - i - 2].GetImageView(),
                        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                    },
                });
        }

        return true;
    }

    bool VulkanRenderer::InitializeBloomPipeline()
    {
        // Initialize bloom compute pipeline
        m_bloomDownsampleShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/bloom_downsample.comp.spv"});
        m_bloomDownsamplePipeline = VulkanComputePipeline::Create(
            m_context,
            {
                .shader = m_bloomDownsampleShader,
                .bindings = {
                    {
                        .binding = 0,
                        .type = DescriptorType::CombinedImageSampler,
                        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                    },
                    {
                        .binding = 1,
                        .type = DescriptorType::StorageImage,
                        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                    },
                },
                .pushConstantRanges = {
                    {
                        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                        .offset = 0,
                        .size = sizeof(uint32_t),
                    },
                },
            });

        m_bloomUpsampleShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/bloom_upsample.comp.spv"});
        m_bloomUpsamplePipeline = VulkanComputePipeline::Create(
            m_context,
            {
                .shader = m_bloomUpsampleShader,
                .bindings = {
                    {
                        .binding = 0,
                        .type = DescriptorType::CombinedImageSampler,
                        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                    },
                    {
                        .binding = 1,
                        .type = DescriptorType::StorageImage,
                        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                    },
                },
                .pushConstantRanges = {
                    {
                        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                        .offset = 0,
                        .size = sizeof(float),
                    },
                },
            });

        m_bloomSettings = {
            .exposure = 1.0f,
            .gamma = 2.2f,
            .bloomIntensity = 1.0f,
            .bloomThreshold = 0.04f,
        };

        return true;
    }

    bool VulkanRenderer::InitializeCommandPools()
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        poolInfo.queueFamilyIndex = m_context.GetTransferQueueFamily();
        VK_CHECK(vkCreateCommandPool(m_context.GetDevice(), &poolInfo, nullptr, &m_transferCommandPool));

        poolInfo.queueFamilyIndex = m_context.GetGraphicsQueueFamily();
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
            allocInfo.commandBufferCount = 3;
            VkCommandBuffer buffers[3];
            VK_CHECK(vkAllocateCommandBuffers(m_context.GetDevice(), &allocInfo, buffers));

            m_frames[i].commandBuffer = buffers[0];
            m_frames[i].computeCommandBuffer = buffers[1];
            m_frames[i].transferCommandBuffer = buffers[2];
        }

        return true;
    }

    bool VulkanRenderer::InitializeSyncObjects()
    {

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < c_frameOverlap; i++)
        {

            VK_CHECK(vkCreateSemaphore(m_context.GetDevice(), &semaphoreInfo, nullptr, &m_frames[i].imageAvailableSemaphore));
            VK_CHECK(vkCreateSemaphore(m_context.GetDevice(), &semaphoreInfo, nullptr, &m_frames[i].renderFinishedSemaphore));
            VK_CHECK(vkCreateSemaphore(m_context.GetDevice(), &semaphoreInfo, nullptr, &m_frames[i].computeFinishedSemaphore));
            for (size_t j = 0; j < c_maxImageTransitionsPerFrame; j++)
            {
                VK_CHECK(vkCreateSemaphore(m_context.GetDevice(), &semaphoreInfo, nullptr, &m_frames[i].imageTransitionFinishedSemaphores[j]));
            }

            VK_CHECK(vkCreateFence(m_context.GetDevice(), &fenceInfo, nullptr, &m_frames[i].inFlightFence));
            VK_CHECK(vkCreateFence(m_context.GetDevice(), &fenceInfo, nullptr, &m_frames[i].computeInFlightFence));
            VK_CHECK(vkCreateFence(m_context.GetDevice(), &fenceInfo, nullptr, &m_frames[i].transferFence));
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

        VkSamplerCreateInfo cubemapSamplerInfo{};
        cubemapSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        cubemapSamplerInfo.magFilter = VK_FILTER_LINEAR;
        cubemapSamplerInfo.minFilter = VK_FILTER_LINEAR;
        cubemapSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        cubemapSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        cubemapSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        cubemapSamplerInfo.anisotropyEnable = VK_TRUE;
        cubemapSamplerInfo.maxAnisotropy = m_context.GetDeviceProperties().limits.maxSamplerAnisotropy;
        cubemapSamplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        cubemapSamplerInfo.unnormalizedCoordinates = VK_FALSE;
        cubemapSamplerInfo.compareEnable = VK_FALSE;
        cubemapSamplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        cubemapSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        cubemapSamplerInfo.mipLodBias = 0.0f;
        cubemapSamplerInfo.minLod = 0.0f;
        cubemapSamplerInfo.maxLod = 9.0f;

        VK_CHECK(vkCreateSampler(m_context.GetDevice(), &cubemapSamplerInfo, nullptr, &m_cubemapSampler));

        // Bloom sampler
        VkSamplerCreateInfo bloomSamplerInfo{};
        bloomSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        bloomSamplerInfo.magFilter = VK_FILTER_LINEAR;
        bloomSamplerInfo.minFilter = VK_FILTER_LINEAR;
        bloomSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        bloomSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        bloomSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        bloomSamplerInfo.anisotropyEnable = VK_FALSE;
        bloomSamplerInfo.maxAnisotropy = 1.0f;
        bloomSamplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        bloomSamplerInfo.unnormalizedCoordinates = VK_FALSE;
        bloomSamplerInfo.compareEnable = VK_FALSE;
        bloomSamplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        bloomSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        bloomSamplerInfo.mipLodBias = 0.0f;
        bloomSamplerInfo.minLod = 0.0f;
        bloomSamplerInfo.maxLod = 0.25f;

        VK_CHECK(vkCreateSampler(m_context.GetDevice(), &bloomSamplerInfo, nullptr, &m_bloomSampler));

        return true;
    }

    bool VulkanRenderer::InitializeDepthBuffer()
    {
        VkFormat depthFormat = VkUtil::FindDepthFormat(m_context.GetPhysicalDevice());

        m_depthImage = VulkanImage::Create(
            m_context,
            {
                .info = {
                    .width = m_swapchain.GetExtent().width,
                    .height = m_swapchain.GetExtent().height,
                    .depth = 1,
                    .format = depthFormat,
                },
                .aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT,
                .additionalUsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            });

        m_depthImage.TransitionLayout(m_context.GetDevice(), m_commandPool, m_context.GetGraphicsQueue(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

        m_depthOutlineImage = VulkanImage::Create(
            m_context,
            {
                .info = {
                    .width = m_swapchain.GetExtent().width,
                    .height = m_swapchain.GetExtent().height,
                    .depth = 1,
                    .format = VK_FORMAT_R32_SFLOAT,
                },
                .aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
                .additionalUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            });

        m_depthOutlineImage.TransitionLayout(m_context.GetDevice(), m_commandPool, m_context.GetGraphicsQueue(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

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

        CalculateProjectionMatrix();

        m_uniformBufferData.lightDir = glm::normalize(glm::vec3(1.0f, -1.0f, 1.0f));
        m_uniformBufferData.lightColor = glm::vec3(1.0f, 1.0f, 1.0f) * 4.0f;
        // m_uniformBufferData.lightViewProjection = ComputeLightProjectionMatrix(m_uniformBufferData.proj, m_uniformBufferData.view, m_uniformBufferData.lightDir);
        UpdateShadowCascades(
            m_uniformBufferData.proj,
            m_uniformBufferData.view,
            m_camera.nearPlane,
            m_camera.farPlane,
            m_uniformBufferData.lightDir,
            m_uniformBufferData.lightViewProjections,
            m_uniformBufferData.lightCascadeSplits);

        return true;
    }

    bool VulkanRenderer::InitializeInstanceBuffers()
    {
        constexpr size_t instancesSize = sizeof(StaticInstanceData) * c_maxInstances;
        constexpr size_t skeletalInstancesSize = sizeof(SkeletalInstanceData) * c_maxSkeletalInstances;
        constexpr size_t animationInstancesSize = sizeof(AnimationInstanceData) * c_maxAnimationInstances;
        constexpr size_t spriteInstancesSize = sizeof(SpriteInstanceData) * c_maxSpriteInstances;
        constexpr size_t particleInstanceSize = sizeof(ParticleInstanceData);
        constexpr size_t particleEmitterSize = sizeof(ParticleEmitterData) * c_maxParticleEmitters;

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

            VulkanBuffer &boneOutputBuffer = m_frames[i].boneOutputBuffer;
            boneOutputBuffer = VulkanBuffer::Create({.allocator = m_context.GetAllocator(), .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, .size = sizeof(glm::mat4) * c_maxBoneOutputs, .allocationUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});
            boneOutputBuffer.Map(m_context.GetAllocator());

            VulkanBuffer &spriteInstanceBuffer = m_frames[i].spriteInstanceBuffer;
            spriteInstanceBuffer = VulkanBuffer::Create({.allocator = m_context.GetAllocator(), .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, .size = spriteInstancesSize, .allocationUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});
            spriteInstanceBuffer.Map(m_context.GetAllocator());

            VulkanBuffer &particleEmitterBuffer = m_frames[i].particleEmitterBuffer;
            particleEmitterBuffer = VulkanBuffer::Create({.allocator = m_context.GetAllocator(), .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, .size = particleEmitterSize, .allocationUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});
            particleEmitterBuffer.Map(m_context.GetAllocator());

            VulkanBuffer &particleBuffer = m_frames[i].particleInstanceBuffer;
            particleBuffer = VulkanBuffer::Create({.allocator = m_context.GetAllocator(), .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, .size = particleInstanceSize, .allocationUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});
            particleBuffer.Map(m_context.GetAllocator());

            VulkanBuffer &particleDrawCommandBuffer = m_frames[i].particleDrawCommandBuffer;
            particleDrawCommandBuffer = VulkanBuffer::Create({.allocator = m_context.GetAllocator(), .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, .size = sizeof(VkDrawIndexedIndirectCommand), .allocationUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});
            particleDrawCommandBuffer.Map(m_context.GetAllocator());

            VkDrawIndexedIndirectCommand command = {
                .indexCount = 6,
                .instanceCount = 0,
                .firstIndex = 0,
                .vertexOffset = 0,
                .firstInstance = 0,
            };
            particleDrawCommandBuffer.CopyData(&command, sizeof(VkDrawIndexedIndirectCommand));
        }

        return true;
    }

    bool VulkanRenderer::InitializeDescriptorPool()
    {
        std::array<VkDescriptorPoolSize, 4> poolSizes = {
            {
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, c_maxUniformBuffers},
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, c_maxStorageBuffers},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, c_maxCombinedImageSamplers},
                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, c_maxStorageImages},
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
                {
                    .binding = 3,
                    .type = DescriptorType::CombinedImageSampler,
                    .imageView = m_brdfLUT.GetImageView(),
                    .sampler = m_textureSampler,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                },
            };

            m_frames[i].staticDescriptorSet = VkInit::CreateDescriptorSet(m_context.GetDevice(), m_descriptorPool, m_staticSetLayout, bindings);

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
                    .type = DescriptorType::CombinedImageSampler,
                    .imageView = m_brdfLUT.GetImageView(),
                    .sampler = m_textureSampler,
                },
                {
                    .binding = 6,
                    .type = DescriptorType::StorageBuffer,
                    .buffer = m_frames[i].boneOutputBuffer.GetBuffer(),
                    .size = m_frames[i].boneOutputBuffer.GetSize(),
                },
            };

            m_frames[i].skeletalDescriptorSet = VkInit::CreateDescriptorSet(m_context.GetDevice(), m_descriptorPool, m_skeletalSetLayout, bindings);

            bindings = {
                {
                    .binding = 0,
                    .type = DescriptorType::StorageBuffer,
                    .buffer = m_frames[i].spriteInstanceBuffer.GetBuffer(),
                    .size = m_frames[i].spriteInstanceBuffer.GetSize(),
                },
            };

            m_frames[i].spriteDescriptorSet = VkInit::CreateDescriptorSet(m_context.GetDevice(), m_descriptorPool, m_spriteSetLayout, bindings);

            bindings = {
                {
                    .binding = 0,
                    .type = DescriptorType::UniformBuffer,
                    .buffer = m_frames[i].uniformBuffer.GetBuffer(),
                    .size = m_frames[i].uniformBuffer.GetSize(),
                },
            };

            m_frames[i].skyboxDescriptorSet = VkInit::CreateDescriptorSet(m_context.GetDevice(), m_descriptorPool, m_skyboxSetLayout, bindings);

            m_frames[i].skeletalComputeDescriptorSet = VkInit::CreateDescriptorSet(
                m_context.GetDevice(),
                m_descriptorPool,
                m_skeletalComputePipeline.GetDescriptorSetLayout(),
                {
                    {
                        .binding = 0,
                        .type = DescriptorType::StorageBuffer,
                        .buffer = m_frames[i].skeletalInstanceBuffer.GetBuffer(),
                        .size = m_frames[i].skeletalInstanceBuffer.GetSize(),
                    },
                    {
                        .binding = 1,
                        .type = DescriptorType::UniformBuffer,
                        .buffer = m_boneBuffer.GetBuffer(),
                        .size = m_boneBuffer.GetSize(),
                    },
                    {
                        .binding = 2,
                        .type = DescriptorType::StorageBuffer,
                        .buffer = m_animationFrameBuffer.GetBuffer(),
                        .size = m_animationFrameBuffer.GetSize(),
                    },
                    {
                        .binding = 3,
                        .type = DescriptorType::UniformBuffer,
                        .buffer = m_frames[i].animationInstanceBuffer.GetBuffer(),
                        .size = m_frames[i].animationInstanceBuffer.GetSize(),
                    },
                    {
                        .binding = 4,
                        .type = DescriptorType::StorageBuffer,
                        .buffer = m_frames[i].boneOutputBuffer.GetBuffer(),
                        .size = m_frames[i].boneOutputBuffer.GetSize(),
                    },
                });

            uint32_t otherFrameIndex = (i + 1) % c_frameOverlap;
            m_frames[i].particleUpdateDescriptorSet = VkInit::CreateDescriptorSet(
                m_context.GetDevice(),
                m_descriptorPool,
                m_particleUpdatePipeline.GetDescriptorSetLayout(),
                {
                    {
                        .binding = 0,
                        .type = DescriptorType::UniformBuffer,
                        .buffer = m_frames[i].uniformBuffer.GetBuffer(),
                        .size = m_frames[i].uniformBuffer.GetSize(),
                    },
                    {
                        .binding = 1,
                        .type = DescriptorType::StorageBuffer,
                        .buffer = m_frames[otherFrameIndex].particleInstanceBuffer.GetBuffer(),
                        .size = m_frames[otherFrameIndex].particleInstanceBuffer.GetSize(),
                    },
                    {
                        .binding = 2,
                        .type = DescriptorType::StorageBuffer,
                        .buffer = m_frames[i].particleInstanceBuffer.GetBuffer(),
                        .size = m_frames[i].particleInstanceBuffer.GetSize(),
                    },
                });

            m_frames[i].particleEmitterDescriptorSet = VkInit::CreateDescriptorSet(
                m_context.GetDevice(),
                m_descriptorPool,
                m_particleEmitterPipeline.GetDescriptorSetLayout(),
                {
                    {
                        .binding = 0,
                        .type = DescriptorType::UniformBuffer,
                        .buffer = m_frames[i].uniformBuffer.GetBuffer(),
                        .size = m_frames[i].uniformBuffer.GetSize(),
                    },
                    {
                        .binding = 1,
                        .type = DescriptorType::StorageBuffer,
                        .buffer = m_frames[i].particleEmitterBuffer.GetBuffer(),
                        .size = m_frames[i].particleEmitterBuffer.GetSize(),
                    },
                    {
                        .binding = 2,
                        .type = DescriptorType::StorageBuffer,
                        .buffer = m_frames[i].particleInstanceBuffer.GetBuffer(),
                        .size = m_frames[i].particleInstanceBuffer.GetSize(),
                    },
                });

            m_frames[i].particleSortDescriptorSet = VkInit::CreateDescriptorSet(
                m_context.GetDevice(),
                m_descriptorPool,
                m_particleSortPipeline.GetDescriptorSetLayout(),
                {
                    {
                        .binding = 0,
                        .type = DescriptorType::StorageBuffer,
                        .buffer = m_frames[i].particleInstanceBuffer.GetBuffer(),
                        .size = m_frames[i].particleInstanceBuffer.GetSize(),
                    },
                });

            m_frames[i].particleDescriptorSet = VkInit::CreateDescriptorSet(
                m_context.GetDevice(),
                m_descriptorPool,
                m_particleSetLayout,
                {
                    {
                        .binding = 0,
                        .type = DescriptorType::UniformBuffer,
                        .buffer = m_frames[i].uniformBuffer.GetBuffer(),
                        .size = m_frames[i].uniformBuffer.GetSize(),
                    },
                    {
                        .binding = 1,
                        .type = DescriptorType::StorageBuffer,
                        .buffer = m_frames[i].particleInstanceBuffer.GetBuffer(),
                        .size = m_frames[i].particleInstanceBuffer.GetSize(),
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
                        .type = DescriptorType::CombinedImageSampler,
                        .imageView = m_brdfLUT.GetImageView(),
                        .sampler = m_textureSampler,
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    },
                });

            m_frames[i].ribbonDescriptorSet = VkInit::CreateDescriptorSet(
                m_context.GetDevice(),
                m_descriptorPool,
                m_ribbonSetLayout,
                {
                    {
                        .binding = 0,
                        .type = DescriptorType::UniformBuffer,
                        .buffer = m_frames[i].uniformBuffer.GetBuffer(),
                        .size = m_frames[i].uniformBuffer.GetSize(),
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
                        .type = DescriptorType::CombinedImageSampler,
                        .imageView = m_brdfLUT.GetImageView(),
                        .sampler = m_textureSampler,
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    },
                });

            m_frames[i].compositeDescriptorSet = VkInit::CreateDescriptorSet(
                m_context.GetDevice(),
                m_descriptorPool,
                m_compositeSetLayout,
                {
                    {
                        .binding = 0,
                        .type = DescriptorType::CombinedImageSampler,
                        .imageView = m_sceneImage.GetImageView(),
                        .sampler = m_textureSampler,
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    },
                    {
                        .binding = 1,
                        .type = DescriptorType::CombinedImageSampler,
                        .imageView = m_bloomMipChain[0].GetImageView(),
                        .sampler = m_textureSampler,
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    },
                    {
                        .binding = 2,
                        .type = DescriptorType::CombinedImageSampler,
                        .imageView = m_depthOutlineImage.GetImageView(),
                        .sampler = m_textureSampler,
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    },
                });
        }

        return true;
    }

    bool VulkanRenderer::InitializeDebugMessenger()
    {
        auto DebugCallback = [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData) -> VkBool32
        {
            VulkanRenderer *renderer = reinterpret_cast<VulkanRenderer *>(pUserData);
            if (renderer->m_debugCallback)
            {
                std::string message = pCallbackData->pMessage;
                renderer->m_debugCallback(message);
            }
            else
            {
                std::cerr << "[VALIDATION LAYER]: " << pCallbackData->pMessage << std::endl;
            }
            return VK_FALSE;
        };

        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pUserData = this;
        createInfo.pfnUserCallback = DebugCallback;
        VK_CHECK(CreateDebugUtilsMessengerEXT(m_context.GetInstance(), &createInfo, nullptr, &m_debugMessenger));

        return true;
    }

    bool VulkanRenderer::InitializeResources()
    {
        // Sprite
        m_spriteQuadMesh = VulkanQuadMesh::Create(m_context, m_commandPool);

        {

            // Black 1x1 texture
            auto blackImage = VulkanImage::Create(
                m_context,
                {
                    .info = {
                        .width = 1,
                        .height = 1,
                        .depth = 1,
                        .format = VK_FORMAT_R8G8B8A8_UNORM,
                    },
                    .type = ImageType::Texture2DArray,
                    .aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
                    .additionalUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
                });

            std::vector<std::vector<MipInfo>> layers;
            layers.resize(1);
            layers[0].resize(1);
            layers[0][0].width = 1;
            layers[0][0].height = 1;
            layers[0][0].mipLevel = 0;
            layers[0][0].data = std::unique_ptr<uint8_t>(new uint8_t[4]{0, 0, 0, 0});
            blackImage.UploadData(m_context, m_commandPool, nullptr, 4 * sizeof(uint8_t), layers);

            m_resourcePool.AddImage("null", blackImage);
        }

        {
            // White 1x1 texture
            auto whiteImage = VulkanImage::Create(
                m_context,
                {
                    .info = {
                        .width = 1,
                        .height = 1,
                        .depth = 1,
                        .format = VK_FORMAT_R8G8B8A8_UNORM,
                    },
                    .type = ImageType::Texture2DArray,
                    .aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
                    .additionalUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
                });

            std::vector<std::vector<MipInfo>> layers;
            layers.resize(1);
            layers[0].resize(1);
            layers[0][0].width = 1;
            layers[0][0].height = 1;
            layers[0][0].mipLevel = 0;
            layers[0][0].data = std::unique_ptr<uint8_t>(new uint8_t[4]{255, 255, 255, 255});
            whiteImage.UploadData(m_context, m_commandPool, nullptr, 4 * sizeof(uint8_t), layers);

            m_resourcePool.AddImage("white", whiteImage);
        }

        {
            // White 1x1 sprite
            auto whiteSprite = VulkanImage::Create(
                m_context,
                {
                    .info = {
                        .width = 1,
                        .height = 1,
                        .depth = 1,
                        .format = VK_FORMAT_R8G8B8A8_UNORM,
                    },
                    .type = ImageType::Texture2D,
                    .aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
                    .additionalUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
                });

            std::vector<std::vector<MipInfo>> layers;
            layers.resize(1);
            layers[0].resize(1);
            layers[0][0].width = 1;
            layers[0][0].height = 1;
            layers[0][0].mipLevel = 0;
            layers[0][0].data = std::unique_ptr<uint8_t>(new uint8_t[4]{255, 255, 255, 255});
            whiteSprite.UploadData(m_context, m_commandPool, nullptr, 4 * sizeof(uint8_t), layers);

            m_resourcePool.AddImage("white_sprite", whiteSprite);
        }

        return true;
    }

    bool VulkanRenderer::InitializeEnvironmentMap()
    {
        m_skyboxMesh = VulkanMesh::CreateFromFile(
            {
                .device = m_context.GetDevice(),
                .commandPool = m_commandPool,
                .queue = m_context.GetGraphicsQueue(),
                .allocator = m_context.GetAllocator(),
                .filepath = std::string(VLT_ASSETS_DIR) + "/meshes/skybox.dat",
            });

        m_brdfLUT = VulkanImage::CreateFromFile(
            m_context, m_commandPool,
            {
                .filepath = std::string(VLT_ASSETS_DIR) + "/textures/brdf.dat",
            });

        return true;
    }

    VulkanImage VulkanRenderer::GenerateBRDFLUT()
    {
        auto tStart = std::chrono::high_resolution_clock::now();

        const VkFormat format = VK_FORMAT_R16G16_SFLOAT;
        const int32_t dim = 512;

        // Image
        VulkanImage image = VulkanImage::Create(
            m_context,
            {
                .info = {
                    .width = dim,
                    .height = dim,
                    .depth = 1,
                    .mipLevels = 1,
                    .format = format,
                },
                .aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
                .additionalUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            });

        // Framebuffer, render pass and pipeline
        VulkanRenderPass renderPass = VulkanRenderPass::Create(
            m_context,
            {
                .attachments = {
                    {
                        .type = VulkanRenderPass::AttachmentType::Color,
                        .format = format,
                        .samples = VK_SAMPLE_COUNT_1_BIT,
                        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                        .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    },
                },
                .dependencies = {
                    {
                        .srcSubpass = VK_SUBPASS_EXTERNAL,
                        .dstSubpass = 0,
                        .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
                        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
                    },
                    {
                        .srcSubpass = 0,
                        .dstSubpass = VK_SUBPASS_EXTERNAL,
                        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
                        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
                    },
                },
            });

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass.GetRenderPass();
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &image.GetImageView();
        framebufferInfo.width = dim;
        framebufferInfo.height = dim;
        framebufferInfo.layers = 1;

        VkFramebuffer framebuffer;
        VK_CHECK(vkCreateFramebuffer(m_context.GetDevice(), &framebufferInfo, nullptr, &framebuffer));

        VkDescriptorSetLayout descriptorSetLayout = VkInit::CreateDescriptorSetLayout(m_context.GetDevice(), {});

        VkDescriptorSet descriptorSet = VkInit::CreateDescriptorSet(m_context.GetDevice(), m_descriptorPool, descriptorSetLayout, {});

        VulkanShader vertexShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/brdf.vert.spv"});
        VulkanShader fragmentShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/brdf.frag.spv"});
        VulkanMaterialPipeline pipeline = VulkanMaterialPipeline::Create(
            m_context, renderPass,
            {
                .vertexShader = vertexShader,
                .fragmentShader = fragmentShader,
                .descriptorSetLayouts = {descriptorSetLayout},
                .bindings = {},
                .vertexDescription = {},
                .cullMode = CullMode::None,
                .depthFunction = DepthFunction::Always,
            });

        // Render
        VkClearValue clearValues[2];
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].color = {{0.0f}};

        VkRenderPassBeginInfo renderPassBeginInfo{};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.renderPass = renderPass.GetRenderPass();
        renderPassBeginInfo.renderArea.extent.width = dim;
        renderPassBeginInfo.renderArea.extent.height = dim;
        renderPassBeginInfo.clearValueCount = 2;
        renderPassBeginInfo.pClearValues = clearValues;
        renderPassBeginInfo.framebuffer = framebuffer;

        VkCommandBuffer commandBuffer = VkUtil::BeginSingleTimeCommands(m_context.GetDevice(), m_commandPool);

        vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        VkViewport viewport = {
            .x = 0.0f,
            .y = 0.0f,
            .width = (float)dim,
            .height = (float)dim,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };

        VkRect2D scissor = {
            .offset = {0, 0},
            .extent = {dim, dim},
        };

        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetPipeline());
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(commandBuffer);

        VkUtil::EndSingleTimeCommands(m_context.GetDevice(), m_commandPool, m_context.GetGraphicsQueue(), commandBuffer);

        vertexShader.Destroy(m_context);
        fragmentShader.Destroy(m_context);
        vkDestroyFramebuffer(m_context.GetDevice(), framebuffer, nullptr);
        vkDestroyDescriptorSetLayout(m_context.GetDevice(), descriptorSetLayout, nullptr);
        pipeline.Destroy(m_context);
        renderPass.Destroy(m_context);

        auto tEnd = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart).count();
        std::cout << "BRDF LUT generation took " << duration << "ms" << std::endl;

        return image;
    }

    void VulkanRenderer::RecreateSwapchain()
    {
        vkDeviceWaitIdle(m_context.GetDevice());

        DestorySwapchain();

        Window *window = m_context.GetWindow();
        auto [width, height] = window->GetExtent();

        m_swapchain.Initialize(m_context, width, height);
        InitializeDepthBuffer();
        InitializeFramebuffers();
        InitializeBloomMipChain();
        InitializeDescriptorSets();

        m_framebufferResized = false;
    }

    void VulkanRenderer::WriteComputeCommands(VkCommandBuffer commandBuffer, const RenderData &renderData)
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;                  // Optional
        beginInfo.pInheritanceInfo = nullptr; // Optional

        const FrameData &frame = m_frames[m_currentFrameIndex];

        VkUtil::BufferBarrier(
            commandBuffer,
            frame.boneOutputBuffer.GetBuffer(),
            VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        // Skeletal Animation
        {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_skeletalComputePipeline.GetPipeline());
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_skeletalComputePipeline.GetPipelineLayout(), 0, 1, &frame.skeletalComputeDescriptorSet, 0, nullptr);
            vkCmdDispatch(commandBuffer, static_cast<uint32_t>(renderData.skeletalInstances.size()), 1, 1);
        }

        // // Barrier that ensures the compute shader writes are finished before the vertex shader reads
        VkUtil::BufferBarrier(
            commandBuffer,
            frame.boneOutputBuffer.GetBuffer(),
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);

        VkUtil::BufferBarrier(
            commandBuffer,
            frame.particleInstanceBuffer.GetBuffer(),
            VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT);

        // Clear count of particles
        {
            vkCmdFillBuffer(commandBuffer, frame.particleInstanceBuffer.GetBuffer(), 0, sizeof(ParticleInstanceData), 0);
        }

        // Ensure the buffer is cleared before it is read
        VkUtil::BufferBarrier(
            commandBuffer,
            frame.particleInstanceBuffer.GetBuffer(),
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        // Particle Update
        {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_particleUpdatePipeline.GetPipeline());
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_particleUpdatePipeline.GetPipelineLayout(), 0, 1, &frame.particleUpdateDescriptorSet, 0, nullptr);
            vkCmdDispatch(commandBuffer, c_maxParticleInstances / 256, 1, 1);
        }

        // Barrier to ensure update is finished before the new particles are added
        VkUtil::BufferBarrier(
            commandBuffer,
            frame.particleInstanceBuffer.GetBuffer(),
            VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        // Particle Emitters
        {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_particleEmitterPipeline.GetPipeline());
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_particleEmitterPipeline.GetPipelineLayout(), 0, 1, &frame.particleEmitterDescriptorSet, 0, nullptr);
            vkCmdDispatch(commandBuffer, static_cast<uint32_t>(renderData.particleEmitters.size()), 1, 1);
        }

        // Ensure that the particle instances are updated before sorting
        VkUtil::BufferBarrier(
            commandBuffer,
            frame.particleInstanceBuffer.GetBuffer(),
            VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        // Particle Sort
        {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_particleSortPipeline.GetPipeline());
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_particleSortPipeline.GetPipelineLayout(), 0, 1, &frame.particleSortDescriptorSet, 0, nullptr);
            SortBuffer(c_maxParticleInstances, commandBuffer, m_particleSortPipeline, frame.particleInstanceBuffer);
        }

        // Ensure that the particle instances are sorted before copying the count to the draw command buffer
        VkUtil::BufferBarrier(
            commandBuffer,
            frame.particleInstanceBuffer.GetBuffer(),
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT);

        VkUtil::BufferBarrier(
            commandBuffer,
            frame.particleDrawCommandBuffer.GetBuffer(),
            VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT);

        // Copy the count from the buffer to the draw command buffer
        {
            VkBufferCopy region = {};
            region.size = sizeof(uint32_t);
            region.srcOffset = 0;
            region.dstOffset = offsetof(VkDrawIndirectCommand, instanceCount);

            vkCmdCopyBuffer(commandBuffer, frame.particleInstanceBuffer.GetBuffer(), frame.particleDrawCommandBuffer.GetBuffer(), 1, &region);
        }

        // Barrier for particleDrawCommandBuffer
        VkUtil::BufferBarrier(
            commandBuffer,
            frame.particleDrawCommandBuffer.GetBuffer(),
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);
    }

    enum class SortAlgorithmPart : uint32_t
    {
        LocalBMS = 0U,
        LocalDisperse = 1U,
        BigFlip = 2U,
        BigDisperse = 3U,
    };

    template <typename PushBlock>
    void DispatchSort(const PushBlock &pushBlock, uint32_t workGroupCount, VkBuffer buffer, VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout)
    {
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushBlock), &pushBlock);
        vkCmdDispatch(commandBuffer, workGroupCount, 1, 1);
        VkUtil::BufferBarrier(
            commandBuffer,
            buffer,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    }

    void VulkanRenderer::SortBuffer(uint32_t n, VkCommandBuffer commandBuffer, const VulkanComputePipeline &pipeline, const VulkanBuffer &buffer)
    {
        uint32_t workGroupSize = 128;
        uint32_t workgroupSizeX = 1;

        if (n < workGroupSize * 2)
        {
            workgroupSizeX = n / 2;
        }
        else
        {
            workgroupSizeX = workGroupSize;
        }

        const uint32_t workGroupCount = n / (workgroupSizeX * 2);
        uint32_t h = workgroupSizeX * 2;

        struct PushBlock
        {
            uint32_t h;
            SortAlgorithmPart algorithm;
            float _padding1[2];
            glm::mat4 viewProjection;
        } pushBlock = {.h = h, .algorithm = SortAlgorithmPart::LocalBMS, .viewProjection = m_uniformBufferData.proj * m_uniformBufferData.view};

        DispatchSort(pushBlock, workGroupCount, buffer.GetBuffer(), commandBuffer, pipeline.GetPipelineLayout());

        h *= 2;

        for (; h <= n; h *= 2)
        {
            pushBlock.h = h;
            pushBlock.algorithm = SortAlgorithmPart::BigFlip;
            DispatchSort(pushBlock, workGroupCount, buffer.GetBuffer(), commandBuffer, pipeline.GetPipelineLayout());
            for (uint32_t hh = h / 2; hh > 1; hh /= 2)
            {
                if (hh <= workgroupSizeX * 2)
                {
                    pushBlock.h = hh;
                    pushBlock.algorithm = SortAlgorithmPart::LocalDisperse;
                    DispatchSort(pushBlock, workGroupCount, buffer.GetBuffer(), commandBuffer, pipeline.GetPipelineLayout());
                    break;
                }
                else
                {
                    pushBlock.h = hh;
                    pushBlock.algorithm = SortAlgorithmPart::BigDisperse;
                    DispatchSort(pushBlock, workGroupCount, buffer.GetBuffer(), commandBuffer, pipeline.GetPipelineLayout());
                }
            }
        }
    }

    void VulkanRenderer::WriteGraphicsCommands(VkCommandBuffer commandBuffer, uint32_t imageIndex, const RenderData &renderData)
    {
        const FrameData &frame = m_frames[m_currentFrameIndex];

        for (uint32_t i = 0; i < c_numShadowCascades; i++)
        {
            // Shadow pass
            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = m_shadowPass.GetRenderPass();
            renderPassInfo.framebuffer = m_shadowCascadeFramebuffers[i];

            renderPassInfo.renderArea.offset = {0, 0};
            const ImageInfo &shadowMapInfo = m_shadowMap.GetInfo();
            renderPassInfo.renderArea.extent = {shadowMapInfo.width, shadowMapInfo.height};
            glm::uvec2 viewportSize = {shadowMapInfo.width, shadowMapInfo.height};

            std::array<VkClearValue, 1> clearValues{};
            clearValues[0].depthStencil = {1.0f, 0};

            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdPushConstants(commandBuffer, m_staticShadowPipeline.GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t), &i);
            DrawWithPipeline<VulkanMesh>(commandBuffer, {frame.staticDescriptorSet}, m_staticShadowPipeline, renderData.staticBatches, viewportSize, true);
            vkCmdPushConstants(commandBuffer, m_skeletalShadowPipeline.GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t), &i);
            DrawWithPipeline<VulkanSkeletalMesh>(commandBuffer, {frame.skeletalDescriptorSet}, m_skeletalShadowPipeline, renderData.skeletalBatches, viewportSize, true);

            vkCmdEndRenderPass(commandBuffer);
        }

        { // Render pass
            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = m_scenePass.GetRenderPass();
            renderPassInfo.framebuffer = m_sceneFramebuffer;

            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = m_swapchain.GetExtent();
            glm::uvec2 viewportSize = {m_swapchain.GetExtent().width, m_swapchain.GetExtent().height};

            std::array<VkClearValue, 3> clearValues{};
            clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
            clearValues[1].color = {{0.0f}};
            clearValues[2].depthStencil = {1.0f, 0};

            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            if (renderData.skybox.has_value())
            {
                const VulkanMaterialInstance &skybox = m_resourcePool.GetMaterialInstance(renderData.skybox.value());
                DrawSkybox(commandBuffer, {frame.skyboxDescriptorSet, skybox.GetDescriptorSet()}, viewportSize);
            }

            if (renderData.environmentMap.has_value())
            {
                const VulkanEnvironmentMap &environmentMap = m_resourcePool.GetEnvironmentMap(renderData.environmentMap.value());
                VkDescriptorSet environmentDescriptorSet = environmentMap.GetEnvironmentDescriptorSet();

                DrawWithPipeline<VulkanMesh>(commandBuffer, {frame.staticDescriptorSet, environmentDescriptorSet}, m_staticPipeline, renderData.staticBatches, viewportSize);
                DrawWithPipeline<VulkanSkeletalMesh>(commandBuffer, {frame.skeletalDescriptorSet, environmentDescriptorSet}, m_skeletalPipeline, renderData.skeletalBatches, viewportSize);
                if (renderData.particleAtlasMaterial.has_value())
                {
                    DrawParticles(commandBuffer, frame.particleDrawCommandBuffer, {frame.particleDescriptorSet, environmentDescriptorSet}, renderData.particleAtlasMaterial.value(), viewportSize);
                    DrawRibbons(commandBuffer, {frame.ribbonDescriptorSet, environmentDescriptorSet}, renderData.particleAtlasMaterial.value(), frame.ribbonVertexBuffer, frame.ribbonIndexBuffer, static_cast<uint32_t>(renderData.ribbonIndices.size()), viewportSize);
                }
            }

            // ClearDepthBuffer(commandBuffer, viewportSize);

            DrawLines(commandBuffer, {frame.skyboxDescriptorSet}, frame.lineVertexBuffer, static_cast<uint32_t>(renderData.lines.size()), viewportSize);

            vkCmdEndRenderPass(commandBuffer);
        }

        {
            // Transition the depth outliner image to shader read only
            VkUtil::TransitionImageLayout(
                m_context.GetDevice(),
                commandBuffer,
                m_context.GetGraphicsQueue(),
                m_depthOutlineImage.GetImage(),
                VK_FORMAT_R32_SFLOAT,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        { // Bloom pass using compute shaders
            WriteBloomDownsampleCommands(commandBuffer);
            WriteBloomUpsampleCommands(commandBuffer);

            // Transition the scene image to shader read only
            VkUtil::TransitionImageLayout(
                m_context.GetDevice(),
                commandBuffer,
                m_context.GetGraphicsQueue(),
                m_sceneImage.GetImage(),
                c_sceneImageFormat,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            // Transition the first mip level of the bloom mip chain to shader read only
            VkUtil::TransitionImageLayout(
                m_context.GetDevice(),
                commandBuffer,
                m_context.GetGraphicsQueue(),
                m_bloomMipChain[0].GetImage(),
                c_sceneImageFormat,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        { // Composite pass
            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = m_compositePass.GetRenderPass();
            renderPassInfo.framebuffer = m_swapchain.GetFramebuffer(imageIndex);

            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = m_swapchain.GetExtent();
            glm::uvec2 viewportSize = {m_swapchain.GetExtent().width, m_swapchain.GetExtent().height};

            std::array<VkClearValue, 1> clearValues{};
            clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};

            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            {
                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_compositePipeline.GetPipeline());

                VkViewport viewport{};
                viewport.x = 0.0f;
                viewport.y = 0.0f;
                viewport.width = (float)viewportSize.x;
                viewport.height = (float)viewportSize.y;

                VkRect2D scissor{};
                scissor.offset = {0, 0};
                scissor.extent = {viewportSize.x, viewportSize.y};

                vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
                vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

                VkDescriptorSet descriptorSets[] = {frame.compositeDescriptorSet};
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_compositePipeline.GetPipelineLayout(), 0, 1, descriptorSets, 0, nullptr);

                vkCmdPushConstants(commandBuffer, m_compositePipeline.GetPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(BloomSettings), &m_bloomSettings);

                vkCmdDraw(commandBuffer, 3, 1, 0, 0);
            }

            DrawWithPipeline<VulkanQuadMesh>(commandBuffer, {frame.spriteDescriptorSet}, m_spritePipeline, renderData.spriteBatches, viewportSize);
            DrawWithPipeline<VulkanQuadMesh>(commandBuffer, {frame.spriteDescriptorSet}, m_sdfPipeline, renderData.sdfBatches, viewportSize);

            vkCmdEndRenderPass(commandBuffer);
        }

        // Transition back the bloom top mip level to general
        VkUtil::TransitionImageLayout(
            m_context.GetDevice(),
            commandBuffer,
            m_context.GetGraphicsQueue(),
            m_bloomMipChain[0].GetImage(),
            c_sceneImageFormat,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_GENERAL);
    }

    template <typename MeshType>
    void VulkanRenderer::DrawWithPipeline(VkCommandBuffer commandBuffer, const std::vector<VkDescriptorSet> &descriptorSets, const VulkanMaterialPipeline &pipeline, const std::vector<RenderBatch> &batches, glm::uvec2 viewportSize, bool omitNonShadowCasters)
    {
        if (batches.empty())
        {
            return;
        }

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetPipeline());

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)viewportSize.x;
        viewport.height = (float)viewportSize.y;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {viewportSize.x, viewportSize.y};
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        // A bit of a hack for now, but we need to pass the viewport size to the shader
        if constexpr (std::is_same<MeshType, VulkanQuadMesh>::value)
        {
            vkCmdPushConstants(commandBuffer, pipeline.GetPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(viewportSize), &viewportSize);
        }

        const uint32_t numDescriptorSets = static_cast<uint32_t>(descriptorSets.size());
        for (uint32_t i = 0; i < numDescriptorSets; i++)
        {
            if (descriptorSets[i] == VK_NULL_HANDLE)
            {
                continue;
            }

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetPipelineLayout(), i, 1, &descriptorSets[i], 0, nullptr);
        }

        for (const auto &batch : batches)
        {
            uint32_t firstInstance = omitNonShadowCasters ? batch.firstInstance + batch.nonShadowCasterCount : batch.firstInstance;

            MeshDrawInfo mesh = GetMeshDrawInfo<MeshType>(batch.mesh);
            const VulkanMaterialInstance &material = m_resourcePool.GetMaterialInstance(batch.material);

            VkBuffer vertexBuffers[] = {mesh.vertexBuffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            if (pipeline.ShouldBindMaterial())
            {
                VkDescriptorSet descriptorSets[] = {material.GetDescriptorSet()};
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetPipelineLayout(), numDescriptorSets, 1, descriptorSets, 0, nullptr);

                std::vector<char> materialData = material.GetMaterialData();
                if (!materialData.empty())
                {
                    vkCmdPushConstants(commandBuffer, pipeline.GetPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, static_cast<uint32_t>(materialData.size()), materialData.data());
                }
            }

            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(mesh.indexCount), batch.instanceCount, 0, 0, firstInstance);
        }
    }

    void VulkanRenderer::DrawParticles(VkCommandBuffer commandBuffer, const VulkanBuffer &drawCommandBuffer, const std::vector<VkDescriptorSet> &descriptorSets, RenderHandle particleAtlasMaterial, glm::uvec2 viewportSize)
    {
        const std::optional<VulkanMaterialInstance> &material = m_resourcePool.GetMaterialInstance(particleAtlasMaterial);
        if (!material.has_value())
        {
            return;
        }

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_particlePipeline.GetPipeline());

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)viewportSize.x;
        viewport.height = (float)viewportSize.y;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {viewportSize.x, viewportSize.y};
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        std::vector<VkDescriptorSet> descriptorSetsCopy = descriptorSets;
        descriptorSetsCopy.push_back(material->GetDescriptorSet());

        const uint32_t numDescriptorSets = static_cast<uint32_t>(descriptorSetsCopy.size());
        for (uint32_t i = 0; i < numDescriptorSets; i++)
        {
            if (descriptorSetsCopy[i] == VK_NULL_HANDLE)
            {
                continue;
            }

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_particlePipeline.GetPipelineLayout(), i, 1, &descriptorSetsCopy[i], 0, nullptr);
        }

        std::vector<char> materialData = material->GetMaterialData();
        if (!materialData.empty())
        {
            vkCmdPushConstants(commandBuffer, m_particlePipeline.GetPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, static_cast<uint32_t>(materialData.size()), materialData.data());
        }

        static RenderHandle quadMesh = ResourcePool::CreateHandle("quad");
        MeshDrawInfo meshInfo = GetMeshDrawInfo<VulkanMesh>(quadMesh);
        VkBuffer vertexBuffers[] = {meshInfo.vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, meshInfo.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexedIndirect(commandBuffer, drawCommandBuffer.GetBuffer(), 0, 1, sizeof(VkDrawIndexedIndirectCommand));
    }

    void VulkanRenderer::DrawRibbons(VkCommandBuffer commandBuffer, const std::vector<VkDescriptorSet> &descriptorSets, RenderHandle particleAtlasMaterial, const VulkanBuffer &ribbonVertexBuffer, const VulkanBuffer &ribbonIndexBuffer, uint32_t ribbonIndexCount, glm::uvec2 viewportSize)
    {
        const std::optional<VulkanMaterialInstance> &material = m_resourcePool.GetMaterialInstance(particleAtlasMaterial);
        if (!material.has_value())
        {
            return;
        }

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ribbonPipeline.GetPipeline());

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)viewportSize.x;
        viewport.height = (float)viewportSize.y;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {viewportSize.x, viewportSize.y};
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        std::vector<VkDescriptorSet> descriptorSetsCopy = descriptorSets;
        descriptorSetsCopy.push_back(material->GetDescriptorSet());

        const uint32_t numDescriptorSets = static_cast<uint32_t>(descriptorSetsCopy.size());
        for (uint32_t i = 0; i < numDescriptorSets; i++)
        {
            if (descriptorSetsCopy[i] == VK_NULL_HANDLE)
            {
                continue;
            }

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ribbonPipeline.GetPipelineLayout(), i, 1, &descriptorSetsCopy[i], 0, nullptr);
        }

        std::vector<char> materialData = material->GetMaterialData();
        if (!materialData.empty())
        {
            vkCmdPushConstants(commandBuffer, m_ribbonPipeline.GetPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, static_cast<uint32_t>(materialData.size()), materialData.data());
        }

        VkBuffer vertexBuffers[] = {ribbonVertexBuffer.GetBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, ribbonIndexBuffer.GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(commandBuffer, ribbonIndexCount, 1, 0, 0, 0);
    }

    void VulkanRenderer::DrawLines(VkCommandBuffer commandBuffer, const std::vector<VkDescriptorSet> &descriptorSets, const VulkanBuffer &lineVertexBuffer, uint32_t lineCount, glm::uvec2 viewportSize)
    {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_linePipeline.GetPipeline());

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)viewportSize.x;
        viewport.height = (float)viewportSize.y;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {viewportSize.x, viewportSize.y};
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        const uint32_t numDescriptorSets = static_cast<uint32_t>(descriptorSets.size());
        for (uint32_t i = 0; i < numDescriptorSets; i++)
        {
            if (descriptorSets[i] == VK_NULL_HANDLE)
            {
                continue;
            }

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipeline.GetPipelineLayout(), i, 1, &descriptorSets[i], 0, nullptr);
        }

        VkBuffer vertexBuffers[] = {lineVertexBuffer.GetBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdDraw(commandBuffer, lineCount * 2, 1, 0, 0);
    }

    void VulkanRenderer::DrawSkybox(VkCommandBuffer commandBuffer, const std::vector<VkDescriptorSet> &descriptorSets, glm::uvec2 viewportSize)
    {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipeline.GetPipeline());

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)viewportSize.x;
        viewport.height = (float)viewportSize.y;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {viewportSize.x, viewportSize.y};
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        const uint32_t numDescriptorSets = static_cast<uint32_t>(descriptorSets.size());
        for (uint32_t i = 0; i < numDescriptorSets; i++)
        {
            if (descriptorSets[i] == VK_NULL_HANDLE)
            {
                continue;
            }

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipeline.GetPipelineLayout(), i, 1, &descriptorSets[i], 0, nullptr);
        }

        MeshDrawInfo meshInfo = m_skyboxMesh.GetDrawInfo();
        VkBuffer vertexBuffers[] = {meshInfo.vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, meshInfo.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(meshInfo.indexCount), 1, 0, 0, 0);
    }

    void VulkanRenderer::WriteBloomDownsampleCommands(VkCommandBuffer commandBuffer)
    {
        for (uint32_t i = 0; i < m_bloomDownsampleSets.size(); i++)
        {
            VkDescriptorSet descriptorSet = m_bloomDownsampleSets[i];

            const VulkanImage &targetImage = m_bloomMipChain[i];
            const ImageInfo &targetImageInfo = targetImage.GetInfo();

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_bloomDownsamplePipeline.GetPipeline());
            vkCmdPushConstants(commandBuffer, m_bloomDownsamplePipeline.GetPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &i);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_bloomDownsamplePipeline.GetPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);
            vkCmdDispatch(commandBuffer, targetImageInfo.width, targetImageInfo.height, 1);

            // Barrier for bloom mip chain
            VkUtil::ImageBarrier(
                commandBuffer,
                targetImage,
                {
                    .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                    .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                    .srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                    .srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                });
        }
    }

    void VulkanRenderer::WriteBloomUpsampleCommands(VkCommandBuffer commandBuffer)
    {
        const float filterRadius = 0.005f * 2.0f;

        for (uint32_t i = 0; i < m_bloomUpsampleSets.size(); i++)
        {
            VkDescriptorSet descriptorSet = m_bloomUpsampleSets[i];

            const VulkanImage &targetImage = m_bloomMipChain[m_bloomMipChain.size() - i - 2];
            const ImageInfo &targetImageInfo = targetImage.GetInfo();

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_bloomUpsamplePipeline.GetPipeline());
            vkCmdPushConstants(commandBuffer, m_bloomUpsamplePipeline.GetPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float), &filterRadius);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_bloomUpsamplePipeline.GetPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);
            vkCmdDispatch(commandBuffer, targetImageInfo.width, targetImageInfo.height, 1);

            // Barrier for bloom mip chain
            VkUtil::ImageBarrier(
                commandBuffer,
                targetImage,
                {
                    .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                    .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                    .srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                    .srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                });
        }
    }

    void VulkanRenderer::ClearDepthBuffer(VkCommandBuffer commandBuffer, glm::uvec2 viewportSize)
    {
        VkClearAttachment clearAttachment{};
        clearAttachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        clearAttachment.clearValue.depthStencil = {1.0f, 0};

        VkClearRect clearRect{};
        clearRect.rect.offset = {0, 0};
        clearRect.rect.extent = {viewportSize.x, viewportSize.y};
        clearRect.baseArrayLayer = 0;
        clearRect.layerCount = 1;

        vkCmdClearAttachments(commandBuffer, 1, &clearAttachment, 1, &clearRect);
    }

    void VulkanRenderer::CalculateProjectionMatrix()
    {
        m_uniformBufferData.proj = glm::perspective(glm::radians(m_camera.fov), (float)m_swapchain.GetExtent().width / (float)m_swapchain.GetExtent().height, m_camera.nearPlane, m_camera.farPlane);
        m_uniformBufferData.proj[1][1] *= -1;
    }

#define COPY_VECTOR_TO_BUFFER(buffer, vector, maxSize)                                                                                        \
    if (!vector.empty())                                                                                                                      \
    {                                                                                                                                         \
        if (static_cast<uint32_t>(vector.size()) > static_cast<uint32_t>(maxSize))                                                            \
        {                                                                                                                                     \
            std::cerr << #vector << ":" << vector.size() << " " << #maxSize << ":" << maxSize << std::endl;                                   \
        }                                                                                                                                     \
        buffer.CopyData(vector.data(), (glm::min)(static_cast<uint32_t>(vector.size()), static_cast<uint32_t>(maxSize)) * sizeof(vector[0])); \
    }

    void VulkanRenderer::Draw(const RenderData &renderData)
    {
        constexpr uint32_t timeout = (std::numeric_limits<uint32_t>::max)();
        const uint32_t currentFrameIndex = m_currentFrameIndex;
        const FrameData &frame = m_frames[currentFrameIndex];

        std::vector<VkSemaphore> renderWaitSemaphores = {};
        std::vector<VkPipelineStageFlags> renderWaitStages = {};
        uint32_t imageTransitionCount = ProcessImageTransitions(frame.transferCommandBuffer, frame.transferFence, frame.imageTransitionFinishedSemaphores);
        for (uint32_t i = 0; i < imageTransitionCount; i++)
        {
            renderWaitSemaphores.push_back(frame.imageTransitionFinishedSemaphores[i]);
            renderWaitStages.push_back(VK_PIPELINE_STAGE_TRANSFER_BIT);
        }

        vkWaitForFences(m_context.GetDevice(), 1, &frame.inFlightFence, VK_TRUE, timeout);

        m_resourcePool.ProcessDeletionQueue(m_context, m_currentFrameIndex);

        {
            // Uniform buffer
            UniformBufferData ubo = m_uniformBufferData;
            const glm::vec3 viewPos = m_camera.position;
            glm::mat4 cameraTransform = glm::translate(glm::mat4(1.0f), viewPos) * glm::mat4_cast(m_camera.rotation);
            ubo.view = glm::inverse(cameraTransform);
            ubo.viewPos = viewPos;
            UpdateShadowCascades(
                ubo.proj,
                ubo.view,
                m_camera.nearPlane,
                m_camera.farPlane,
                m_uniformBufferData.lightDir,
                ubo.lightViewProjections,
                ubo.lightCascadeSplits);

            static std::random_device rd;
            static std::mt19937 gen(rd());
            static std::uniform_real_distribution<float> dis(0.0f, 1.0f);
            ubo.random = dis(gen);
            ubo.pointLights = renderData.pointLights;
            m_uniformBufferData = ubo;

            frame.uniformBuffer.CopyData(&ubo, sizeof(ubo));

            COPY_VECTOR_TO_BUFFER(frame.staticInstanceBuffer, renderData.staticInstances, c_maxInstances);
            COPY_VECTOR_TO_BUFFER(frame.skeletalInstanceBuffer, renderData.skeletalInstances, c_maxSkeletalInstances);
            COPY_VECTOR_TO_BUFFER(frame.animationInstanceBuffer, renderData.animationInstances, c_maxAnimationInstances);
            COPY_VECTOR_TO_BUFFER(frame.spriteInstanceBuffer, renderData.spriteInstances, c_maxSpriteInstances);
            COPY_VECTOR_TO_BUFFER(frame.ribbonVertexBuffer, renderData.ribbonVertices, c_maxRibbonVertices);
            COPY_VECTOR_TO_BUFFER(frame.ribbonIndexBuffer, renderData.ribbonIndices, c_maxRibbonVertices);
            COPY_VECTOR_TO_BUFFER(frame.lineVertexBuffer, renderData.lines, c_maxLines);
            COPY_VECTOR_TO_BUFFER(frame.particleEmitterBuffer, renderData.particleEmitters, c_maxParticleEmitters);
        }

        uint32_t imageIndex;
        VkResult acquireResult = vkAcquireNextImageKHR(m_context.GetDevice(), m_swapchain.GetSwapchain(), timeout, frame.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            RecreateSwapchain();
            return;
        }
        else
        {
            VK_CHECK(acquireResult);
        }

        vkResetFences(m_context.GetDevice(), 1, &frame.inFlightFence);

        vkResetCommandBuffer(frame.commandBuffer, 0);
        constexpr VkCommandBufferBeginInfo c_beginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        VK_CHECK(vkBeginCommandBuffer(frame.commandBuffer, &c_beginInfo));

        WriteComputeCommands(frame.commandBuffer, renderData);
        WriteGraphicsCommands(frame.commandBuffer, imageIndex, renderData);

        VK_CHECK(vkEndCommandBuffer(frame.commandBuffer));

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        renderWaitSemaphores.push_back(frame.imageAvailableSemaphore);
        renderWaitStages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        submitInfo.waitSemaphoreCount = static_cast<uint32_t>(renderWaitSemaphores.size());
        submitInfo.pWaitSemaphores = renderWaitSemaphores.data();
        submitInfo.pWaitDstStageMask = renderWaitStages.data();

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

        VkResult presentResult = vkQueuePresentKHR(m_context.GetPresentQueue(), &presentInfo);
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || m_framebufferResized)
        {
            RecreateSwapchain();
        }
        else
        {
            VK_CHECK(presentResult);
        }

        m_currentFrameIndex = (currentFrameIndex + 1) % c_frameOverlap;
    }

    void VulkanRenderer::DestorySwapchain()
    {
        m_depthImage.Destroy(m_context);
        m_depthOutlineImage.Destroy(m_context);
        m_sceneImage.Destroy(m_context);

        for (auto &mip : m_bloomMipChain)
        {
            mip.Destroy(m_context);
        }

        vkDestroyFramebuffer(m_context.GetDevice(), m_sceneFramebuffer, nullptr);

        m_swapchain.Destroy(m_context);
    }

    void VulkanRenderer::Shutdown()
    {
        vkDeviceWaitIdle(m_context.GetDevice());

        m_spriteQuadMesh.Destroy(m_context);
        m_skyboxMesh.Destroy(m_context);

        for (size_t i = 0; i < c_frameOverlap; i++)
        {
            vkDestroySemaphore(m_context.GetDevice(), m_frames[i].imageAvailableSemaphore, nullptr);
            vkDestroySemaphore(m_context.GetDevice(), m_frames[i].renderFinishedSemaphore, nullptr);
            vkDestroySemaphore(m_context.GetDevice(), m_frames[i].computeFinishedSemaphore, nullptr);
            for (uint32_t j = 0; j < c_maxImageTransitionsPerFrame; j++)
            {
                vkDestroySemaphore(m_context.GetDevice(), m_frames[i].imageTransitionFinishedSemaphores[j], nullptr);
            }
            vkDestroyFence(m_context.GetDevice(), m_frames[i].inFlightFence, nullptr);
            vkDestroyFence(m_context.GetDevice(), m_frames[i].computeInFlightFence, nullptr);
            vkDestroyFence(m_context.GetDevice(), m_frames[i].transferFence, nullptr);

            vkFreeCommandBuffers(m_context.GetDevice(), m_commandPool, 1, &m_frames[i].commandBuffer);
            vkFreeCommandBuffers(m_context.GetDevice(), m_commandPool, 1, &m_frames[i].computeCommandBuffer);
            vkFreeCommandBuffers(m_context.GetDevice(), m_commandPool, 1, &m_frames[i].transferCommandBuffer);

            m_frames[i].uniformBuffer.Unmap(m_context.GetAllocator());
            m_frames[i].uniformBuffer.Destroy(m_context.GetAllocator());

            m_frames[i].staticInstanceBuffer.Unmap(m_context.GetAllocator());
            m_frames[i].staticInstanceBuffer.Destroy(m_context.GetAllocator());

            m_frames[i].skeletalInstanceBuffer.Unmap(m_context.GetAllocator());
            m_frames[i].skeletalInstanceBuffer.Destroy(m_context.GetAllocator());

            m_frames[i].animationInstanceBuffer.Unmap(m_context.GetAllocator());
            m_frames[i].animationInstanceBuffer.Destroy(m_context.GetAllocator());

            m_frames[i].boneOutputBuffer.Unmap(m_context.GetAllocator());
            m_frames[i].boneOutputBuffer.Destroy(m_context.GetAllocator());

            m_frames[i].spriteInstanceBuffer.Unmap(m_context.GetAllocator());
            m_frames[i].spriteInstanceBuffer.Destroy(m_context.GetAllocator());

            m_frames[i].particleInstanceBuffer.Unmap(m_context.GetAllocator());
            m_frames[i].particleInstanceBuffer.Destroy(m_context.GetAllocator());

            m_frames[i].particleEmitterBuffer.Unmap(m_context.GetAllocator());
            m_frames[i].particleEmitterBuffer.Destroy(m_context.GetAllocator());

            m_frames[i].particleDrawCommandBuffer.Unmap(m_context.GetAllocator());
            m_frames[i].particleDrawCommandBuffer.Destroy(m_context.GetAllocator());

            m_frames[i].ribbonVertexBuffer.Unmap(m_context.GetAllocator());
            m_frames[i].ribbonVertexBuffer.Destroy(m_context.GetAllocator());
            m_frames[i].ribbonIndexBuffer.Unmap(m_context.GetAllocator());
            m_frames[i].ribbonIndexBuffer.Destroy(m_context.GetAllocator());

            m_frames[i].lineVertexBuffer.Unmap(m_context.GetAllocator());
            m_frames[i].lineVertexBuffer.Destroy(m_context.GetAllocator());
        }

        vkDestroySampler(m_context.GetDevice(), m_textureSampler, nullptr);
        vkDestroySampler(m_context.GetDevice(), m_shadowSampler, nullptr);
        vkDestroySampler(m_context.GetDevice(), m_cubemapSampler, nullptr);
        vkDestroySampler(m_context.GetDevice(), m_bloomSampler, nullptr);

        m_shadowMap.Destroy(m_context);
        m_brdfLUT.Destroy(m_context);

        m_resourcePool.Destroy(m_context);
        m_staticVertexShader.Destroy(m_context);
        m_skeletalVertexShader.Destroy(m_context);
        m_fragmentShader.Destroy(m_context);
        m_staticShadowVertexShader.Destroy(m_context);
        m_skeletalShadowVertexShader.Destroy(m_context);
        m_shadowFragmentShader.Destroy(m_context);
        m_spriteVertexShader.Destroy(m_context);
        m_spriteFragmentShader.Destroy(m_context);
        m_sdfFragmentShader.Destroy(m_context);
        m_skyboxVertexShader.Destroy(m_context);
        m_skyboxFragmentShader.Destroy(m_context);
        m_skeletalComputeShader.Destroy(m_context);
        m_particleEmitterShader.Destroy(m_context);
        m_particleUpdateShader.Destroy(m_context);
        m_particleSortShader.Destroy(m_context);
        m_particleVertexShader.Destroy(m_context);
        m_ribbonVertexShader.Destroy(m_context);
        m_lineVertexShader.Destroy(m_context);
        m_lineFragmentShader.Destroy(m_context);
        m_bloomDownsampleShader.Destroy(m_context);
        m_bloomUpsampleShader.Destroy(m_context);
        m_compositeVertexShader.Destroy(m_context);
        m_compositeFragmentShader.Destroy(m_context);

        vkDestroyCommandPool(m_context.GetDevice(), m_commandPool, nullptr);
        vkDestroyCommandPool(m_context.GetDevice(), m_transferCommandPool, nullptr);

        vkDestroyDescriptorPool(m_context.GetDevice(), m_descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(m_context.GetDevice(), m_staticSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_context.GetDevice(), m_skeletalSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_context.GetDevice(), m_spriteSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_context.GetDevice(), m_skyboxSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_context.GetDevice(), m_environmentSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_context.GetDevice(), m_particleSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_context.GetDevice(), m_ribbonSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_context.GetDevice(), m_bloomSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_context.GetDevice(), m_compositeSetLayout, nullptr);

        m_boneBuffer.Destroy(m_context.GetAllocator());
        m_animationFrameBuffer.Destroy(m_context.GetAllocator());

        m_staticPipeline.Destroy(m_context);
        m_skeletalPipeline.Destroy(m_context);
        m_staticShadowPipeline.Destroy(m_context);
        m_skeletalShadowPipeline.Destroy(m_context);
        m_spritePipeline.Destroy(m_context);
        m_sdfPipeline.Destroy(m_context);
        m_skyboxPipeline.Destroy(m_context);
        m_particlePipeline.Destroy(m_context);
        m_skeletalComputePipeline.Destroy(m_context);
        m_particleEmitterPipeline.Destroy(m_context);
        m_particleUpdatePipeline.Destroy(m_context);
        m_particleSortPipeline.Destroy(m_context);
        m_ribbonPipeline.Destroy(m_context);
        m_linePipeline.Destroy(m_context);
        m_bloomDownsamplePipeline.Destroy(m_context);
        m_bloomUpsamplePipeline.Destroy(m_context);
        m_compositePipeline.Destroy(m_context);

        m_scenePass.Destroy(m_context);
        m_shadowPass.Destroy(m_context);
        m_compositePass.Destroy(m_context);
        for (uint32_t i = 0; i < c_numShadowCascades; i++)
        {
            vkDestroyImageView(m_context.GetDevice(), m_shadowCascadeImageViews[i], nullptr);
            vkDestroyFramebuffer(m_context.GetDevice(), m_shadowCascadeFramebuffers[i], nullptr);
        }
        DestorySwapchain();

        if (c_validationLayersEnabled)
        {
            DestroyDebugUtilsMessengerEXT(m_context.GetInstance(), m_debugMessenger, nullptr);
        }

        m_context.Destroy();
    }

    RenderHandle VulkanRenderer::LoadMesh(const std::string &filepath, bool keepInMemory)
    {
        if (OptionalRenderHandle handle = m_resourcePool.TryAcquireResource(filepath))
        {
            return handle.value();
        }

        VulkanMesh mesh = VulkanMesh::CreateFromFile(
            {
                .device = m_context.GetDevice(),
                .commandPool = m_transferCommandPool,
                .queue = m_context.GetTransferQueue(),
                .allocator = m_context.GetAllocator(),
                .filepath = filepath,
                .keepInMemory = keepInMemory,
            });

        return m_resourcePool.AddMesh(filepath, std::move(mesh));
    }

    RenderHandle VulkanRenderer::LoadQuad(const std::string &name)
    {
        if (OptionalRenderHandle handle = m_resourcePool.TryAcquireResource(name))
        {
            return handle.value();
        }

        std::vector<StaticMeshVertex> vertices = {
            {.position = {-1.0f, -1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f}, .texCoord = {0.0f, 0.0f, 0.0f}},
            {.position = {1.0f, -1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f}, .texCoord = {1.0f, 0.0f, 0.0f}},
            {.position = {1.0f, 1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f}, .texCoord = {1.0f, 1.0f, 0.0f}},
            {.position = {-1.0f, 1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f}, .texCoord = {0.0f, 1.0f, 0.0f}},
        };

        std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};

        VulkanMesh mesh = VulkanMesh::Create({
            .device = m_context.GetDevice(),
            .commandPool = m_transferCommandPool,
            .queue = m_context.GetTransferQueue(),
            .allocator = m_context.GetAllocator(),
            .vertices = vertices,
            .indices = indices,
        });

        return m_resourcePool.AddMesh(name, std::move(mesh));
    }

    RenderHandle VulkanRenderer::LoadSkeletalMesh(const std::string &filepath)
    {
        if (OptionalRenderHandle handle = m_resourcePool.TryAcquireResource(filepath))
        {
            return handle.value();
        }

        VulkanSkeletalMesh mesh = VulkanSkeletalMesh::CreateFromFile(m_context, m_transferCommandPool, m_bones, {.filepath = filepath});

        return m_resourcePool.AddSkeletalMesh(filepath, std::move(mesh));
    }

    RenderHandle VulkanRenderer::LoadAnimation(const std::string &filepath)
    {
        if (OptionalRenderHandle handle = m_resourcePool.TryAcquireResource(filepath))
        {
            return handle.value();
        }

        return m_resourcePool.AddAnimation(filepath, VulkanAnimation::CreateFromFile(m_animationFrames, {.filepath = filepath}));
    }

    RenderHandle VulkanRenderer::LoadImage(const std::string &filepath, ImageType type, bool useAllMips)
    {
        if (OptionalRenderHandle handle = m_resourcePool.TryAcquireResource(filepath))
        {
            return handle.value();
        }

        VulkanImage image = VulkanImage::CreateFromFile(
            m_context, m_transferCommandPool,
            {
                .type = type,
                .useAllMips = useAllMips,
                .filepath = filepath,
                .imageTransitionQueue = &m_imageTransitionQueue,
            });

        return m_resourcePool.AddImage(filepath, std::move(image));
    }

    RenderHandle VulkanRenderer::LoadFontAtlas(const std::string &filepath)
    {
        if (OptionalRenderHandle handle = m_resourcePool.TryAcquireResource(filepath))
        {
            return handle.value();
        }

        auto fontAtlas = VulkanFontAtlas::CreateFromFile(
            m_context, m_transferCommandPool,
            {
                .filepath = filepath,
                .imageTransitionQueue = &m_imageTransitionQueue,
            });

        return m_resourcePool.AddFontAtlas(filepath, std::move(fontAtlas));
    }

    RenderHandle VulkanRenderer::LoadEnvironmentMap(const std::string &name, const std::string &irradianceFilepath, const std::string &prefilteredFilepath, const VolumeData &irradianceVolumeData, const std::vector<glm::vec3> &probePositions)
    {
        if (OptionalRenderHandle handle = m_resourcePool.TryAcquireResource(name))
        {
            return handle.value();
        }

        VulkanEnvironmentMap environmentMap = VulkanEnvironmentMap::CreateFromFile(
            m_context,
            m_transferCommandPool,
            m_descriptorPool,
            m_environmentSetLayout,
            m_cubemapSampler,
            {
                .irradianceFilepath = irradianceFilepath,
                .prefilteredFilepath = prefilteredFilepath,
                .irradianceVolumeData = irradianceVolumeData,
                .probePositions = probePositions,
                .imageTransitionQueue = &m_imageTransitionQueue,
            });

        return m_resourcePool.AddEnvironmentMap(name, std::move(environmentMap));
    }

    RenderHandle VulkanRenderer::GenerateIrradianceMap(RenderHandle environmentImage, const std::string &name)
    {
        VulkanImage environment = m_resourcePool.GetImage(environmentImage);
        VulkanImage irradiance = VulkanEnvironmentMap::GenerateIrradianceMap(m_context, m_commandPool, m_descriptorPool, m_skyboxMesh, environment);

        return m_resourcePool.AddImage(name, std::move(irradiance));
    }

    std::vector<SHData> VulkanRenderer::GenerateIrradianceSHs(RenderHandle environmentImageArray)
    {
        VulkanImage environment = m_resourcePool.GetImage(environmentImageArray);
        return VulkanEnvironmentMap::GenerateIrradianceSHs(m_context, m_commandPool, m_descriptorPool, m_skyboxMesh, environment);
    }

    RenderHandle VulkanRenderer::GenerateCubemapFromSHs(const std::vector<SHData> &shs, const std::string &name)
    {
        VulkanImage cubemap = VulkanEnvironmentMap::GenerateCubemapFromSHs(m_context, m_commandPool, m_descriptorPool, m_skyboxMesh, shs);

        return m_resourcePool.AddImage(name, std::move(cubemap));
    }

    RenderHandle VulkanRenderer::GeneratePrefilteredMap(RenderHandle environmentImage, const std::string &name)
    {
        VulkanImage environment = m_resourcePool.GetImage(environmentImage);
        VulkanImage prefiltered = VulkanEnvironmentMap::GeneratePrefilteredMap(m_context, m_commandPool, m_descriptorPool, m_skyboxMesh, environment);

        return m_resourcePool.AddImage(name, std::move(prefiltered));
    }

    RenderHandle VulkanRenderer::CreateCaptureCubemap(const std::string &name, uint32_t width, uint32_t height, uint32_t numCubemaps)
    {
        if (OptionalRenderHandle handle = m_resourcePool.TryAcquireResource(name))
        {
            return handle.value();
        }

        VulkanImage cubemap = VulkanImage::Create(
            m_context,
            {
                .info = {
                    .width = width,
                    .height = height,
                    .mipLevels = 1,
                    .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                    .layers = numCubemaps * 6,
                },
                .type = numCubemaps > 1 ? ImageType::CubemapArray : ImageType::Cubemap,
                .additionalUsageFlags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            });

        VkCommandBuffer commandBuffer = VkUtil::BeginSingleTimeCommands(m_context.GetDevice(), m_commandPool);

        VkUtil::ImageBarrier(
            commandBuffer,
            cubemap,
            {
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .srcAccessMask = 0,
                .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            });

        VkUtil::EndSingleTimeCommands(m_context.GetDevice(), m_commandPool, m_context.GetTransferQueue(), commandBuffer);

        return m_resourcePool.AddImage(name, std::move(cubemap));
    }

    void VulkanRenderer::CaptureSceneToCubemap(RenderHandle cubemap, uint32_t faceIndex)
    {
        // Wait for the current frame to finish rendering
        vkWaitForFences(m_context.GetDevice(), 1, &m_frames[m_currentFrameIndex].inFlightFence, VK_TRUE, (std::numeric_limits<uint32_t>::max)());

        VulkanImage cubemapImage = m_resourcePool.GetImage(cubemap);

        VkCommandBuffer commandBuffer = VkUtil::BeginSingleTimeCommands(m_context.GetDevice(), m_commandPool);

        VkUtil::ImageBarrier(
            commandBuffer,
            m_sceneImage.GetImage(),
            1,
            1,
            {
                .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            });

        VkImageSubresourceLayers subresourceLayers{};
        subresourceLayers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceLayers.mipLevel = 0;
        subresourceLayers.baseArrayLayer = faceIndex;
        subresourceLayers.layerCount = 1;

        VkImageBlit blit{};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = 0;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {static_cast<int32_t>(m_sceneImage.GetInfo().width), static_cast<int32_t>(m_sceneImage.GetInfo().height), 1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = 0;
        blit.dstSubresource.baseArrayLayer = faceIndex;
        blit.dstSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {static_cast<int32_t>(cubemapImage.GetInfo().width), static_cast<int32_t>(cubemapImage.GetInfo().height), 1};

        vkCmdBlitImage(
            commandBuffer,
            m_sceneImage.GetImage(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            cubemapImage.GetImage(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &blit,
            VK_FILTER_LINEAR);

        VkUtil::ImageBarrier(
            commandBuffer,
            m_sceneImage.GetImage(),
            1,
            1,
            {
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            });

        VkUtil::EndSingleTimeCommands(m_context.GetDevice(), m_commandPool, m_context.GetGraphicsQueue(), commandBuffer);
    }

    void VulkanRenderer::SaveImage(RenderHandle imageHandle, const std::string &filepath, bool saveAsCompressed)
    {
        VulkanImage image = m_resourcePool.GetImage(imageHandle);
        VulkanImage::SaveImageToFile(m_context, m_commandPool, image, filepath, saveAsCompressed);
    }

    void VulkanRenderer::SaveScreenshot(const std::string &filepath, bool saveAsCompressed)
    {
        // Wait for the current frame to finish rendering
        vkWaitForFences(m_context.GetDevice(), 1, &m_frames[m_currentFrameIndex].inFlightFence, VK_TRUE, (std::numeric_limits<uint32_t>::max)());

        VulkanImage outputImage = VulkanImage::Create(
            m_context,
            {
                .info = {
                    .width = m_sceneImage.GetInfo().width,
                    .height = m_sceneImage.GetInfo().height,
                    .mipLevels = 1,
                    .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                },
                .additionalUsageFlags = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            });

        VkCommandBuffer commandBuffer = VkUtil::BeginSingleTimeCommands(m_context.GetDevice(), m_commandPool);

        VkUtil::ImageBarrier(
            commandBuffer,
            outputImage,
            {
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .srcAccessMask = 0,
                .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            });

        VkUtil::ImageBarrier(
            commandBuffer,
            m_sceneImage.GetImage(),
            1,
            1,
            {
                .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            });

        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {static_cast<int32_t>(m_swapchain.GetExtent().width), static_cast<int32_t>(m_swapchain.GetExtent().height), 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = 0;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {static_cast<int32_t>(m_swapchain.GetExtent().width), static_cast<int32_t>(m_swapchain.GetExtent().height), 1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = 0;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(
            commandBuffer,
            m_sceneImage.GetImage(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            outputImage.GetImage(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &blit,
            VK_FILTER_LINEAR);

        VkUtil::ImageBarrier(
            commandBuffer,
            outputImage,
            {
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            });

        VkUtil::ImageBarrier(
            commandBuffer,
            m_sceneImage.GetImage(),
            1,
            1,
            {
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
                .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            });

        VkUtil::EndSingleTimeCommands(m_context.GetDevice(), m_commandPool, m_context.GetGraphicsQueue(), commandBuffer);

        VulkanImage::SaveImageToFile(m_context, m_commandPool, outputImage, filepath, saveAsCompressed);

        // Destroy the output image
        outputImage.Destroy(m_context);
    }

    uint32_t VulkanRenderer::ProcessImageTransitions(VkCommandBuffer commandBuffer, VkFence fence, const VkSemaphore *imageTransitionFinishedSemaphores, uint32_t timeout)
    {
        if (m_imageTransitionQueue.IsEmpty())
        {
            return 0;
        }

        const std::chrono::time_point<std::chrono::high_resolution_clock> start = std::chrono::high_resolution_clock::now();
        const VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        vkResetFences(m_context.GetDevice(), 1, &fence);

        uint32_t imageCount = 0;

        ImageTransition transition;
        while (m_imageTransitionQueue.Pop(transition) && imageCount < c_maxImageTransitionsPerFrame)
        {
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

            VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

            vkCmdPipelineBarrier(commandBuffer, transition.srcStage, transition.dstStage, 0, 0, nullptr, 0, nullptr, 1, &transition.barrier);

            vkEndCommandBuffer(commandBuffer);

            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;

            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &transition.semaphore;
            submitInfo.pWaitDstStageMask = &waitDstStageMask;

            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &imageTransitionFinishedSemaphores[imageCount];

            vkQueueSubmit(m_context.GetGraphicsQueue(), 1, &submitInfo, fence);
            vkWaitForFences(m_context.GetDevice(), 1, &fence, VK_TRUE, UINT64_MAX);
            vkResetFences(m_context.GetDevice(), 1, &fence);
            vkResetCommandBuffer(commandBuffer, 0);

            vkDestroySemaphore(m_context.GetDevice(), transition.semaphore, nullptr);
            imageCount++;

            if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count() > timeout)
            {
                break;
            }
        }

        return imageCount;
    }

    void VulkanRenderer::WaitAndResetImageTransitionQueue()
    {
        while (!m_imageTransitionQueue.IsEmpty())
        {
            std::this_thread::yield();
        }

        m_imageTransitionQueue.Clear();
    }
}