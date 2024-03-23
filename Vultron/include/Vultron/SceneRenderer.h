#pragma once

#include "Vultron/Types.h"
#include "Vultron/Window.h"
#include "Vultron/Vulkan/VulkanRenderer.h"

#include <glm/glm.hpp>

#include <map>
#include <vector>
#include <iostream>

namespace Vultron
{
    // What is sent to the backend renderer
    struct InstancedStaticRenderJob
    {
        RenderHandle mesh = {};
        RenderHandle material = {};
        std::vector<glm::mat4> transforms = {};
    };

    struct InstancedSkeletalRenderJob
    {
        RenderHandle mesh = {};
        RenderHandle material = {};
        std::vector<struct SkeletalInstanceData> instances = {};
    };

    /* Idea: Frontend renderer idea: Take in render jobs */
    class SceneRenderer
    {
    private:
        VulkanRenderer m_backend;
        std::map<uint64_t, InstancedStaticRenderJob> m_staticJobs;
        std::map<uint64_t, InstancedSkeletalRenderJob> m_skeletalJobs;
        std::vector<AnimationInstanceData> m_animationInstances;

    public:
        SceneRenderer() = default;
        ~SceneRenderer() = default;

        bool Initialize(const Window &window);
        void PostInitialize();
        void BeginFrame();
        void SubmitRenderJob(const StaticRenderJob &job);
        void SubmitRenderJob(const SkeletalRenderJob &job);
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

        AnimationTiming GetAnimationTiming(const RenderHandle &animation, float time, bool loop = true) const;
        // NOTE: Expensive
        glm::mat4 GetBoneTransform(RenderHandle skeletalMesh, const std::vector<AnimationInstance> &animationInstances, uint32_t boneIndex);

        void SetCamera(const Camera &camera);
        void SetProjection(const glm::mat4 &projection);

        RenderHandle LoadMesh(const std::string &path);
        RenderHandle LoadSkeletalMesh(const std::string &path);
        RenderHandle LoadImage(const std::string &path);
        RenderHandle LoadAnimation(const std::string &path);
        template <typename T>
        RenderHandle CreateMaterial(const T &materialCreateInfo)
        {
            return m_backend.CreateMaterial(materialCreateInfo);
        }
    };

}
