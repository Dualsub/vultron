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
        std::vector<AnimationInstanceData> m_animationInstances;

        std::unordered_map<RenderHandle, int32_t> m_spriteMaterialToLayer;
        std::set<RenderHandle> m_transparentMaterials;
        RenderHandle m_quadMesh = {};

    public:
        SceneRenderer() = default;
        ~SceneRenderer() = default;

        bool Initialize(const Window &window);
        void PostInitialize();
        void BeginFrame();
        void SubmitRenderJob(const StaticRenderJob &job);
        void SubmitRenderJob(const SkeletalRenderJob &job);
        void SubmitRenderJob(const SpriteRenderJob &job);
        void SubmitRenderJob(const FontRenderJob &job);
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

        // Font stuff
        std::vector<FontGlyph> GetTextGlyphs(const RenderHandle &font, const std::string &text) const;
        FontGlyph GetGlyph(const RenderHandle &font, const std::string &name) const;

        // Animation stuff
        AnimationTiming GetAnimationTiming(const RenderHandle &animation, float time, bool loop = true) const;
        // --NOTE: Expensive
        glm::mat4 GetBoneTransform(RenderHandle skeletalMesh, const std::vector<AnimationInstance> &animationInstances, uint32_t boneIndex) const;

        glm::mat4 GetProjectionMatrix() const { return m_backend.GetProjectionMatrix(); }
        glm::mat4 GetViewMatrix() const { return m_backend.GetViewMatrix(); }

        void SetCamera(const Camera &camera);
        void SetProjection(const glm::mat4 &projection);

        RenderHandle GetQuadMesh() const { return m_quadMesh; }
        RenderHandle LoadMesh(const std::string &path);
        RenderHandle LoadSkeletalMesh(const std::string &path);
        RenderHandle LoadImage(const std::string &path);
        RenderHandle LoadFontAtlas(const std::string &path);
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
    };

}
