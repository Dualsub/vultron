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

        const float zMultiplier = 1.0f; // To make sure things even outside the frustum are included
        minExtents.z = -radius * zMultiplier;
        maxExtents.z = radius * zMultiplier;

        glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);
        glm::mat4 lightProjMatrix = lightOrthoMatrix * lightViewMatrix;

        return lightProjMatrix;
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

        if (bones.size() > 0)
        {
            m_boneBuffer.UploadStaged(m_context.GetDevice(), m_commandPool, m_context.GetTransferQueue(), m_context.GetAllocator(), bones.data(), sizeof(bones[0]) * bones.size());
        }

        if (m_animationFrames.size() > 0)
        {
            m_animationFrameBuffer.UploadStaged(m_context.GetDevice(), m_commandPool, m_context.GetTransferQueue(), m_context.GetAllocator(), m_animationFrames.data(), sizeof(m_animationFrames[0]) * m_animationFrames.size());
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
                .descriptorSetLayouts = {m_staticSetLayout, m_environmentSetLayout},
                .bindings = materialBindings,
                .vertexDescription = StaticMeshVertex::GetVertexDescription(),
            });

        m_skeletalPipeline = VulkanMaterialPipeline::Create(
            m_context, m_renderPass,
            {
                .vertexShader = m_skeletalVertexShader,
                .fragmentShader = m_fragmentShader,
                .descriptorSetLayouts = {m_skeletalSetLayout, m_environmentSetLayout},
                .bindings = materialBindings,
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
                .vertexDescription = StaticMeshVertex::GetVertexDescription(),
                .cullMode = CullMode::Front,
            });

        m_skeletalShadowPipeline = VulkanMaterialPipeline::Create(
            m_context, m_shadowPass,
            {
                .vertexShader = m_skeletalShadowVertexShader,
                .fragmentShader = m_shadowFragmentShader,
                .descriptorSetLayouts = {m_skeletalSetLayout},
                .bindings = {},
                .vertexDescription = SkeletalMeshVertex::GetVertexDescription(),
                .cullMode = CullMode::Front,
            });

        // Sprite
        m_spriteVertexShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/sprite.vert.spv"});
        m_spriteFragmentShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/sprite.frag.spv"});

        m_spritePipeline = VulkanMaterialPipeline::Create(
            m_context, m_renderPass,
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
                .vertexDescription = SpriteVertex::GetVertexDescription(),
                .cullMode = CullMode::Front,
                .depthFunction = DepthFunction::Always,
            });

        // SDF
        m_sdfFragmentShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/sdf.frag.spv"});

        m_sdfPipeline = VulkanMaterialPipeline::Create(
            m_context, m_renderPass,
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
                .vertexDescription = SpriteVertex::GetVertexDescription(),
                .cullMode = CullMode::Front,
                .depthFunction = DepthFunction::Always,
            });

        // Skybox
        m_skyboxVertexShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/skybox.vert.spv"});
        m_skyboxFragmentShader = VulkanShader::CreateFromFile(m_context, {.filepath = std::string(VLT_ASSETS_DIR) + "/shaders/skybox.frag.spv"});

        m_skyboxPipeline = VulkanMaterialPipeline::Create(
            m_context, m_renderPass,
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
            m_context, m_renderPass,
            {
                .vertexShader = m_particleVertexShader,
                .fragmentShader = m_fragmentShader,
                .descriptorSetLayouts = {m_particleSetLayout, m_environmentSetLayout},
                .bindings = {
                    {
                        .binding = 0,
                        .type = DescriptorType::CombinedImageSampler,
                        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                    },
                    {
                        .binding = 1,
                        .type = DescriptorType::CombinedImageSampler,
                        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                    },
                    {
                        .binding = 2,
                        .type = DescriptorType::CombinedImageSampler,
                        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
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
            {
                .device = m_context.GetDevice(),
                .commandPool = m_commandPool,
                .queue = m_context.GetGraphicsQueue(),
                .allocator = m_context.GetAllocator(),
                .info = {
                    .width = 2048 * 2,
                    .height = 2048 * 2,
                    .depth = 1,
                    .mipLevels = 1,
                    .format = VK_FORMAT_D32_SFLOAT,
                },
                .aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT,
                .additionalUsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            });

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

    bool VulkanRenderer::InitializeCommandPools()
    {
        VkUtil::QueueFamilies families = VkUtil::QueryQueueFamilies(m_context.GetPhysicalDevice(), m_context.GetSurface());

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        poolInfo.queueFamilyIndex = families.transferFamily.value();
        VK_CHECK(vkCreateCommandPool(m_context.GetDevice(), &poolInfo, nullptr, &m_transferCommandPool));

        poolInfo.queueFamilyIndex = families.graphicsFamily.value();
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
            allocInfo.commandBufferCount = 2;
            VkCommandBuffer buffers[2];
            VK_CHECK(vkAllocateCommandBuffers(m_context.GetDevice(), &allocInfo, buffers));

            m_frames[i].commandBuffer = buffers[0];
            m_frames[i].computeCommandBuffer = buffers[1];
        }

        return true;
    }

    bool VulkanRenderer::InitializeSyncObjects()
    {

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

        // VK_CHECK(vkCreateFence(m_context.GetDevice(), &fenceInfo, nullptr, &m_transferFence));

        for (size_t i = 0; i < c_frameOverlap; i++)
        {
            VkSemaphoreCreateInfo semaphoreInfo{};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

            VK_CHECK(vkCreateSemaphore(m_context.GetDevice(), &semaphoreInfo, nullptr, &m_frames[i].imageAvailableSemaphore));
            VK_CHECK(vkCreateSemaphore(m_context.GetDevice(), &semaphoreInfo, nullptr, &m_frames[i].renderFinishedSemaphore));
            VK_CHECK(vkCreateSemaphore(m_context.GetDevice(), &semaphoreInfo, nullptr, &m_frames[i].computeFinishedSemaphore));

            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            VK_CHECK(vkCreateFence(m_context.GetDevice(), &fenceInfo, nullptr, &m_frames[i].inFlightFence));
            VK_CHECK(vkCreateFence(m_context.GetDevice(), &fenceInfo, nullptr, &m_frames[i].computeInFlightFence));
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

        m_uniformBufferData.proj = glm::perspective(glm::radians(45.0f), (float)m_swapchain.GetExtent().width / (float)m_swapchain.GetExtent().height, 0.1f, 3200.0f);
        m_uniformBufferData.proj[1][1] *= -1;

        m_uniformBufferData.lightDir = glm::normalize(glm::vec3(1.0f, -1.0f, 1.0f));
        m_uniformBufferData.lightColor = glm::vec3(1.0f, 1.0f, 1.0f) * 4.0f;
        m_uniformBufferData.lightViewProjection = ComputeLightProjectionMatrix(m_uniformBufferData.proj, m_uniformBufferData.view, m_uniformBufferData.lightDir);

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

    bool VulkanRenderer::InitializeResources()
    {
        // Sprite
        m_spriteQuadMesh = VulkanQuadMesh::Create(m_context, m_commandPool);

        // Skybox

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

        m_brdfLUT = GenerateBRDFLUT();

        return true;
    }

    VulkanImage VulkanRenderer::GenerateBRDFLUT()
    {
        auto tStart = std::chrono::high_resolution_clock::now();

        const VkFormat format = VK_FORMAT_R16G16_SFLOAT;
        const int32_t dim = 512;

        // Image
        VulkanImage image = VulkanImage::Create(
            {
                .device = m_context.GetDevice(),
                .commandPool = m_commandPool,
                .queue = m_context.GetGraphicsQueue(),
                .allocator = m_context.GetAllocator(),
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
        VkClearValue clearValues[1];
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};

        VkRenderPassBeginInfo renderPassBeginInfo{};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.renderPass = renderPass.GetRenderPass();
        renderPassBeginInfo.renderArea.extent.width = dim;
        renderPassBeginInfo.renderArea.extent.height = dim;
        renderPassBeginInfo.clearValueCount = 1;
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

    void VulkanRenderer::WriteComputeCommands(VkCommandBuffer commandBuffer, const RenderData &renderData)
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;                  // Optional
        beginInfo.pInheritanceInfo = nullptr; // Optional

        const FrameData &frame = m_frames[m_currentFrameIndex];

        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

        // Skeletal Animation
        {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_skeletalComputePipeline.GetPipeline());
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_skeletalComputePipeline.GetPipelineLayout(), 0, 1, &frame.skeletalComputeDescriptorSet, 0, nullptr);
            vkCmdDispatch(commandBuffer, static_cast<uint32_t>(renderData.skeletalInstances.size()), 1, 1);
        }

        // Clear count of particles
        {
            vkCmdFillBuffer(commandBuffer, frame.particleInstanceBuffer.GetBuffer(), 0, sizeof(ParticleInstanceData), 0);
        }

        // Barrier after clearing particle count
        {
            VkBufferMemoryBarrier bufferBarrier = {};
            bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            bufferBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufferBarrier.buffer = frame.particleInstanceBuffer.GetBuffer();
            bufferBarrier.offset = 0;
            bufferBarrier.size = VK_WHOLE_SIZE;

            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                0, nullptr,
                1, &bufferBarrier,
                0, nullptr);
        }

        // Particle Update
        {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_particleUpdatePipeline.GetPipeline());
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_particleUpdatePipeline.GetPipelineLayout(), 0, 1, &frame.particleUpdateDescriptorSet, 0, nullptr);
            vkCmdDispatch(commandBuffer, c_maxParticleInstances / 256, 1, 1);
        }

        // TODO: Change this to a buffer barrier
        // Barrier for Particle Update
        {
            VkMemoryBarrier memoryBarrier = {};
            memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                1,
                &memoryBarrier,
                0, nullptr,
                0, nullptr);
        }

        // Particle Emitters
        {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_particleEmitterPipeline.GetPipeline());
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_particleEmitterPipeline.GetPipelineLayout(), 0, 1, &frame.particleEmitterDescriptorSet, 0, nullptr);
            vkCmdDispatch(commandBuffer, static_cast<uint32_t>(renderData.particleEmitters.size()), 1, 1);
        }

        // TODO: Change this to a buffer barrier
        // Barrier for Particle Emitters
        {
            VkMemoryBarrier memoryBarrier = {};
            memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                1,
                &memoryBarrier,
                0, nullptr,
                0, nullptr);
        }

        // Particle Sort
        {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_particleSortPipeline.GetPipeline());
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_particleSortPipeline.GetPipelineLayout(), 0, 1, &frame.particleSortDescriptorSet, 0, nullptr);
            SortBuffer(c_maxParticleInstances, commandBuffer, m_particleSortPipeline, frame.particleInstanceBuffer);
        }

        // Barrier for particle instance buffer
        {
            VkBufferMemoryBarrier bufferBarrier = {};
            bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            bufferBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufferBarrier.buffer = frame.particleInstanceBuffer.GetBuffer();
            bufferBarrier.offset = 0;
            bufferBarrier.size = VK_WHOLE_SIZE;

            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0, nullptr,
                1, &bufferBarrier,
                0, nullptr);
        }

        // Copy the count from the buffer to the draw command buffer
        {
            VkBufferCopy region = {};
            region.size = sizeof(uint32_t);
            region.srcOffset = 0;
            region.dstOffset = offsetof(VkDrawIndirectCommand, instanceCount);

            vkCmdCopyBuffer(commandBuffer, frame.particleInstanceBuffer.GetBuffer(), frame.particleDrawCommandBuffer.GetBuffer(), 1, &region);
        }

        // Barrier for particleDrawCommandBuffer
        {
            VkBufferMemoryBarrier bufferBarrier = {};
            bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            bufferBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            bufferBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
            bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufferBarrier.buffer = frame.particleDrawCommandBuffer.GetBuffer();
            bufferBarrier.offset = 0;
            bufferBarrier.size = VK_WHOLE_SIZE;

            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                0,
                0, nullptr,
                1, &bufferBarrier,
                0, nullptr);
        }

        // Barrier for particleInstanceBuffer
        {
            VkBufferMemoryBarrier bufferBarrier = {};
            bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            bufferBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufferBarrier.buffer = frame.particleInstanceBuffer.GetBuffer();
            bufferBarrier.offset = 0;
            bufferBarrier.size = VK_WHOLE_SIZE;

            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                0,
                0, nullptr,
                1, &bufferBarrier,
                0, nullptr);
        }

        VK_CHECK(vkEndCommandBuffer(commandBuffer));
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

        // Barrier for buffer
        {
            VkBufferMemoryBarrier bufferBarrier = {};
            bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufferBarrier.buffer = buffer;
            bufferBarrier.offset = 0;
            bufferBarrier.size = VK_WHOLE_SIZE;

            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                0, nullptr,
                1, &bufferBarrier,
                0, nullptr);
        }
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
            const ImageInfo &shadowMapInfo = m_shadowMap.GetInfo();
            renderPassInfo.renderArea.extent = {shadowMapInfo.width, shadowMapInfo.height};
            glm::uvec2 viewportSize = {shadowMapInfo.width, shadowMapInfo.height};

            std::array<VkClearValue, 1> clearValues{};
            clearValues[0].depthStencil = {1.0f, 0};

            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            DrawWithPipeline<VulkanMesh>(commandBuffer, {frame.staticDescriptorSet}, m_staticShadowPipeline, renderData.staticBatches, viewportSize, true);
            DrawWithPipeline<VulkanSkeletalMesh>(commandBuffer, {frame.skeletalDescriptorSet}, m_skeletalShadowPipeline, renderData.skeletalBatches, viewportSize, true);

            vkCmdEndRenderPass(commandBuffer);
        }

        { // Render pass
            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = m_renderPass.GetRenderPass();
            renderPassInfo.framebuffer = m_swapchain.GetFramebuffer(imageIndex);

            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = m_swapchain.GetExtent();
            glm::uvec2 viewportSize = {m_swapchain.GetExtent().width, m_swapchain.GetExtent().height};

            std::array<VkClearValue, 2> clearValues{};
            clearValues[0].color = {{0.07f, 0.07f, 0.07f, 1.0f}};
            clearValues[1].depthStencil = {1.0f, 0};

            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            if (renderData.environmentMap.has_value())
            {
                const VulkanEnvironmentMap &environmentMap = m_resourcePool.GetEnvironmentMap(renderData.environmentMap.value());
                VkDescriptorSet environmentDescriptorSet = environmentMap.GetEnvironmentDescriptorSet();

                DrawSkybox(commandBuffer, {frame.skyboxDescriptorSet, environmentMap.GetSkyboxDescriptorSet()}, viewportSize);
                DrawWithPipeline<VulkanMesh>(commandBuffer, {frame.staticDescriptorSet, environmentDescriptorSet}, m_staticPipeline, renderData.staticBatches, viewportSize);
                DrawWithPipeline<VulkanSkeletalMesh>(commandBuffer, {frame.skeletalDescriptorSet, environmentDescriptorSet}, m_skeletalPipeline, renderData.skeletalBatches, viewportSize);
                if (renderData.particleAtlasMaterial.has_value())
                {
                    DrawParticles(commandBuffer, frame.particleDrawCommandBuffer, {frame.particleDescriptorSet, environmentDescriptorSet}, renderData.particleAtlasMaterial.value(), viewportSize);
                }
            }

            ClearDepthBuffer(commandBuffer, viewportSize);

            DrawWithPipeline<VulkanQuadMesh>(commandBuffer, {frame.spriteDescriptorSet}, m_spritePipeline, renderData.spriteBatches, viewportSize);
            DrawWithPipeline<VulkanQuadMesh>(commandBuffer, {frame.spriteDescriptorSet}, m_sdfPipeline, renderData.sdfBatches, viewportSize);

            vkCmdEndRenderPass(commandBuffer);
        }

        VK_CHECK(vkEndCommandBuffer(commandBuffer));
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

        static RenderHandle quadMesh = ResourcePool::CreateHandle("quad");
        MeshDrawInfo meshInfo = GetMeshDrawInfo<VulkanMesh>(quadMesh);
        VkBuffer vertexBuffers[] = {meshInfo.vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, meshInfo.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexedIndirect(commandBuffer, drawCommandBuffer.GetBuffer(), 0, 1, sizeof(VkDrawIndexedIndirectCommand));
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

    void VulkanRenderer::Draw(const RenderData &renderData)
    {
        constexpr uint32_t timeout = (std::numeric_limits<uint32_t>::max)();
        const uint32_t currentFrame = m_currentFrameIndex;
        const FrameData &frame = m_frames[currentFrame];

        // Compute
        vkWaitForFences(m_context.GetDevice(), 1, &frame.computeInFlightFence, VK_TRUE, timeout);

        // Uniform buffer
        UniformBufferData ubo = m_uniformBufferData;
        // Rortate forward vector to get view direction
        const glm::vec3 viewPos = m_camera.position;
        const glm::vec3 viewDir = m_camera.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        ubo.view = glm::lookAt(viewPos, viewPos + viewDir, glm::vec3(0.0f, 1.0f, 0.0f));
        ubo.viewPos = viewPos;
        ubo.lightViewProjection = ComputeLightProjectionMatrix(ubo.proj, ubo.view, ubo.lightDir);
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<float> dis(0.0f, 1.0f);
        ubo.random = dis(gen);

        m_uniformBufferData = ubo;

        frame.uniformBuffer.CopyData(&ubo, sizeof(ubo));

        // Skeletal instance buffer
        const size_t skeletalSize = sizeof(SkeletalInstanceData) * renderData.skeletalInstances.size();
        frame.skeletalInstanceBuffer.CopyData(renderData.skeletalInstances.data(), skeletalSize);

        // Animation instance buffer
        const size_t animationSize = sizeof(AnimationInstanceData) * renderData.animationInstances.size();
        frame.animationInstanceBuffer.CopyData(renderData.animationInstances.data(), animationSize);

        vkResetFences(m_context.GetDevice(), 1, &frame.computeInFlightFence);

        vkResetCommandBuffer(frame.computeCommandBuffer, 0);
        WriteComputeCommands(frame.computeCommandBuffer, renderData);

        VkSubmitInfo computeSubmitInfo{};
        computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        computeSubmitInfo.commandBufferCount = 1;
        computeSubmitInfo.pCommandBuffers = &frame.computeCommandBuffer;
        computeSubmitInfo.signalSemaphoreCount = 1;
        computeSubmitInfo.pSignalSemaphores = &frame.computeFinishedSemaphore;

        VK_CHECK(vkQueueSubmit(m_context.GetComputeQueue(), 1, &computeSubmitInfo, frame.computeInFlightFence));

        // Graphics
        vkWaitForFences(m_context.GetDevice(), 1, &frame.inFlightFence, VK_TRUE, timeout);

        // Static instance buffer
        const size_t size = sizeof(StaticInstanceData) * renderData.staticInstances.size();
        frame.staticInstanceBuffer.CopyData(renderData.staticInstances.data(), size);

        // Sprite instance buffer
        const size_t spriteSize = sizeof(SpriteInstanceData) * renderData.spriteInstances.size();
        frame.spriteInstanceBuffer.CopyData(renderData.spriteInstances.data(), spriteSize);

        // Particle emitter buffer
        const size_t emitterSize = sizeof(ParticleEmitterData) * renderData.particleEmitters.size();
        frame.particleEmitterBuffer.CopyData(renderData.particleEmitters.data(), emitterSize);

        uint32_t imageIndex;
        VK_CHECK(vkAcquireNextImageKHR(m_context.GetDevice(), m_swapchain.GetSwapchain(), timeout, frame.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex));

        vkResetFences(m_context.GetDevice(), 1, &frame.inFlightFence);

        vkResetCommandBuffer(frame.commandBuffer, 0);
        WriteGraphicsCommands(frame.commandBuffer, imageIndex, renderData);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {
            frame.computeFinishedSemaphore,
            frame.imageAvailableSemaphore};
        VkPipelineStageFlags waitStages[] = {
            VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 2;
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

        m_spriteQuadMesh.Destroy(m_context);
        m_skyboxMesh.Destroy(m_context);

        for (size_t i = 0; i < c_frameOverlap; i++)
        {
            vkDestroySemaphore(m_context.GetDevice(), m_frames[i].imageAvailableSemaphore, nullptr);
            vkDestroySemaphore(m_context.GetDevice(), m_frames[i].renderFinishedSemaphore, nullptr);
            vkDestroySemaphore(m_context.GetDevice(), m_frames[i].computeFinishedSemaphore, nullptr);
            vkDestroyFence(m_context.GetDevice(), m_frames[i].inFlightFence, nullptr);
            vkDestroyFence(m_context.GetDevice(), m_frames[i].computeInFlightFence, nullptr);

            vkFreeCommandBuffers(m_context.GetDevice(), m_commandPool, 1, &m_frames[i].commandBuffer);

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
        }

        vkDestroySampler(m_context.GetDevice(), m_textureSampler, nullptr);
        vkDestroySampler(m_context.GetDevice(), m_shadowSampler, nullptr);
        vkDestroySampler(m_context.GetDevice(), m_cubemapSampler, nullptr);

        m_depthImage.Destroy(m_context);
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

        vkDestroyCommandPool(m_context.GetDevice(), m_commandPool, nullptr);
        vkDestroyCommandPool(m_context.GetDevice(), m_transferCommandPool, nullptr);

        vkDestroyDescriptorPool(m_context.GetDevice(), m_descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(m_context.GetDevice(), m_staticSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_context.GetDevice(), m_skeletalSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_context.GetDevice(), m_spriteSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_context.GetDevice(), m_skyboxSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_context.GetDevice(), m_environmentSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_context.GetDevice(), m_particleSetLayout, nullptr);

        m_boneBuffer.Destroy(m_context.GetAllocator());
        m_animationFrameBuffer.Destroy(m_context.GetAllocator());
        m_animationInstanceBuffer.Destroy(m_context.GetAllocator());
        m_skeletalInstanceBuffer.Destroy(m_context.GetAllocator());

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
             .commandPool = m_transferCommandPool,
             .queue = m_context.GetTransferQueue(),
             .allocator = m_context.GetAllocator(),
             .filepath = filepath});

        return m_resourcePool.AddMesh(filepath, std::move(mesh));
    }

    RenderHandle VulkanRenderer::LoadQuad(const std::string &name)
    {
        std::vector<StaticMeshVertex> vertices = {
            {.position = {-1.0f, -1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f}, .texCoord = {0.0f, 0.0f}},
            {.position = {1.0f, -1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f}, .texCoord = {1.0f, 0.0f}},
            {.position = {1.0f, 1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f}, .texCoord = {1.0f, 1.0f}},
            {.position = {-1.0f, 1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f}, .texCoord = {0.0f, 1.0f}},
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
        VulkanSkeletalMesh mesh = VulkanSkeletalMesh::CreateFromFile(m_context, m_transferCommandPool, m_bones, {.filepath = filepath});

        return m_resourcePool.AddSkeletalMesh(filepath, std::move(mesh));
    }

    RenderHandle VulkanRenderer::LoadAnimation(const std::string &filepath)
    {
        return m_resourcePool.AddAnimation(filepath, VulkanAnimation::CreateFromFile(m_animationFrames, {.filepath = filepath}));
    }

    RenderHandle VulkanRenderer::LoadImage(const std::string &filepath)
    {
        VulkanImage image = VulkanImage::CreateFromFile(
            {.device = m_context.GetDevice(),
             .commandPool = m_transferCommandPool,
             .queue = m_context.GetTransferQueue(),
             .allocator = m_context.GetAllocator(),
             .filepath = filepath});

        return m_resourcePool.AddImage(filepath, std::move(image));
    }

    RenderHandle VulkanRenderer::LoadFontAtlas(const std::string &filepath)
    {
        return m_resourcePool.AddFontAtlas(filepath, VulkanFontAtlas::CreateFromFile(m_context, m_transferCommandPool, {.filepath = filepath}));
    }

    RenderHandle VulkanRenderer::LoadEnvironmentMap(const std::string &filepath)
    {
        VulkanEnvironmentMap environmentMap = VulkanEnvironmentMap::CreateFromFile(
            m_context,
            m_transferCommandPool, m_descriptorPool,
            m_skyboxMesh,
            m_environmentSetLayout, m_skyboxPipeline.GetDescriptorSetLayout(),
            m_cubemapSampler,
            {.filepath = filepath});

        return m_resourcePool.AddEnvironmentMap(filepath, std::move(environmentMap));
    }
}