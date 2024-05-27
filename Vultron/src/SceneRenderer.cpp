#include "Vultron/SceneRenderer.h"

#include <algorithm>

namespace Vultron
{

    bool SceneRenderer::Initialize(const Window &window)
    {
        bool success = m_backend.Initialize(window);
        m_quadMesh = m_backend.LoadQuad("quad");
        return success;
    }

    void SceneRenderer::PostInitialize()
    {
        m_backend.PostInitialize();
    }

    void SceneRenderer::BeginFrame()
    {
        m_staticJobs.clear();
        m_skeletalJobs.clear();
        m_animationInstances.clear();
        m_spriteJobs.clear();
        m_fontJobs.clear();
    }

    void SceneRenderer::SubmitRenderJob(const StaticRenderJob &job)
    {
        uint64_t hash = job.GetHash();
        if (m_staticJobs.find(hash) == m_staticJobs.end())
        {
            m_staticJobs.insert({
                hash,
                InstancedStaticRenderJob{
                    .mesh = job.mesh,
                    .material = job.material,
                    .instances = {},
                    .transparent = m_transparentMaterials.find(job.material) != m_transparentMaterials.end(),
                },
            });
        }

        auto &instancedJob = m_staticJobs[hash];
        if (job.castShadows)
        {
            instancedJob.instances.push_back(
                StaticInstanceData{
                    .model = job.transform,
                    .texCoord = job.texCoord,
                    .texSize = job.texSize,
                    .color = job.color,
                });
        }
        else
        {
            // Insert at the beginning of the list to separate shadow casters from non-shadow casters
            instancedJob.instances.emplace(
                instancedJob.instances.begin(),
                StaticInstanceData{
                    .model = job.transform,
                    .texCoord = job.texCoord,
                    .texSize = job.texSize,
                    .color = job.color,
                });
            instancedJob.nonShadowCasterCount++;
        }
    }

    void SceneRenderer::SubmitRenderJob(const SkeletalRenderJob &job)
    {
        uint64_t hash = job.GetHash();
        if (m_skeletalJobs.find(hash) == m_skeletalJobs.end())
        {
            m_skeletalJobs.insert({hash, InstancedSkeletalRenderJob{job.mesh, job.material, {}}});
        }

        auto &instancedJob = m_skeletalJobs[hash];

        // Filter out animations with 0 blend factor
        std::vector<AnimationInstance> animations;
        for (const auto &animation : job.animations)
        {
            if (animation.blendFactor > 0.0f)
            {
                animations.push_back(animation);
            }
        }

        int32_t animationOffset = static_cast<int32_t>(m_animationInstances.size());
        int32_t animationCount = static_cast<int32_t>(animations.size());

        const auto &rp = m_backend.GetResourcePool();
        const auto &mesh = rp.GetSkeletalMesh(job.mesh);

        int32_t boneOutputOffset = instancedJob.instances.size() > 0 ? instancedJob.instances.back().boneOutputOffset + instancedJob.instances.back().boneCount : 0;

        SkeletalInstanceData instance = {
            .model = job.transform,
            // We are settings these per instance for now, but we only need to do it per batch.
            // This will do for now.
            .boneOffset = static_cast<int32_t>(mesh.GetBoneOffset()),
            .boneCount = static_cast<int32_t>(mesh.GetBoneCount()),

            .animationInstanceOffset = animationOffset,
            .animationInstanceCount = animationCount,

            .boneOutputOffset = boneOutputOffset,
        };

        instancedJob.instances.push_back(instance);

        for (const auto &animation : animations)
        {
            const auto &a = rp.GetAnimation(animation.animation);
            const int32_t frameOffset = static_cast<int32_t>(a.GetFrameOffset());
            m_animationInstances.push_back({
                .frameOffset = frameOffset,
                .frame1 = static_cast<int32_t>(animation.frame1),
                .frame2 = static_cast<int32_t>(animation.frame2),
                .timeFactor = animation.frameBlendFactor,
                .blendFactor = animation.blendFactor,
            });
        }
    }

    void SceneRenderer::SubmitRenderJob(const SpriteRenderJob &job)
    {
        uint64_t hash = job.GetHash();

        if (m_spriteJobs.find(hash) == m_spriteJobs.end())
        {
            m_spriteJobs.insert({hash, InstancedSpriteRenderJob{job.material, {}}});
        }

        m_spriteJobs[hash].instances.push_back(SpriteInstanceData{
            .position = job.position,
            .size = job.size,
            .texCoord = job.texCoord,
            .texSize = job.texSize,
            .color = job.color,
        });
    }

    void SceneRenderer::SubmitRenderJob(const FontRenderJob &job)
    {
        uint64_t hash = job.GetHash();

        if (m_fontJobs.find(hash) == m_fontJobs.end())
        {
            m_fontJobs.insert({hash, InstancedSpriteRenderJob{job.material, {}}});
        }

        m_fontJobs[hash].instances.push_back(SpriteInstanceData{
            .position = job.position,
            .size = job.size,
            .texCoord = job.texCoord,
            .texSize = job.texSize,
            .color = job.color,
        });
    }

    void SceneRenderer::SubmitRenderJob(const ParticleEmitJob &job)
    {
        m_particleEmitters.push_back({
            .position = job.position,
            .lifetime = job.lifetime,
            .lifeDuration = job.lifetime,
            .numFrames = float(job.numFrames),
            .framesPerSecond = job.framesPerSecond,
            .initialVelocity = job.initialVelocity,
            .gravityFactor = job.gravityFactor,
            .size = job.size,
            .sizeSpan = job.sizeSpan,
            .phiSpan = job.phiSpan,
            .thetaSpan = job.thetaSpan,
            .texCoord = job.texCoord,
            .texSize = job.texSize,
            .color = job.color,
            .numParticles = float(job.numParticles),
            .velocitySpan = job.velocitySpan,
            .texCoordSpan = job.texCoordSpan,
            .scaleIn = job.scaleIn,
            .scaleOut = job.scaleOut,
            .opacityIn = job.opacityIn,
            .opacityOut = job.opacityOut,
        });
    }

    void SceneRenderer::EndFrame()
    {
        std::vector<StaticInstanceData> staticInstances;
        std::vector<RenderBatch> staticBatches;
        std::vector<InstancedStaticRenderJob> staticJobs;

        staticJobs.reserve(m_staticJobs.size());
        for (auto &job : m_staticJobs)
        {
            staticJobs.push_back(job.second);
        }

        // Maybe look over this sorting logic
        std::sort(staticJobs.begin(), staticJobs.end(), [](const InstancedStaticRenderJob &a, const InstancedStaticRenderJob &b)
                  { return a.transparent < b.transparent; });

        for (auto &job : staticJobs)
        {
            staticBatches.push_back({
                .mesh = job.mesh,
                .material = job.material,
                .firstInstance = static_cast<uint32_t>(staticInstances.size()),
                .instanceCount = static_cast<uint32_t>(job.instances.size()),
                .nonShadowCasterCount = job.nonShadowCasterCount,
            });

            // This is a hack that works for a top-down game.
            std::sort(job.instances.begin(), job.instances.end(), [](const StaticInstanceData &a, const StaticInstanceData &b)
                      { return a.model[3].y < b.model[3].y; });

            staticInstances.insert(staticInstances.end(), job.instances.begin(), job.instances.end());
        }

        std::vector<SkeletalInstanceData> skeletalInstances;
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
        }

        std::vector<SpriteInstanceData> spriteInstances;
        std::vector<RenderBatch> spriteBatches;
        std::vector<RenderBatch> sdfBatches;
        std::vector<InstancedSpriteRenderJob> spriteJobs;
        std::vector<InstancedSpriteRenderJob> fontJobs;

        for (auto &job : m_spriteJobs)
        {
            spriteJobs.push_back(job.second);
        }

        for (auto &job : m_fontJobs)
        {
            fontJobs.push_back(job.second);
        }

        std::sort(
            spriteJobs.begin(), spriteJobs.end(),
            [this](const InstancedSpriteRenderJob &a, const InstancedSpriteRenderJob &b)
            {
                return m_spriteMaterialToLayer[a.material] < m_spriteMaterialToLayer[b.material];
            });

        for (auto &job : spriteJobs)
        {
            spriteBatches.push_back({
                .mesh = {},
                .material = job.material,
                .firstInstance = static_cast<uint32_t>(spriteInstances.size()),
                .instanceCount = static_cast<uint32_t>(job.instances.size()),
            });
            spriteInstances.insert(spriteInstances.end(), job.instances.begin(), job.instances.end());
        }

        for (auto &job : fontJobs)
        {
            sdfBatches.push_back({
                .mesh = m_quadMesh,
                .material = job.material,
                .firstInstance = static_cast<uint32_t>(spriteInstances.size()),
                .instanceCount = static_cast<uint32_t>(job.instances.size()),
            });
            spriteInstances.insert(spriteInstances.end(), job.instances.begin(), job.instances.end());
        }

        m_backend.Draw(RenderData{
            .staticBatches = staticBatches,
            .staticInstances = staticInstances,
            .skeletalBatches = skeletalBatches,
            .skeletalInstances = skeletalInstances,
            .animationInstances = m_animationInstances,
            .spriteBatches = spriteBatches,
            .sdfBatches = sdfBatches,
            .spriteInstances = spriteInstances,
            .particleEmitters = m_particleEmitters,
        });

        m_particleEmitters.clear();
        m_backend.SetDeltaTime(0.0f);
    }

    std::vector<FontGlyph> SceneRenderer::GetTextGlyphs(const RenderHandle &font, const std::string &text) const
    {
        static FontGlyph spaceGlyph = {.character = " ", .uvOffset = {1.0f, 1.0f}, .uvExtent = {0.03f, 0.0f}, .aspectRatio = 1.0f};

        const auto &rp = m_backend.GetResourcePool();
        const auto &fontAtlas = rp.GetFontAtlas(font);

        std::vector<FontGlyph> glyphs;
        for (char c : text)
        {
            if (c == ' ')
            {
                glyphs.push_back(spaceGlyph);
                continue;
            }

            glyphs.push_back(fontAtlas.GetGlyph(std::string(1, c)));
        }

        return glyphs;
    }

    FontGlyph SceneRenderer::GetGlyph(const RenderHandle &font, const std::string &name) const
    {
        const auto &rp = m_backend.GetResourcePool();
        const auto &fontAtlas = rp.GetFontAtlas(font);
        return fontAtlas.GetGlyph(name);
    }

    SceneRenderer::AnimationTiming SceneRenderer::GetAnimationTiming(const RenderHandle &animation, float time, bool loop) const
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

        return {frame1, frame2, frameBlendFactor, newTime, anim.GetDuration()};
    }

    // NOTE: Expensive
    glm::mat4 SceneRenderer::GetBoneTransform(RenderHandle skeletalMesh, const std::vector<AnimationInstance> &animationInstances, uint32_t boneIndex) const
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

                if (blendFactor <= 0.0f)
                {
                    continue;
                }

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

    void SceneRenderer::Shutdown()
    {
        m_backend.Shutdown();
    }

    void SceneRenderer::SetCamera(const Camera &camera)
    {
        m_backend.SetCamera(camera);
    }

    void SceneRenderer::SetProjection(const glm::mat4 &projection)
    {
        m_backend.SetProjection(projection);
    }

    RenderHandle SceneRenderer::LoadMesh(const std::string &path)
    {
        return m_backend.LoadMesh(path);
    }

    RenderHandle SceneRenderer::LoadSkeletalMesh(const std::string &path)
    {
        return m_backend.LoadSkeletalMesh(path);
    }

    RenderHandle SceneRenderer::LoadImage(const std::string &path)
    {
        return m_backend.LoadImage(path);
    }

    RenderHandle SceneRenderer::LoadFontAtlas(const std::string &path)
    {
        return m_backend.LoadFontAtlas(path);
    }

    RenderHandle SceneRenderer::LoadAnimation(const std::string &path)
    {
        return m_backend.LoadAnimation(path);
    }
}