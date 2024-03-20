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
        std::vector<struct AnimationInstanceData> animations = {};
    };

    /* Idea: Frontend renderer idea: Take in render jobs */
    class SceneRenderer
    {
    private:
        VulkanRenderer m_backend;
        std::map<uint64_t, InstancedStaticRenderJob> m_staticJobs;
        std::map<uint64_t, InstancedSkeletalRenderJob> m_skeletalJobs;

    public:
        SceneRenderer() = default;
        ~SceneRenderer() = default;

        bool Initialize(const Window &window)
        {
            return m_backend.Initialize(window);
        }

        void PostInitialize()
        {
            m_backend.PostInitialize();
        }

        void BeginFrame()
        {
            m_staticJobs.clear();
            m_skeletalJobs.clear();
        }

        void SubmitRenderJob(const StaticRenderJob &job)
        {
            uint64_t hash = job.GetHash();
            if (m_staticJobs.find(hash) == m_staticJobs.end())
            {
                m_staticJobs.insert({hash, InstancedStaticRenderJob{job.mesh, job.material, {}}});
            }

            m_staticJobs[hash].transforms.push_back(job.transform);
        }

        void SubmitRenderJob(const SkeletalRenderJob &job)
        {
            uint64_t hash = job.GetHash();
            if (m_skeletalJobs.find(hash) == m_skeletalJobs.end())
            {
                m_skeletalJobs.insert({hash, InstancedSkeletalRenderJob{job.mesh, job.material, {}, {}}});
            }

            auto &instancedJob = m_skeletalJobs[hash];

            int32_t animationOffset = static_cast<int32_t>(instancedJob.animations.size());
            int32_t animationCount = static_cast<int32_t>(job.animations.size());

            const auto &rp = m_backend.GetResourcePool();
            const auto &mesh = rp.GetSkeletalMesh(job.mesh);

            SkeletalInstanceData instance = {
                .model = job.transform,
                // We are settings these per instance for now, but we only need to do it per batch.
                // This will do for now.
                .boneOffset = static_cast<int32_t>(mesh.GetBoneOffset()),
                .boneCount = static_cast<int32_t>(mesh.GetBoneCount()),

                .animationInstanceOffset = animationOffset,
                .animationInstanceCount = animationCount,
            };

            instancedJob.instances.push_back(instance);

            for (const auto &animation : job.animations)
            {
                const auto &a = rp.GetAnimation(animation.animation);
                const int32_t frameOffset = static_cast<int32_t>(a.GetFrameOffset());
                instancedJob.animations.push_back({
                    .frameOffset = frameOffset,
                    .frame1 = static_cast<int32_t>(animation.frame1),
                    .frame2 = static_cast<int32_t>(animation.frame2),
                    .timeFactor = animation.frameBlendFactor,
                    .blendFactor = animation.blendFactor,
                });
            }
        }

        void EndFrame()
        {
            std::vector<glm::mat4> staticInstances;
            std::vector<RenderBatch> staticBatches;

            for (auto &job : m_staticJobs)
            {
                staticBatches.push_back({
                    .mesh = job.second.mesh,
                    .material = job.second.material,
                    .firstInstance = static_cast<uint32_t>(staticInstances.size()),
                    .instanceCount = static_cast<uint32_t>(job.second.transforms.size()),
                });
                staticInstances.insert(staticInstances.end(), job.second.transforms.begin(), job.second.transforms.end());
            }

            std::vector<SkeletalInstanceData> skeletalInstances;
            std::vector<AnimationInstanceData> animationInstances;
            std::vector<RenderBatch> skeletalBatches;

            for (auto &job : m_skeletalJobs)
            {
                skeletalBatches.push_back({
                    .mesh = job.second.mesh,
                    .material = job.second.material,
                    .firstInstance = static_cast<uint32_t>(skeletalInstances.size()),
                    .instanceCount = static_cast<uint32_t>(job.second.instances.size()),
                });
                skeletalInstances.insert(skeletalInstances.end(), job.second.instances.begin(), job.second.instances.end());
                animationInstances.insert(animationInstances.end(), job.second.animations.begin(), job.second.animations.end());
            }

            m_backend.Draw(staticBatches, staticInstances, skeletalBatches, skeletalInstances, animationInstances);
        }

        // This should be
        struct AnimationTiming
        {
            uint32_t frame1 = 0;
            uint32_t frame2 = 0;
            float frameBlendFactor = 0.0f;
            float time = 0.0f;
        };

        // TODO: This should be somewhere else
        AnimationTiming GetAnimationTiming(const RenderHandle &animation, float time, bool loop = true) const
        {
            const auto &rp = m_backend.GetResourcePool();
            const auto &anim = rp.GetAnimation(animation);

            const float newTime = loop ? glm::mod(time, anim.GetDuration()) : glm::min(time, anim.GetDuration());

            const std::vector<float> &times = anim.GetTimes();

            uint32_t numFrames = static_cast<uint32_t>(times.size());
            uint32_t frame1 = 0;
            float frame1Time = 0.0f;
            for (uint32_t i = 0; i < numFrames; i++)
            {
                if (time < times[i] || i == numFrames - 1)
                {
                    frame1 = i == 0 ? numFrames - 1 : i - 1;
                    frame1Time = times[frame1];
                    break;
                }
            }

            uint32_t frame2 = frame1 + 1;
            if (loop)
            {
                frame2 = frame2 % numFrames;
            }
            else
            {
                frame2 = glm::min(frame2, numFrames - 1);
            }
            float frame2Time = times[frame2];

            if (frame2Time < frame1Time)
            {
                frame2Time += anim.GetDuration();
            }

            float frameBlendFactor = glm::clamp((newTime - frame1Time) / (frame2Time - frame1Time), 0.0f, 1.0f);

            return {frame1, frame2, frameBlendFactor, newTime};
        }

        // NOTE: Expensive
        glm::mat4 GetBoneTransform(RenderHandle skeletalMesh, const std::vector<AnimationInstance> &animationInstances, uint32_t boneIndex)
        {
            const auto &rp = m_backend.GetResourcePool();
            const auto &mesh = rp.GetSkeletalMesh(skeletalMesh);

            const auto &bones = m_backend.GetBones();
            const auto &frames = m_backend.GetAnimationFrames();

            glm::mat4 boneTransform = glm::mat4(1.0f);

            uint32_t currBoneIndex = boneIndex;
            for (uint32_t b = 0; b < mesh.GetBoneCount(); b++)
            {
                float totalBlendFactor = 0.0f;
                glm::vec3 accPosition = glm::vec3(0.0f);
                glm::quat accRotation = glm::identity<glm::quat>();
                glm::vec3 accScale = glm::vec3(1.0f);

                for (uint32_t i = 0; i < animationInstances.size(); i++)
                {
                    const auto &instance = animationInstances[i];
                    const auto &anim = rp.GetAnimation(instance.animation);
                    const uint32_t frameOffset = anim.GetFrameOffset();
                    const uint32_t frame1 = instance.frame1;
                    const uint32_t frame2 = instance.frame2;
                    const float frameBlendFactor = instance.frameBlendFactor;
                    const float blendFactor = instance.blendFactor;

                    uint32_t frame1Index = frameOffset + frame1 * mesh.GetBoneCount() + currBoneIndex;
                    uint32_t frame2Index = frameOffset + frame2 * mesh.GetBoneCount() + currBoneIndex;

                    glm::vec3 position = glm::mix(frames[frame1Index].position, frames[frame2Index].position, frameBlendFactor);
                    glm::quat rotation = glm::slerp(frames[frame1Index].rotation, frames[frame2Index].rotation, frameBlendFactor);
                    glm::vec3 scale = glm::mix(frames[frame1Index].scale, frames[frame2Index].scale, frameBlendFactor);

                    totalBlendFactor += blendFactor;
                    float blend = blendFactor / totalBlendFactor;

                    accPosition = mix(accPosition, position, blend);
                    accRotation = glm::slerp(accRotation, rotation, blend);
                    accScale = mix(accScale, scale, blend);
                }

                glm::mat4 localBoneTransform = glm::translate(glm::mat4(1.0f), accPosition) * glm::mat4_cast(accRotation) * glm::scale(glm::mat4(1.0f), accScale);
                boneTransform = localBoneTransform * boneTransform;

                currBoneIndex = bones[currBoneIndex].parentID;

                if (currBoneIndex == -1)
                {
                    break;
                }
            }

            return boneTransform;
        }

        void Shutdown()
        {
            m_backend.Shutdown();
        }

        void SetCamera(const Camera &camera)
        {
            m_backend.SetCamera(camera);
        }

        void SetProjection(const glm::mat4 &projection)
        {
            m_backend.SetProjection(projection);
        }

        RenderHandle LoadMesh(const std::string &path)
        {
            return m_backend.LoadMesh(path);
        }

        RenderHandle LoadSkeletalMesh(const std::string &path)
        {
            return m_backend.LoadSkeletalMesh(path);
        }

        RenderHandle LoadImage(const std::string &path)
        {
            return m_backend.LoadImage(path);
        }

        RenderHandle LoadAnimation(const std::string &path)
        {
            return m_backend.LoadAnimation(path);
        }

        template <typename T>
        RenderHandle CreateMaterial(const T &materialCreateInfo)
        {
            return m_backend.CreateMaterial(materialCreateInfo);
        }
    };

}
