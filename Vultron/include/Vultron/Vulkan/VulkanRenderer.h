#pragma once

// Undefine windows.h define for min/max
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#define GLM_FORCE_ALIGNED_GENTYPES
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include "Vultron/Core/Core.h"
#include "Vultron/Types.h"
#include "Vultron/Window.h"
#include "Vultron/Vulkan/VulkanTypes.h"
#include "Vultron/Vulkan/VulkanUtils.h"
#include "Vultron/Vulkan/VulkanAnimation.h"
#include "Vultron/Vulkan/VulkanContext.h"
#include "Vultron/Vulkan/VulkanMaterial.h"
#include "Vultron/Vulkan/VulkanBuffer.h"
#include "Vultron/Vulkan/VulkanImage.h"
#include "Vultron/Vulkan/VulkanMesh.h"
#include "Vultron/Vulkan/VulkanRenderPass.h"
#include "Vultron/Vulkan/VulkanResourcePool.h"
#include "Vultron/Vulkan/VulkanShader.h"
#include "Vultron/Vulkan/VulkanSprite.h"
#include "Vultron/Vulkan/VulkanSwapchain.h"

#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <array>
#include <iostream>
#include <optional>

namespace Vultron
{
    struct SpriteMaterial
    {
        RenderHandle texture;
        int32_t layer = 0;

        std::vector<DescriptorSetBinding> GetBindings(const ResourcePool &pool, VkSampler sampler) const
        {
            const auto &image = pool.GetImage(texture);
            return {
                {
                    .binding = 0,
                    .type = DescriptorType::CombinedImageSampler,
                    .imageView = image.GetImageView(),
                    .sampler = sampler,
                },
            };
        }
    };

    struct FontSpriteMaterial
    {
        RenderHandle fontAtlas;

        std::vector<DescriptorSetBinding> GetBindings(const ResourcePool &pool, VkSampler sampler) const
        {
            const auto &atlas = pool.GetFontAtlas(fontAtlas);
            return {
                {
                    .binding = 0,
                    .type = DescriptorType::CombinedImageSampler,
                    .imageView = atlas.GetAtlas().GetImageView(),
                    .sampler = sampler,
                },
            };
        }
    };

    struct PBRMaterial
    {
        RenderHandle albedo;
        RenderHandle normal;
        RenderHandle metallicRoughnessAO;

        std::vector<DescriptorSetBinding> GetBindings(const ResourcePool &pool, VkSampler sampler) const
        {
            const auto &albedoImage = pool.GetImage(albedo);
            const auto &normalImage = pool.GetImage(normal);
            const auto &metallicRoughnessAOImage = pool.GetImage(metallicRoughnessAO);
            return {
                {
                    .binding = 0,
                    .type = DescriptorType::CombinedImageSampler,
                    .imageView = albedoImage.GetImageView(),
                    .sampler = sampler,
                },
                {
                    .binding = 1,
                    .type = DescriptorType::CombinedImageSampler,
                    .imageView = normalImage.GetImageView(),
                    .sampler = sampler,
                },
                {
                    .binding = 2,
                    .type = DescriptorType::CombinedImageSampler,
                    .imageView = metallicRoughnessAOImage.GetImageView(),
                    .sampler = sampler,
                },
            };
        }
    };

    struct FrameData
    {
        VkSemaphore imageAvailableSemaphore;
        VkSemaphore renderFinishedSemaphore;
        VkFence inFlightFence;

        VkCommandBuffer commandBuffer;

        // Global scene data resources
        VulkanBuffer uniformBuffer;

        VulkanBuffer staticInstanceBuffer;
        VkDescriptorSet staticDescriptorSet;

        VulkanBuffer skeletalInstanceBuffer;
        VulkanBuffer animationInstanceBuffer;
        VkDescriptorSet skeletalDescriptorSet;

        VulkanBuffer spriteInstanceBuffer;
        VkDescriptorSet spriteDescriptorSet;

        VkDescriptorSet skyboxDescriptorSet;
    };

    struct StaticInstanceData
    {
        glm::mat4 model;
        glm::vec2 texCoord;
        glm::vec2 texSize;
        glm::vec4 color;
    };

    struct SkeletalInstanceData
    {
        glm::mat4 model;
        int32_t boneOffset;
        int32_t boneCount;
        int32_t animationInstanceOffset;
        int32_t animationInstanceCount;
    };

    struct AnimationInstanceData
    {
        int32_t frameOffset;
        int32_t frame1;
        int32_t frame2;
        float _padding;
        float timeFactor;
        float blendFactor;
        float _padding2[2];
    };

    struct SpriteInstanceData
    {
        glm::vec2 position;
        glm::vec2 size;
        glm::vec2 texCoord;
        glm::vec2 texSize;
        glm::vec4 color;
    };

    struct Camera
    {
        glm::vec3 position;
        glm::quat rotation;
    };

    struct UniformBufferData
    {
        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 proj = glm::mat4(1.0f);
        glm::vec3 viewPos = glm::vec3(0.0f);
        float _padding1;
        glm::vec3 lightDir = glm::vec3(0.0f, -1.0f, 0.0f);
        float _padding2;
        glm::vec3 lightColor = glm::vec3(1.0f);
        float _padding3;
        glm::mat4 lightViewProjection = glm::mat4(1.0f);
    };

    static_assert(sizeof(UniformBufferData) % 16 == 0);

    struct RenderData
    {
        const std::vector<RenderBatch> &staticBatches;
        const std::vector<StaticInstanceData> &staticInstances;
        const std::vector<RenderBatch> &skeletalBatches;
        const std::vector<SkeletalInstanceData> &skeletalInstances;
        const std::vector<AnimationInstanceData> &animationInstances;
        const std::vector<RenderBatch> &spriteBatches;
        const std::vector<SpriteInstanceData> &spriteInstances;
    };

    constexpr size_t c_maxInstances = 1024;
    constexpr uint32_t c_frameOverlap = 2;

    constexpr uint32_t c_maxSets = static_cast<uint32_t>(c_maxInstances * 2);
    constexpr uint32_t c_maxUniformBuffers = 2 * c_maxSets;
    constexpr uint32_t c_maxStorageBuffers = 2 * c_maxSets;
    constexpr uint32_t c_maxCombinedImageSamplers = 2 * c_maxSets;

    constexpr uint32_t c_maxSkeletalInstances = 512;
    constexpr uint32_t c_maxAnimationInstances = 512;
    constexpr uint32_t c_maxBones = 256;
    constexpr uint32_t c_maxAnimationFrames = 32 * 1024 * 1024;

    constexpr uint32_t c_maxSpriteInstances = 1024;

    class VulkanRenderer
    {
    private:
        VulkanContext m_context;

        // Swap chain
        VulkanSwapchain m_swapchain;

        // Shadow map
        VulkanImage m_shadowMap;
        VkSampler m_shadowSampler;
        VkFramebuffer m_shadowFramebuffer;

        // Render passes
        VulkanRenderPass m_shadowPass;
        VulkanRenderPass m_renderPass;

        // Pools
        VkCommandPool m_commandPool;
        VkDescriptorPool m_descriptorPool;

        // Static pipeline
        VulkanMaterialPipeline m_staticPipeline;
        VulkanMaterialPipeline m_staticShadowPipeline;
        VkDescriptorSetLayout m_staticSetLayout;

        VulkanShader m_staticShadowVertexShader;
        VulkanShader m_skeletalShadowVertexShader;
        VulkanShader m_shadowFragmentShader;
        VkDescriptorSetLayout m_spriteSetLayout;

        // Skeletal pipeline
        VulkanMaterialPipeline m_skeletalPipeline;
        VulkanMaterialPipeline m_skeletalShadowPipeline;
        VkDescriptorSetLayout m_skeletalSetLayout;
        // -- GPU only resources
        std::vector<SkeletonBone> m_bones;
        VulkanBuffer m_boneBuffer;
        std::vector<AnimationFrame> m_animationFrames;
        VulkanBuffer m_animationFrameBuffer;
        // -- GPU to CPU resources
        VulkanBuffer m_skeletalInstanceBuffer;
        VulkanBuffer m_animationInstanceBuffer;

        // Sprite pipeline
        VulkanMaterialPipeline m_spritePipeline;
        VulkanQuadMesh m_spriteQuadMesh = {};
        VulkanShader m_spriteVertexShader;
        VulkanShader m_spriteFragmentShader;

        // Skybox
        VkSampler m_cubemapSampler;
        VulkanImage m_skyboxImage;
        VkDescriptorSetLayout m_skyboxSetLayout;
        VulkanMaterialPipeline m_skyboxPipeline;
        VulkanShader m_skyboxVertexShader;
        VulkanShader m_skyboxFragmentShader;
        VulkanMesh m_skyboxMesh;
        VulkanImage m_brdfLUT;
        VulkanImage m_irradianceMap;
        VulkanImage m_prefilteredMap;

        // Material instance resources
        UniformBufferData m_uniformBufferData{};
        VulkanImage m_depthImage;
        VkSampler m_textureSampler;

        // Debugging
        VkDebugUtilsMessengerEXT m_debugMessenger;

        // Frame data
        FrameData m_frames[c_frameOverlap];
        uint32_t m_currentFrameIndex = 0;

        // Assets, will be removed in the future
        VulkanShader m_staticVertexShader;
        VulkanShader m_skeletalVertexShader;
        VulkanShader m_fragmentShader;

        // Permanent resources
        ResourcePool m_resourcePool;

        Camera m_camera;

        // Render pass
        bool InitializeRenderPass();
        bool InitializeFramebuffers();

        // Shadow map
        bool InitializeShadowMap();

        // Material pipeline
        bool InitializeDescriptorSetLayout();
        bool InitializeGraphicsPipeline();

        // Skeletal pipeline
        bool InitializeSkeletalBuffers();

        // Command pool
        bool InitializeCommandPool();
        bool InitializeCommandBuffer();

        bool InitializeSyncObjects();

        // Permanent resources
        bool InitializeSamplers();
        bool InitializeDepthBuffer();
        bool InitializeDescriptorPool();

        // Material instance resources
        bool InitializeUniformBuffers();
        bool InitializeInstanceBuffers();
        bool InitializeDescriptorSets();

        // Assets, will be removed in the future
        bool InitializeResources();

        bool InitializeEnvironmentMap();
        VulkanImage GenerateBRDFLUT();
        VulkanImage GenerateIrradianceMap();
        VulkanImage GeneratePrefilteredMap();

        // Swapchain
        void RecreateSwapchain(uint32_t width, uint32_t height);

        // Validation/debugging
        bool InitializeDebugMessenger();

        // Command buffer
        void WriteCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, const RenderData &renderData);
        template <typename MeshType>
        void DrawWithPipeline(VkCommandBuffer commandBuffer, VkDescriptorSet descriptorSet, const VulkanMaterialPipeline &pipeline, const std::vector<RenderBatch> &batches, glm::uvec2 viewportSize, bool omitNonShadowCasters = false);
        void DrawSkybox(VkCommandBuffer commandBuffer, VkDescriptorSet descriptorSet, glm::uvec2 viewportSize);
        void ClearDepthBuffer(VkCommandBuffer commandBuffer, glm::uvec2 viewportSize);

        template <typename MeshType>
        MeshDrawInfo GetMeshDrawInfo(RenderHandle id) const
        {
            return m_resourcePool.GetMeshDrawInfo<MeshType>(id);
        }

        template <>
        MeshDrawInfo GetMeshDrawInfo<VulkanQuadMesh>(RenderHandle id) const
        {
            return m_spriteQuadMesh.GetDrawInfo();
        }

    public:
        VulkanRenderer() = default;
        ~VulkanRenderer() = default;

        bool Initialize(const Window &window);
        void PostInitialize();
        void Draw(const RenderData &renderData);
        void Shutdown();

        void SetCamera(const Camera &camera) { m_camera = camera; }
        void SetProjection(const glm::mat4 &projection) { m_uniformBufferData.proj = projection; }
        Camera &GetCamera() { return m_camera; }

        const std::vector<SkeletonBone> &GetBones() const { return m_bones; }
        const std::vector<AnimationFrame> &GetAnimationFrames() const { return m_animationFrames; }
        const glm::mat4 &GetProjectionMatrix() const { return m_uniformBufferData.proj; }
        const glm::mat4 &GetViewMatrix() const { return m_uniformBufferData.view; }

        RenderHandle LoadMesh(const std::string &filepath);
        RenderHandle LoadQuad(const std::string &name);
        RenderHandle LoadSkeletalMesh(const std::string &filepath);
        RenderHandle LoadAnimation(const std::string &filepath);
        RenderHandle LoadImage(const std::string &filepath);
        RenderHandle LoadFontAtlas(const std::string &filepath);

        const ResourcePool &GetResourcePool() const { return m_resourcePool; }

        template <typename T>
        RenderHandle CreateMaterial(const std::string &name, const T &materialCreateInfo)
        {
            std::vector<DescriptorSetBinding> bindings = materialCreateInfo.GetBindings(m_resourcePool, m_textureSampler);
            auto materialInstance = VulkanMaterialInstance::Create(
                m_context, m_descriptorPool, m_staticPipeline,
                {
                    bindings,
                });

            return m_resourcePool.AddMaterialInstance(name, materialInstance);
        }

        template <>
        RenderHandle CreateMaterial(const std::string &name, const SpriteMaterial &materialCreateInfo)
        {
            std::vector<DescriptorSetBinding> bindings = materialCreateInfo.GetBindings(m_resourcePool, m_textureSampler);
            auto materialInstance = VulkanMaterialInstance::Create(
                m_context, m_descriptorPool, m_spritePipeline,
                {
                    bindings,
                });

            return m_resourcePool.AddMaterialInstance(name, materialInstance);
        }

        template <>
        RenderHandle CreateMaterial(const std::string &name, const FontSpriteMaterial &materialCreateInfo)
        {
            std::vector<DescriptorSetBinding> bindings = materialCreateInfo.GetBindings(m_resourcePool, m_textureSampler);
            auto materialInstance = VulkanMaterialInstance::Create(
                m_context, m_descriptorPool, m_spritePipeline,
                {
                    bindings,
                });

            return m_resourcePool.AddMaterialInstance(name, materialInstance);
        }
    };

}
