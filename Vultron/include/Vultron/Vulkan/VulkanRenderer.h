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
#include "Vultron/Core/Queue.h"
#include "Vultron/Types.h"
#include "Vultron/Window.h"
#include "Vultron/Vulkan/VulkanTypes.h"
#include "Vultron/Vulkan/VulkanUtils.h"
#include "Vultron/Vulkan/VulkanAnimation.h"
#include "Vultron/Vulkan/VulkanBuffer.h"
#include "Vultron/Vulkan/VulkanComputePipeline.h"
#include "Vultron/Vulkan/VulkanContext.h"
#include "Vultron/Vulkan/VulkanMaterial.h"
#include "Vultron/Vulkan/VulkanImage.h"
#include "Vultron/Vulkan/VulkanMesh.h"
#include "Vultron/Vulkan/VulkanRenderPass.h"
#include "Vultron/Vulkan/VulkanResourcePool.h"
#include "Vultron/Vulkan/VulkanShader.h"
#include "Vultron/Vulkan/VulkanSprite.h"
#include "Vultron/Vulkan/VulkanSwapchain.h"
#include "Vultron/Vulkan/VulkanEnvironmentMap.h"

#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <array>
#include <iostream>
#include <optional>

namespace Vultron
{
    constexpr size_t c_maxInstances = 1024;
    constexpr uint32_t c_frameOverlap = 2;

    constexpr uint32_t c_maxSets = static_cast<uint32_t>(c_maxInstances * 2);
    constexpr uint32_t c_maxUniformBuffers = 2 * c_maxSets;
    constexpr uint32_t c_maxStorageBuffers = 2 * c_maxSets;
    constexpr uint32_t c_maxCombinedImageSamplers = 2 * c_maxSets;
    constexpr uint32_t c_maxStorageImages = 2 * c_maxSets;

    constexpr uint32_t c_maxSkeletalInstances = 512;
    constexpr uint32_t c_maxAnimationInstances = 4 * c_maxSkeletalInstances;
    constexpr uint32_t c_maxBones = 512;
    constexpr uint32_t c_maxAnimationFrames = 32 * 1024 * 1024;
    constexpr uint32_t c_maxBoneOutputs = c_maxBones * c_maxSkeletalInstances;

    constexpr uint32_t c_maxSpriteInstances = 1024;

    constexpr uint32_t c_maxParticleEmitters = 128;
    constexpr uint32_t c_maxParticleInstances = 4096;

    constexpr uint32_t c_maxLines = 4096;

    constexpr uint32_t c_maxBloomMipLevels = 6;

    constexpr uint32_t c_maxImageTransitionsPerFrame = 32;

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

        std::vector<char> GetMaterialData() const
        {
            return {};
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

        std::vector<char> GetMaterialData() const
        {
            return {};
        }
    };

    struct PBRMaterial
    {
        RenderHandle albedo;
        glm::vec4 albedoColor = glm::vec4(1.0f);
        RenderHandle normal;
        RenderHandle metallicRoughnessAO;
        float metallic = 1.0f;
        float roughness = 1.0f;
        float ao = 1.0f;
        bool transparent = false;

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

        std::vector<char> GetMaterialData() const
        {
            struct
            {
                glm::vec4 albedoColor;
                float metallic;
                float roughness;
                float ao;
            } materialData = {
                .albedoColor = albedoColor,
                .metallic = metallic,
                .roughness = roughness,
                .ao = ao,
            };

            std::vector<char> data;
            data.resize(sizeof(materialData));
            std::memcpy(data.data(), &materialData, sizeof(materialData));

            return data;
        }
    };

    struct FrameData
    {
        VkSemaphore imageAvailableSemaphore;
        VkSemaphore renderFinishedSemaphore;
        VkFence inFlightFence;

        VkSemaphore computeFinishedSemaphore;
        VkFence computeInFlightFence;

        VkFence transferFence;
        VkSemaphore imageTransitionFinishedSemaphores[c_maxImageTransitionsPerFrame];

        VkCommandBuffer commandBuffer;
        VkCommandBuffer computeCommandBuffer;
        VkCommandBuffer transferCommandBuffer;

        // Global scene data resources
        VulkanBuffer uniformBuffer;

        VulkanBuffer staticInstanceBuffer;
        VkDescriptorSet staticDescriptorSet;

        VulkanBuffer skeletalInstanceBuffer;
        VulkanBuffer animationInstanceBuffer;
        VkDescriptorSet skeletalDescriptorSet;
        VulkanBuffer boneOutputBuffer;
        VkDescriptorSet skeletalComputeDescriptorSet;

        VulkanBuffer spriteInstanceBuffer;
        VkDescriptorSet spriteDescriptorSet;

        VkDescriptorSet skyboxDescriptorSet;

        VulkanBuffer particleEmitterBuffer;
        VulkanBuffer particleInstanceBuffer;
        VulkanBuffer particleDrawCommandBuffer;
        VkDescriptorSet particleEmitterDescriptorSet;
        VkDescriptorSet particleUpdateDescriptorSet;
        VkDescriptorSet particleSortDescriptorSet;
        VkDescriptorSet particleDescriptorSet;

        VulkanBuffer lineVertexBuffer;

        VkDescriptorSet compositeDescriptorSet;
    };

    struct StaticInstanceData
    {
        glm::mat4 model;
        glm::vec2 texCoord;
        glm::vec2 texSize;
        glm::vec4 color;
        glm::vec4 emissiveColor;
    };

    static_assert(sizeof(StaticInstanceData) % 16 == 0);

    struct SkeletalInstanceData
    {
        glm::mat4 model;
        int32_t boneOffset;
        int32_t boneCount;
        int32_t animationInstanceOffset;
        int32_t animationInstanceCount;
        int32_t boneOutputOffset;
        std::array<int32_t, 3> bonesToIgnore = {-1, -1, -1};
        glm::vec4 color;
    };

    static_assert(sizeof(SkeletalInstanceData) % 16 == 0);

    struct AnimationInstanceData
    {
        int32_t frameOffset;
        int32_t frame1;
        int32_t frame2;
        int32_t referenceFrame;
        float timeFactor;
        float blendFactor;
        float _padding[2];
    };

    static_assert(sizeof(SkeletalInstanceData) % 16 == 0);

    struct SpriteInstanceData
    {
        glm::vec2 position;
        glm::vec2 size;
        glm::vec2 texCoord;
        glm::vec2 texSize;
        glm::vec4 color;
        glm::vec4 borderRadius;
        float rotation;
        float zOrder;
        float _padding[2];
    };

    static_assert(sizeof(SpriteInstanceData) % 16 == 0);

    struct ParticleEmitterData
    {
        glm::vec3 position;
        float lifetime;

        float lifeDuration;
        float numFrames;
        float framesPerSecond;
        float _padding0;

        glm::vec3 initialVelocity;
        float _padding1[1];
        glm::vec3 acceleration;
        float _padding2[1];

        glm::vec2 size;
        glm::vec2 sizeSpan;

        float phiSpan;
        float thetaSpan;
        float rotation;
        float _padding3[1];

        glm::vec2 texCoord;
        glm::vec2 texSize;

        glm::vec4 startColor;
        glm::vec4 endColor;

        // We use a float here to avoid having to deal with alignment issues
        float numParticles;
        float velocitySpan;
        glm::vec2 texCoordSpan;

        float scaleIn;
        float scaleOut;
        float opacityIn;
        float opacityOut;
    };

    static_assert(sizeof(ParticleEmitterData) % 16 == 0);

    struct ParticleInstanceData
    {
        struct InstanceData
        {
            glm::vec3 position;
            float lifetime;

            float lifeDuration;
            float numFrames;
            float framesPerSecond;
            float _padding0;

            glm::vec2 size;
            float rotation;
            float _padding1[1];

            glm::vec3 velocity;
            float _padding2[1];
            glm::vec3 acceleration;
            float _padding3[1];

            glm::vec2 texCoord;
            glm::vec2 texSize;

            glm::vec4 startColor;
            glm::vec4 endColor;

            float scaleIn;
            float scaleOut;
            float opacityIn;
            float opacityOut;
        };

        uint32_t numParticles;
        uint32_t _padding[3];
        InstanceData instances[c_maxParticleInstances];
    };

    static_assert(sizeof(StaticInstanceData) % 16 == 0);

    struct Camera
    {
        glm::vec3 position;
        glm::quat rotation;
        float fov = 45.0f;
    };

    struct PointLightData
    {
        glm::vec3 position = glm::vec3(0.0f);
        float radius = 0.0f;
        glm::vec4 color = glm::vec4(1.0f);
    };

    struct LineData
    {
        glm::vec3 start;
        glm::vec4 startColor;
        glm::vec3 end;
        glm::vec4 endColor;
    };

    struct UniformBufferData
    {
        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 proj = glm::mat4(1.0f);
        glm::vec3 viewPos = glm::vec3(0.0f);
        float deltaTime = 0.0f;
        glm::vec3 lightDir = glm::vec3(0.0f, -1.0f, 0.0f);
        float random = 0.0f;
        glm::vec3 lightColor = glm::vec3(1.0f);
        float _padding2;
        glm::mat4 lightViewProjection = glm::mat4(1.0f);
        std::array<PointLightData, 4> pointLights = {};
    };

    static_assert(sizeof(UniformBufferData) % 16 == 0);

    struct BloomSettings
    {
        float exposure = 1.0f;
        float gamma = 2.2f;
        float bloomIntensity = 1.0f;
        float bloomThreshold = 0.04f;
    };

    static_assert(sizeof(BloomSettings) % 16 == 0);

    struct RenderData
    {
        const std::vector<RenderBatch> &staticBatches;
        const std::vector<StaticInstanceData> &staticInstances;
        const std::vector<RenderBatch> &skeletalBatches;
        const std::vector<SkeletalInstanceData> &skeletalInstances;
        const std::vector<AnimationInstanceData> &animationInstances;
        const std::vector<RenderBatch> &spriteBatches;
        const std::vector<RenderBatch> &sdfBatches;
        const std::vector<SpriteInstanceData> &spriteInstances;
        const std::vector<ParticleEmitterData> &particleEmitters;
        const std::optional<RenderHandle> environmentMap;
        const std::optional<RenderHandle> particleAtlasMaterial;
        const std::array<PointLightData, 4> &pointLights;
        const std::vector<LineData> &lines;
    };

    class VulkanRenderer
    {
    private:
        VulkanContext m_context;

        // Transfer operations
        VkFence m_transferFence;
        VkCommandPool m_transferCommandPool;

        // Swap chain
        VulkanSwapchain m_swapchain;

        // Shadow map
        VulkanImage m_shadowMap;
        VkSampler m_shadowSampler;
        VkFramebuffer m_shadowFramebuffer;

        // Render passes
        VulkanRenderPass m_shadowPass;
        VulkanRenderPass m_scenePass;
        VulkanRenderPass m_compositePass;

        // Scene
        VulkanImage m_sceneImage;
        VulkanImage m_depthImage;
        VkFramebuffer m_sceneFramebuffer;

        // Bloom
        std::vector<VulkanImage> m_bloomMipChain;
        VkDescriptorSet m_sceneToBloomDescriptorSet;
        std::vector<VkDescriptorSet> m_bloomDownsampleSets;
        std::vector<VkDescriptorSet> m_bloomUpsampleSets;
        VkDescriptorSetLayout m_bloomSetLayout;
        VulkanShader m_bloomDownsampleShader;
        VulkanComputePipeline m_bloomDownsamplePipeline;
        VulkanShader m_bloomUpsampleShader;
        VulkanComputePipeline m_bloomUpsamplePipeline;
        VkSampler m_bloomSampler;

        // Pools
        VkCommandPool m_commandPool;
        VkDescriptorPool m_descriptorPool;

        // Skeletal animation compute pipeline
        VulkanComputePipeline m_skeletalComputePipeline;
        VulkanShader m_skeletalComputeShader;

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

        // Sprite pipeline
        VulkanMaterialPipeline m_spritePipeline;
        VulkanQuadMesh m_spriteQuadMesh = {};
        VulkanShader m_spriteVertexShader;
        VulkanShader m_spriteFragmentShader;

        // SDF pipeline
        VulkanMaterialPipeline m_sdfPipeline;
        VulkanShader m_sdfFragmentShader;

        // Skybox
        VkSampler m_cubemapSampler;
        VkDescriptorSetLayout m_skyboxSetLayout;
        VkDescriptorSetLayout m_environmentSetLayout;
        VulkanMaterialPipeline m_skyboxPipeline;
        VulkanShader m_skyboxVertexShader;
        VulkanShader m_skyboxFragmentShader;
        VulkanMesh m_skyboxMesh;
        VulkanImage m_brdfLUT;

        // Particle pipeline
        VulkanShader m_particleEmitterShader;
        VulkanComputePipeline m_particleEmitterPipeline;
        VulkanShader m_particleUpdateShader;
        VulkanComputePipeline m_particleUpdatePipeline;
        VulkanShader m_particleSortShader;
        VulkanComputePipeline m_particleSortPipeline;
        VulkanShader m_particleVertexShader;
        VulkanMaterialPipeline m_particlePipeline;
        VkDescriptorSetLayout m_particleSetLayout;

        // Line pipeline
        VulkanMaterialPipeline m_linePipeline;
        VulkanShader m_lineVertexShader;
        VulkanShader m_lineFragmentShader;

        // Composite pipeline
        VulkanMaterialPipeline m_compositePipeline;
        VulkanShader m_compositeVertexShader;
        VulkanShader m_compositeFragmentShader;
        VkDescriptorSetLayout m_compositeSetLayout;

        // Material instance resources
        UniformBufferData m_uniformBufferData{};
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

        // Image transition queue
        ImageTransitionQueue m_imageTransitionQueue = {};

        // Permanent resources
        ResourcePool m_resourcePool;

        BloomSettings m_bloomSettings;

        Camera m_camera;

        // Render pass
        bool InitializeRenderPass();
        bool InitializeFramebuffers();

        // Shadow map
        bool InitializeShadowMap();

        // Bloom
        bool InitializeBloomPipeline();

        // Material pipeline
        bool InitializeDescriptorSetLayout();
        bool InitializeGraphicsPipeline();

        // Skeletal pipeline
        bool InitializeSkeletalComputePipeline();
        bool InitializeSkeletalBuffers();

        // Particle pipeline
        bool InitializeParticlePipeline();
        bool InitializeParticleBuffers();

        // Line pipeline
        bool InitializeLinePipeline();
        bool InitializeLineBuffers();

        // Command pool
        bool InitializeCommandPools();
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

        // Swapchain
        void RecreateSwapchain(uint32_t width, uint32_t height);

        // Validation/debugging
        bool InitializeDebugMessenger();

        // Command buffer
        void WriteGraphicsCommands(VkCommandBuffer commandBuffer, uint32_t imageIndex, const RenderData &renderData);
        void WriteComputeCommands(VkCommandBuffer commandBuffer, const RenderData &renderData);
        void SortBuffer(uint32_t n, VkCommandBuffer commandBuffer, const VulkanComputePipeline &pipeline, const VulkanBuffer &buffer);
        template <typename MeshType>
        void DrawWithPipeline(VkCommandBuffer commandBuffer, const std::vector<VkDescriptorSet> &descriptorSets, const VulkanMaterialPipeline &pipeline, const std::vector<RenderBatch> &batches, glm::uvec2 viewportSize, bool omitNonShadowCasters = false);
        void DrawSkybox(VkCommandBuffer commandBuffer, const std::vector<VkDescriptorSet> &descriptorSets, glm::uvec2 viewportSize);
        void DrawParticles(VkCommandBuffer commandBuffer, const VulkanBuffer &drawCommandBuffer, const std::vector<VkDescriptorSet> &descriptorSets, RenderHandle particleAtlasMaterial, glm::uvec2 viewportSize);
        void DrawLines(VkCommandBuffer commandBuffer, const std::vector<VkDescriptorSet> &descriptorSets, const VulkanBuffer &lineVertexBuffer, uint32_t lineCount, glm::uvec2 viewportSize);
        void WriteBloomDownsampleCommands(VkCommandBuffer commandBuffer);
        void WriteBloomUpsampleCommands(VkCommandBuffer commandBuffer);
        void ClearDepthBuffer(VkCommandBuffer commandBuffer, glm::uvec2 viewportSize);
        void CalculateProjectionMatrix();

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

        void SetCamera(const Camera &camera)
        {
            bool isProjectionDirty = camera.fov != m_camera.fov;
            m_camera = camera;
            if (isProjectionDirty)
            {
                CalculateProjectionMatrix();
            }
        }
        void SetProjection(const glm::mat4 &projection) { m_uniformBufferData.proj = projection; }
        void SetDeltaTime(float deltaTime) { m_uniformBufferData.deltaTime = deltaTime; }
        void SetBloomSettings(const BloomSettings &settings) { m_bloomSettings = settings; }

        const std::vector<SkeletonBone> &GetBones() const { return m_bones; }
        const std::vector<AnimationFrame> &GetAnimationFrames() const { return m_animationFrames; }
        const glm::mat4 &GetProjectionMatrix() const { return m_uniformBufferData.proj; }
        const glm::mat4 &GetViewMatrix() const { return m_uniformBufferData.view; }
        Camera &GetCamera() { return m_camera; }
        const BloomSettings &GetBloomSettings() const { return m_bloomSettings; }

        RenderHandle LoadMesh(const std::string &filepath);
        RenderHandle LoadQuad(const std::string &name);
        RenderHandle LoadSkeletalMesh(const std::string &filepath);
        RenderHandle LoadAnimation(const std::string &filepath);
        RenderHandle LoadImage(const std::string &filepath, ImageType type = ImageType::None, bool useAllMips = false);
        RenderHandle LoadFontAtlas(const std::string &filepath);
        RenderHandle LoadEnvironmentMap(const std::string &filepath, const std::string &irradianceFilepath, const std::string &prefilteredFilepath);

        RenderHandle GenerateIrradianceMap(RenderHandle environmentImage, const std::string &name);
        RenderHandle GeneratePrefilteredMap(RenderHandle environmentImage, const std::string &name);
        void SaveImage(RenderHandle image, const std::string &filepath);

        uint32_t ProcessImageTransitions(VkCommandBuffer commandBuffer, VkFence fence, const VkSemaphore *imageTransitionFinishedSemaphores, uint32_t timeout = 16);
        void WaitAndResetImageTransitionQueue();

        const ResourcePool &GetResourcePool() const { return m_resourcePool; }

        template <typename T>
        RenderHandle CreateMaterial(const std::string &name, const T &materialCreateInfo)
        {
            std::vector<DescriptorSetBinding> bindings = materialCreateInfo.GetBindings(m_resourcePool, m_textureSampler);
            std::vector<char> materialData = materialCreateInfo.GetMaterialData();
            auto materialInstance = VulkanMaterialInstance::Create(
                m_context, m_descriptorPool, m_staticPipeline,
                {
                    bindings,
                    materialData,
                });

            return m_resourcePool.AddMaterialInstance(name, materialInstance);
        }

        template <>
        RenderHandle CreateMaterial(const std::string &name, const SpriteMaterial &materialCreateInfo)
        {
            std::vector<DescriptorSetBinding> bindings = materialCreateInfo.GetBindings(m_resourcePool, m_textureSampler);
            std::vector<char> materialData = materialCreateInfo.GetMaterialData();
            auto materialInstance = VulkanMaterialInstance::Create(
                m_context, m_descriptorPool, m_spritePipeline,
                {
                    bindings,
                    materialData,
                });

            return m_resourcePool.AddMaterialInstance(name, materialInstance);
        }

        template <>
        RenderHandle CreateMaterial(const std::string &name, const FontSpriteMaterial &materialCreateInfo)
        {
            std::vector<DescriptorSetBinding> bindings = materialCreateInfo.GetBindings(m_resourcePool, m_textureSampler);
            std::vector<char> materialData = materialCreateInfo.GetMaterialData();
            auto materialInstance = VulkanMaterialInstance::Create(
                m_context, m_descriptorPool, m_spritePipeline,
                {
                    bindings,
                    materialData,
                });

            return m_resourcePool.AddMaterialInstance(name, materialInstance);
        }

        void Destroy(RenderHandle id) { m_resourcePool.AddToDeletionQueue(id, (m_currentFrameIndex + 1) % c_frameOverlap); }

        size_t GetImageMemoryUsage() const
        {
            return VulkanImage::GetMemoryUsage();
        }

        size_t GetBufferMemoryUsage() const
        {
            return VulkanBuffer::GetMemoryUsage();
        }
    };
}
