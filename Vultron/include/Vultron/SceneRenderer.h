#pragma once

#include "Vultron/Types.h"
#include "Vultron/Window.h"
#include "Vultron/Vulkan/VulkanRenderer.h"

#include <glm/glm.hpp>

#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <unordered_map>

namespace Vultron
{
    // What is sent to the backend renderer
    struct InstancedStaticRenderJob
    {
        RenderHandle mesh = {};
        RenderHandle material = {};
        std::vector<StaticInstanceData> instances = {};
        uint32_t nonShadowCasterCount = 0;
        bool transparent = false;
    };

    struct InstancedSkeletalRenderJob
    {
        RenderHandle mesh = {};
        RenderHandle material = {};
        std::vector<struct SkeletalInstanceData> instances = {};
    };

    struct InstancedSpriteRenderJob
    {
        RenderHandle material = {};
        std::vector<struct SpriteInstanceData> instances = {};
    };

    class SceneRenderer
    {
    private:
        VulkanRenderer m_backend;
        std::map<uint64_t, InstancedStaticRenderJob> m_staticJobs;
        std::map<uint64_t, InstancedSkeletalRenderJob> m_skeletalJobs;
        std::map<uint64_t, InstancedSpriteRenderJob> m_spriteJobs;
        std::map<uint64_t, InstancedSpriteRenderJob> m_fontJobs;
        std::vector<ParticleEmitterData> m_particleEmitters;
        std::vector<AnimationInstanceData> m_animationInstances;
        std::vector<DecalInstanceData> m_decalInstances;
        std::vector<LineData> m_lines;
        std::vector<RibbonVertex> m_ribbonVertices;
        std::vector<uint32_t> m_ribbonIndices;
        std::optional<RenderHandle> m_skybox;
        std::optional<RenderHandle> m_environmentMap;
        std::optional<RenderHandle> m_particleAtlasMaterial;
        std::optional<RenderHandle> m_decalAtlasMaterial;
        std::array<PointLightData, 4> m_pointLights;

        int32_t m_boneOutputOffset = 0;
        std::unordered_map<RenderHandle, int32_t> m_spriteMaterialToLayer;
        std::set<RenderHandle> m_transparentMaterials;
        RenderHandle m_quadMesh = {};

        // Function that generates vertices for a ribbon, and then appends it to the list of ribbon vertices
        void GenerateRibbonVertices(const std::vector<RibbonControlPoint> &points, const glm::vec2 &uvStart, const glm::vec2 &uvEnd, std::vector<RibbonVertex> &vertices, std::vector<uint32_t> &indices);

    public:
        SceneRenderer() = default;
        ~SceneRenderer() = default;

        bool Initialize(const Window &window);
        void PostInitialize();
        void BeginFrame();
        void SetEnvironmentMap(const std::optional<RenderHandle> &environmentMap) { m_environmentMap = environmentMap; }
        void SetSkybox(const std::optional<RenderHandle> &skybox) { m_skybox = skybox; }
        void SetParticleAtlasMaterial(const std::optional<RenderHandle> &particleAtlasMaterial) { m_particleAtlasMaterial = particleAtlasMaterial; }
        void SetDecalAtlasMaterial(const std::optional<RenderHandle> &decalAtlasMaterial) { m_decalAtlasMaterial = decalAtlasMaterial; }
        void SetPointLights(const std::array<PointLightData, 4> &pointLights) { m_pointLights = pointLights; }
        void SubmitRenderJob(const StaticRenderJob &job);
        void SubmitRenderJob(const SkeletalRenderJob &job);
        void SubmitRenderJob(const DecalRenderJob &job);
        void SubmitRenderJob(const SpriteRenderJob &job);
        void SubmitRenderJob(const FontRenderJob &job);
        void SubmitRenderJob(const ParticleEmitJob &job);
        void SubmitRenderJob(const AnimationJob &job);
        void SubmitRenderJob(const RibbonRenderJob &job);
        void SubmitRenderJob(const ParticleRenderJob &job);
        void SubmitRenderJob(const LineRenderJob &job);
        void EndFrame();
        void Shutdown();

        // TODO: This should be somewhere else
        struct AnimationTiming
        {
            uint32_t frame1 = 0;
            uint32_t frame2 = 0;
            float frameBlendFactor = 0.0f;
            float time = 0.0f;
            float duration = 0.0f;
        };

        float GetAspectRatio() const { return m_backend.GetAspectRatio(); }
        glm::uvec2 GetWindowSize() const { return m_backend.GetSwapchainExtent(); }

        // Font stuff
        std::vector<FontGlyph> GetTextGlyphs(const RenderHandle &font, const std::string &text) const;
        FontGlyph GetGlyph(const RenderHandle &font, const std::string &name) const;

        // Animation stuff
        float GetAnimationDuration(const RenderHandle &animation) const;
        AnimationTiming GetAnimationTiming(const RenderHandle &animation, float time, bool loop = true) const;
        // NOTE: Expensive
        glm::mat4 GetBoneTransform(RenderHandle skeletalMesh, const std::vector<AnimationJob> &animationInstances, uint32_t boneIndex) const;
        uint32_t GetBoneCount(RenderHandle skeletalMesh) const;

        glm::vec3 GetMeshCenterOffset(const RenderHandle &mesh) const;
        const std::vector<glm::vec3> &GetMeshVertices(const RenderHandle &mesh) const;
        const std::vector<uint32_t> &GetMeshIndices(const RenderHandle &mesh) const;

        glm::mat4 GetProjectionMatrix() const
        {
            return m_backend.GetProjectionMatrix();
        }
        glm::mat4 GetViewMatrix() const { return m_backend.GetViewMatrix(); }
        Camera &GetCamera() { return m_backend.GetCamera(); }

        void SetFramebufferResized(bool resized) { m_backend.SetFramebufferResized(resized); }
        void SetCamera(const Camera &camera);
        void SetProjection(const glm::mat4 &projection);
        void SetDeltaTime(float deltaTime) { m_backend.SetDeltaTime(deltaTime); }
        void SetBloomSettings(const BloomSettings &bloomSettings) { m_backend.SetBloomSettings(bloomSettings); }
        void SetDebugCallback(std::function<void(const std::string &)> callback) { m_backend.SetDebugCallback(callback); }

        void WaitAndResetImageTransitionQueue() { m_backend.WaitAndResetImageTransitionQueue(); }

        bool IsResourceValid(const RenderHandle &handle) const { return m_backend.IsResourceValid(handle); }
        RenderHandle GetQuadMesh() const { return m_quadMesh; }
        RenderHandle LoadMesh(const std::string &path, bool keepInMemory = false);
        RenderHandle LoadSkeletalMesh(const std::string &path);
        RenderHandle LoadImage(const std::string &path, ImageType type = ImageType::None, bool useAllMips = false);
        RenderHandle LoadFontAtlas(const std::string &path);
        RenderHandle LoadEnvironmentMap(const std::string &name, const std::string &irradianceFilepath, const std::string &prefilteredFilepath, const VolumeData &irradianceVolumeData, const std::vector<glm::vec3> &probePositions);
        RenderHandle LoadAnimation(const std::string &path);
        template <typename T>
        RenderHandle CreateMaterial(const std::string &name, const T &materialCreateInfo)
        {
            return m_backend.CreateMaterial(name, materialCreateInfo);
        }

        template <>
        RenderHandle CreateMaterial<SpriteMaterial>(const std::string &name, const SpriteMaterial &materialCreateInfo)
        {
            RenderHandle handle = m_backend.CreateMaterial(name, materialCreateInfo);
            m_spriteMaterialToLayer[handle] = materialCreateInfo.layer;
            return handle;
        }

        template <>
        RenderHandle CreateMaterial<PBRMaterial>(const std::string &name, const PBRMaterial &materialCreateInfo)
        {
            RenderHandle handle = m_backend.CreateMaterial(name, materialCreateInfo);
            if (materialCreateInfo.transparent)
            {
                m_transparentMaterials.insert(handle);
            }
            return handle;
        }

        void Destroy(const RenderHandle &handle) { m_backend.Destroy(handle); }

        RenderHandle CreateCaptureCubemap(std::string name, uint32_t width, uint32_t height, uint32_t numCubemaps = 1) { return m_backend.CreateCaptureCubemap(name, width, height, numCubemaps); }
        void CaptureSceneToCubemap(RenderHandle cubemap, uint32_t faceIndex) { m_backend.CaptureSceneToCubemap(cubemap, faceIndex); }

        void SaveScreenshot(const std::string &filepath, bool saveAsCompressed) { m_backend.SaveScreenshot(filepath, saveAsCompressed); }
        void SaveImage(const RenderHandle &image, const std::string &filepath, bool saveAsCompressed) { m_backend.SaveImage(image, filepath, saveAsCompressed); }

        std::vector<SHData> GenerateIrradianceSHs(RenderHandle environmentImageArray) { return m_backend.GenerateIrradianceSHs(environmentImageArray); }
        RenderHandle GenerateIrradianceMap(RenderHandle environmentMap, const std::string &name) { return m_backend.GenerateIrradianceMap(environmentMap, name); }
        const VolumeData &GetIrradianceVolume(RenderHandle environmentMap) const;

        size_t GetImageMemoryUsage() const { return m_backend.GetImageMemoryUsage(); }
        size_t GetBufferMemoryUsage() const { return m_backend.GetBufferMemoryUsage(); }
        size_t GetMemoryUsage() const { return m_backend.GetImageMemoryUsage() + m_backend.GetBufferMemoryUsage(); }
    };

}
