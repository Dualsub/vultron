#include "Vultron/SceneRenderer.h"

#include <algorithm>

namespace Vultron
{
    void SceneRenderer::GenerateRibbonVertices(const std::vector<RibbonControlPoint> &points, const glm::vec2 &uvStart, const glm::vec2 &uvEnd, std::vector<RibbonVertex> &vertices, std::vector<uint32_t> &indices)
    {
        if (points.size() < 2)
        {
            return;
        }

        const uint32_t numSegments = static_cast<uint32_t>(points.size()) - 1;
        const uint32_t numVertices = (numSegments + 1) * 2; // Two vertices per control point
        const uint32_t numIndices = numSegments * 6;        // Two triangles per segment

        const uint32_t vertexOffset = static_cast<uint32_t>(vertices.size());
        const uint32_t indexOffset = static_cast<uint32_t>(indices.size());

        vertices.resize(vertexOffset + numVertices);
        indices.resize(indexOffset + numIndices);

        for (uint32_t i = 0; i <= numSegments; i++)
        {
            const RibbonControlPoint &p0 = points[i];

            glm::vec3 dir = glm::vec3(0.0f);
            if (i < numSegments)
            {
                dir = glm::normalize(points[i + 1].position - p0.position);
            }
            else
            {
                dir = glm::normalize(p0.position - points[i - 1].position);
            }

            const glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
            const glm::vec3 normal = glm::normalize(glm::cross(dir, up));

            const glm::vec3 left = p0.position - normal * p0.width;
            const glm::vec3 right = p0.position + normal * p0.width;

            const float t = static_cast<float>(i) / static_cast<float>(numSegments);
            float uvX = glm::mix(uvStart.x, uvEnd.x, t);
            const glm::vec2 uv0 = glm::vec2(uvX, uvStart.y);
            const glm::vec2 uv1 = glm::vec2(uvX, uvEnd.y);

            // Add two vertices for the current control point
            vertices[vertexOffset + i * 2 + 0] = RibbonVertex{.position = left, .normal = up, .texCoord = glm::vec3(uv0, 0.0f), .color = p0.color};
            vertices[vertexOffset + i * 2 + 1] = RibbonVertex{.position = right, .normal = up, .texCoord = glm::vec3(uv1, 0.0f), .color = p0.color};
        }

        for (uint32_t i = 0; i < numSegments; i++)
        {
            // Triangle 1
            indices[indexOffset + i * 6 + 0] = vertexOffset + i * 2 + 0;
            indices[indexOffset + i * 6 + 1] = vertexOffset + i * 2 + 1;
            indices[indexOffset + i * 6 + 2] = vertexOffset + (i + 1) * 2 + 0;

            // Triangle 2
            indices[indexOffset + i * 6 + 3] = vertexOffset + (i + 1) * 2 + 0;
            indices[indexOffset + i * 6 + 4] = vertexOffset + i * 2 + 1;
            indices[indexOffset + i * 6 + 5] = vertexOffset + (i + 1) * 2 + 1;
        }
    }

    bool SceneRenderer::Initialize(const Window &window)
    {
        bool success = m_backend.Initialize(window);
        m_quadMesh = m_backend.LoadQuad("quad");
        m_backend.LoadCube("cube");
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
        m_decalInstances.clear();
        m_ribbonJobs.clear();
        m_lines.clear();
        m_spriteJobs.clear();
        m_fontJobs.clear();
        m_boneOutputOffset = 0;
        m_boneInputTransforms.clear();
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
                    .emissiveColor = job.emissiveColor,
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
                    .emissiveColor = job.emissiveColor,
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
        std::vector<AnimationJob> animations;
        for (const auto &animation : job.animations)
        {
            if (animation.blendFactor != 0.0f)
            {
                animations.push_back(animation);
            }
        }

        int32_t animationOffset = static_cast<int32_t>(m_animationInstances.size());
        int32_t animationCount = static_cast<int32_t>(animations.size());

        const auto &rp = m_backend.GetResourcePool();
        const auto &mesh = rp.GetSkeletalMesh(job.mesh);

        int32_t boneOutputOffset = m_boneOutputOffset;
        m_boneOutputOffset += mesh.GetBoneCount();

        int32_t boneInputOffset = job.inputBones.empty() ? -1 : static_cast<int32_t>(m_boneInputTransforms.size());
        if (boneInputOffset != -1)
        {
            m_boneInputTransforms.insert(m_boneInputTransforms.end(), job.inputBones.begin(), job.inputBones.end());
        }

        SkeletalInstanceData instance = {
            .model = job.transform,
            // We are settings these per instance for now, but we only need to do it per batch.
            // This will do for now.
            .boneOffset = static_cast<int32_t>(mesh.GetBoneOffset()),
            .boneCount = static_cast<int32_t>(mesh.GetBoneCount()),

            .animationInstanceOffset = animationOffset,
            .animationInstanceCount = animationCount,

            .boneOutputOffset = boneOutputOffset,
            .boneInputOffset = boneInputOffset,
            .boneInputInterval = {
                job.animations.empty() ? 0 : job.inputBoneIndex,
                job.animations.empty() ? 0 : (job.inputBoneIndex + static_cast<int32_t>(job.inputBones.size()) - 1),
            },
            .color = job.color,
            .emissiveColor = job.emissiveColor,
        };

        instancedJob.instances.push_back(instance);

        for (const auto &animation : animations)
        {
            const auto &a = rp.GetAnimation(animation.animation);
            const int32_t frameOffset = static_cast<int32_t>(a.GetFrameOffset());
            int32_t referenceFrame = -1;

            if (animation.referenceAnimation != VLT_INVALID_HANDLE)
            {
                const auto &referenceAnim = rp.GetAnimation(animation.referenceAnimation);
                referenceFrame = static_cast<int32_t>(referenceAnim.GetFrameOffset()) + animation.referenceFrame;
            }

            m_animationInstances.push_back({
                .frameOffset = frameOffset,
                .frame1 = static_cast<int32_t>(animation.frame1),
                .frame2 = static_cast<int32_t>(animation.frame2),
                .referenceFrame = referenceFrame,
                .timeFactor = animation.frameBlendFactor,
                .blendFactor = animation.blendFactor,
                .boneInterval = {animation.boneIntervalStart, animation.boneIntervalEnd},
            });
        }
    }

    void SceneRenderer::SubmitRenderJob(const DecalRenderJob &job)
    {
        m_decalInstances.push_back(DecalInstanceData{
            .model = job.transform,
            .inverseModel = glm::inverse(job.transform),
            .texCoord = job.texCoord,
            .texSize = job.texSize,
            .color = job.color,
        });
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
            .borderRadius = job.borderRadius,
            .rotation = job.rotation,
            .zOrder = job.zOrder,
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
            .borderRadius = glm::vec4(0.0f),
            .rotation = job.rotation,
            .zOrder = job.zOrder,
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
            .acceleration = job.acceleration + glm::vec3(0.0f, -job.gravityFactor * 981.0f, 0.0f),
            .size = job.size,
            .sizeSpan = job.sizeSpan,
            .phiSpan = job.phiSpan,
            .thetaSpan = job.thetaSpan,
            .rotation = job.rotation,
            .texCoord = job.texCoord,
            .texSize = job.texSize,
            .startColor = job.startColor,
            .endColor = job.endColor.value_or(job.startColor),
            .numParticles = float(job.numParticles),
            .velocitySpan = job.velocitySpan,
            .texCoordSpan = job.texCoordSpan,
            .scaleIn = job.scaleIn,
            .scaleOut = job.scaleOut,
            .opacityIn = job.opacityIn,
            .opacityOut = job.opacityOut,
        });
    }

    void SceneRenderer::SubmitRenderJob(const RibbonRenderJob &job)
    {
        if (job.vertices.empty() || job.indices.empty())
        {
            return;
        }

        uint64_t hash = job.GetHash();
        auto it = m_ribbonJobs.find(hash);
        if (it == m_ribbonJobs.end())
        {
            m_ribbonJobs.insert({hash, InstancedRibbonRenderJob{.mesh = m_quadMesh, .material = job.material, .vertices = {}, .indices = {}}});
            it = m_ribbonJobs.find(hash);
        }

        auto &instancedJob = it->second;
        uint32_t vertexOffset = static_cast<uint32_t>(instancedJob.vertices.size());
        instancedJob.vertices.insert(instancedJob.vertices.end(), job.vertices.begin(), job.vertices.end());
        for (const auto &index : job.indices)
        {
            instancedJob.indices.push_back(index + vertexOffset);
        }
    }

    void SceneRenderer::SubmitRenderJob(const LineRenderJob &job)
    {
        m_lines.push_back({
            .start = job.start,
            .startColor = job.color,
            .end = job.end,
            .endColor = job.color,
        });
    }

    void SceneRenderer::EndFrame()
    {
        std::vector<StaticInstanceData> staticInstances;
        std::vector<RenderBatch> staticBatches;
        std::vector<RenderBatch> staticTransparentBatches;
        std::vector<InstancedStaticRenderJob> staticJobs;
        std::vector<InstancedStaticRenderJob> staticTransparentJobs;

        staticJobs.reserve(m_staticJobs.size());
        for (auto &job : m_staticJobs)
        {
            if (job.second.transparent)
            {
                staticTransparentJobs.push_back(job.second);
            }
            else
            {
                staticJobs.push_back(job.second);
            }
        }

        for (auto &job : staticJobs)
        {
            staticBatches.push_back({
                .mesh = job.mesh,
                .material = job.material,
                .firstInstance = static_cast<uint32_t>(staticInstances.size()),
                .instanceCount = static_cast<uint32_t>(job.instances.size()),
                .nonShadowCasterCount = job.nonShadowCasterCount,
            });

            staticInstances.insert(staticInstances.end(), job.instances.begin(), job.instances.end());
        }

        for (auto &job : staticTransparentJobs)
        {
            staticTransparentBatches.push_back({
                .mesh = job.mesh,
                .material = job.material,
                .firstInstance = static_cast<uint32_t>(staticInstances.size()),
                .instanceCount = static_cast<uint32_t>(job.instances.size()),
                .nonShadowCasterCount = job.nonShadowCasterCount,
            });

            glm::vec3 camPos = m_backend.GetCamera().position;
            // Sort by distance from camera for proper transparency rendering
            std::sort(job.instances.begin(), job.instances.end(), [camPos](const StaticInstanceData &a, const StaticInstanceData &b)
                      {
                          float distA = glm::length(camPos - glm::vec3(a.model[3]));
                          float distB = glm::length(camPos - glm::vec3(b.model[3]));
                          return distA > distB; // Farther objects first
                      });

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

        std::vector<RenderBatch> ribbonBatches;
        std::vector<RibbonVertex> ribbonVertices;
        std::vector<uint32_t> ribbonIndices;

        for (auto &job : m_ribbonJobs)
        {
            uint32_t firstVertex = static_cast<uint32_t>(ribbonVertices.size());
            uint32_t vertexCount = static_cast<uint32_t>(job.second.vertices.size());
            uint32_t firstIndex = static_cast<uint32_t>(ribbonIndices.size());
            uint32_t indexCount = static_cast<uint32_t>(job.second.indices.size());

            if (vertexCount == 0 || indexCount == 0)
            {
                continue;
            }

            ribbonBatches.push_back(RenderBatch{
                .mesh = job.second.mesh,
                .material = job.second.material,
                .firstIndex = firstIndex,
                .indexCount = indexCount,
            });

            ribbonVertices.insert(ribbonVertices.end(), job.second.vertices.begin(), job.second.vertices.end());

            // Adjust indices to account for the new vertex offset
            for (const auto &index : job.second.indices)
            {
                ribbonIndices.push_back(index + firstVertex);
            }
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

            std::sort(job.instances.begin(), job.instances.end(), [](const SpriteInstanceData &a, const SpriteInstanceData &b)
                      { return a.zOrder < b.zOrder; });

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
            .transparentStaticBatches = staticTransparentBatches,
            .staticInstances = staticInstances,
            .skeletalBatches = skeletalBatches,
            .skeletalInstances = skeletalInstances,
            .animationInstances = m_animationInstances,
            .boneInputTransforms = m_boneInputTransforms,
            .decalInstances = m_decalInstances,
            .spriteBatches = spriteBatches,
            .sdfBatches = sdfBatches,
            .spriteInstances = spriteInstances,
            .particleEmitters = m_particleEmitters,
            .skybox = m_skybox,
            .environmentMap = m_environmentMap,
            .particleAtlasMaterial = m_particleAtlasMaterial,
            .decalAtlasMaterial = m_decalAtlasMaterial,
            .pointLights = m_pointLights,
            .lines = m_lines,
            .ribbonBatches = ribbonBatches,
            .ribbonVertices = ribbonVertices,
            .ribbonIndices = ribbonIndices,
        });

        m_particleEmitters.clear();
        m_backend.SetDeltaTime(0.0f);
        InvalidateBoneCache();
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

    float SceneRenderer::GetAnimationDuration(const RenderHandle &animation) const
    {
        const auto &rp = m_backend.GetResourcePool();
        const auto &anim = rp.GetAnimation(animation);
        return anim.GetDuration();
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
            if (newTime < times[i] || i == numFrames - 1)
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

    size_t GetAnimationInstanceHash(RenderHandle skeletalMesh, const std::vector<AnimationJob> &animationInstances)
    {
        size_t hash = std::hash<RenderHandle>()(skeletalMesh);
        for (const auto &instance : animationInstances)
        {
            hash ^= std::hash<RenderHandle>()(instance.animation);
            hash ^= std::hash<uint32_t>()(instance.frame1);
            hash ^= std::hash<uint32_t>()(instance.frame2);
            hash ^= std::hash<int32_t>()(instance.referenceFrame);
            hash ^= std::hash<RenderHandle>()(instance.referenceAnimation);
            hash ^= std::hash<float>()(instance.frameBlendFactor);
            hash ^= std::hash<float>()(instance.blendFactor);
            hash ^= std::hash<int32_t>()(instance.boneIntervalStart);
            hash ^= std::hash<int32_t>()(instance.boneIntervalEnd);
        }
        return hash;
    }

    size_t GetBoneTransformHash(size_t animationInstanceHash, uint32_t boneIndex)
    {
        size_t hash = animationInstanceHash;
        hash ^= std::hash<uint32_t>()(boneIndex);
        return hash;
    }

    size_t GetBoneTransformHash(RenderHandle skeletalMesh, const std::vector<AnimationJob> &animationInstances, uint32_t boneIndex)
    {
        size_t animationInstanceHash = GetAnimationInstanceHash(skeletalMesh, animationInstances);
        return GetBoneTransformHash(animationInstanceHash, boneIndex);
    }

    // NOTE: Expensive
    glm::mat4 SceneRenderer::GetBoneTransform(RenderHandle skeletalMesh, const std::vector<AnimationJob> &animationInstances, uint32_t boneIndex) const
    {
        size_t animationInstanceHash = GetAnimationInstanceHash(skeletalMesh, animationInstances);

        const auto &rp = m_backend.GetResourcePool();
        const auto &mesh = rp.GetSkeletalMesh(skeletalMesh);

        const auto &bones = m_backend.GetBones();
        const auto &frames = m_backend.GetAnimationFrames();

        glm::mat4 boneTransform = glm::mat4(1.0f);

        uint32_t currBoneIndex = boneIndex;
        for (uint32_t b = 0; b < mesh.GetBoneCount(); b++)
        {
            size_t boneHash = GetBoneTransformHash(animationInstanceHash, currBoneIndex);

            {
                std::lock_guard<std::mutex> lock(m_boneTransformCacheMutex);
                auto boneIt = m_boneTransformsCache.find(boneHash);
                if (boneIt != m_boneTransformsCache.end())
                {
                    // boneTransform = boneIt->second * boneTransform;
                    // m_numTimesCachedBoneTransform++;
                    // break;
                }
            }

            float totalBlendFactor = 0.0f;
            glm::vec3 accPosition = glm::vec3(0.0f);
            glm::quat accRotation = glm::quat(0.0f, 0.0f, 0.0f, 0.0f);
            glm::vec3 accScale = glm::vec3(0.0f);

            for (uint32_t i = 0; i < animationInstances.size(); i++)
            {
                const auto &instance = animationInstances[i];
                const auto &anim = rp.GetAnimation(instance.animation);
                const uint32_t frameOffset = anim.GetFrameOffset();
                const uint32_t frame1 = instance.frame1;
                const uint32_t frame2 = instance.frame2;
                const int32_t referenceFrame = instance.referenceFrame;
                const float frameBlendFactor = instance.frameBlendFactor;
                const float blendFactor = instance.blendFactor;
                const int32_t boneIntervalStart = instance.boneIntervalStart;
                const int32_t boneIntervalEnd = instance.boneIntervalEnd;

                if ((boneIntervalStart <= boneIntervalEnd && (currBoneIndex < boneIntervalStart || currBoneIndex > boneIntervalEnd)) ||
                    (boneIntervalStart > boneIntervalEnd && (currBoneIndex >= boneIntervalEnd && currBoneIndex <= boneIntervalStart)))
                {
                    continue;
                }

                if (blendFactor <= 0.0f || referenceFrame != -1)
                {
                    continue;
                }

                uint32_t frame1Index = frameOffset + frame1 * mesh.GetBoneCount() + currBoneIndex;
                uint32_t frame2Index = frameOffset + frame2 * mesh.GetBoneCount() + currBoneIndex;

                glm::quat rotation1 = frames[frame1Index].rotation;
                glm::quat rotation2 = frames[frame2Index].rotation;
                if (glm::dot(rotation1, rotation2) < 0.0f)
                {
                    rotation1 *= -1.0f;
                }

                glm::vec3 position = glm::mix(frames[frame1Index].position, frames[frame2Index].position, frameBlendFactor);
                glm::quat rotation = glm::mix(rotation1, rotation2, frameBlendFactor);
                glm::vec3 scale = glm::mix(frames[frame1Index].scale, frames[frame2Index].scale, frameBlendFactor);

                if (glm::dot(accRotation, rotation) < 0.0f)
                {
                    accRotation *= -1.0f;
                }
                accPosition += position * blendFactor;
                accRotation += rotation * blendFactor;
                accScale += scale * blendFactor;
                totalBlendFactor += blendFactor;
            }

            accPosition /= totalBlendFactor;
            accRotation /= totalBlendFactor;
            accScale /= totalBlendFactor;
            accRotation = glm::normalize(accRotation);

            glm::mat4 baseMatrix = glm::translate(glm::mat4(1.0f), accPosition) * glm::mat4_cast(accRotation) * glm::scale(glm::mat4(1.0f), accScale);

            // Additive pass
            int32_t referenceFrameIndex = -1;
            totalBlendFactor = 0.0f;
            glm::vec3 addPosition = glm::vec3(0.0f);
            glm::quat addRotation = glm::quat(0.0f, 0.0f, 0.0f, 0.0f);
            glm::vec3 addScale = glm::vec3(0.0f);

            for (uint32_t i = 0; i < animationInstances.size(); i++)
            {
                const auto &instance = animationInstances[i];
                const auto &anim = rp.GetAnimation(instance.animation);
                const uint32_t frameOffset = anim.GetFrameOffset();
                const uint32_t frame1 = instance.frame1;
                const uint32_t frame2 = instance.frame2;
                const int32_t referenceFrame = instance.referenceFrame;
                const float frameBlendFactor = instance.frameBlendFactor;
                const float blendFactor = instance.blendFactor;
                const int32_t boneIntervalStart = instance.boneIntervalStart;
                const int32_t boneIntervalEnd = instance.boneIntervalEnd;

                if ((boneIntervalStart <= boneIntervalEnd && (currBoneIndex < boneIntervalStart || currBoneIndex > boneIntervalEnd)) ||
                    (boneIntervalStart > boneIntervalEnd && (currBoneIndex >= boneIntervalEnd && currBoneIndex <= boneIntervalStart)))
                {
                    continue;
                }

                if (blendFactor <= 0.0f || referenceFrame == -1)
                {
                    continue;
                }

                const auto &refAnim = rp.GetAnimation(instance.referenceAnimation);
                referenceFrameIndex = static_cast<int32_t>(refAnim.GetFrameOffset()) + instance.referenceFrame + currBoneIndex;

                uint32_t frame1Index = frameOffset + frame1 * mesh.GetBoneCount() + currBoneIndex;
                uint32_t frame2Index = frameOffset + frame2 * mesh.GetBoneCount() + currBoneIndex;

                glm::quat rotation1 = frames[frame1Index].rotation;
                glm::quat rotation2 = frames[frame2Index].rotation;
                if (glm::dot(rotation1, rotation2) < 0.0f)
                {
                    rotation1 *= -1.0f;
                }

                glm::vec3 position = glm::mix(frames[frame1Index].position, frames[frame2Index].position, frameBlendFactor);
                glm::quat rotation = glm::mix(rotation1, rotation2, frameBlendFactor);
                glm::vec3 scale = glm::mix(frames[frame1Index].scale, frames[frame2Index].scale, frameBlendFactor);

                if (glm::dot(addRotation, rotation) < 0.0f)
                {
                    addRotation *= -1.0f;
                }
                addPosition += position * blendFactor;
                addRotation += rotation * blendFactor;
                addScale += scale * blendFactor;
                totalBlendFactor += blendFactor;
            }

            glm::mat4 additiveMatrix = glm::mat4(1.0f);

            if (totalBlendFactor > 0.0f)
            {
                addRotation = glm::normalize(addRotation);
                glm::mat4 refrenceMatrix = glm::inverse(
                    glm::translate(glm::mat4(1.0f), frames[referenceFrameIndex].position) *
                    glm::mat4_cast(frames[referenceFrameIndex].rotation) *
                    glm::scale(glm::mat4(1.0f), frames[referenceFrameIndex].scale));

                addPosition /= totalBlendFactor;
                addRotation /= totalBlendFactor;
                addScale /= totalBlendFactor;
                addRotation = glm::normalize(addRotation);

                additiveMatrix = refrenceMatrix * glm::translate(glm::mat4(1.0f), addPosition) * glm::mat4_cast(addRotation) * glm::scale(glm::mat4(1.0f), addScale);
            }

            glm::mat4 noAdditive = baseMatrix;
            glm::mat4 withAdditive = baseMatrix * additiveMatrix;
            glm::mat4 localBoneTransform = ((1.0f - totalBlendFactor) * noAdditive + totalBlendFactor * withAdditive);
            boneTransform = localBoneTransform * boneTransform;

            {
                std::lock_guard<std::mutex> lock(m_boneTransformCacheMutex);
                m_boneTransformsCache[boneHash] = boneTransform;
            }

            currBoneIndex = bones[mesh.GetBoneOffset() + currBoneIndex].parentID;

            if (currBoneIndex == -1)
            {
                break;
            }
        }

        return boneTransform;
    }

    uint32_t SceneRenderer::GetBoneCount(RenderHandle skeletalMesh) const
    {
        const auto &rp = m_backend.GetResourcePool();
        const auto &mesh = rp.GetSkeletalMesh(skeletalMesh);
        return mesh.GetBoneCount();
    }

    glm::vec3 SceneRenderer::GetMeshCenterOffset(const RenderHandle &mesh) const
    {
        const auto &rp = m_backend.GetResourcePool();
        const auto &m = rp.GetMesh(mesh);
        return m.GetCenter();
    }

    const std::vector<glm::vec3> &SceneRenderer::GetMeshVertices(const RenderHandle &mesh) const
    {
        const auto &rp = m_backend.GetResourcePool();
        const auto &m = rp.GetMesh(mesh);
        return m.GetVertices();
    }

    const std::vector<uint32_t> &SceneRenderer::GetMeshIndices(const RenderHandle &mesh) const
    {
        const auto &rp = m_backend.GetResourcePool();
        const auto &m = rp.GetMesh(mesh);
        return m.GetIndices();
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

    RenderHandle SceneRenderer::LoadMesh(const std::string &path, bool keepInMemory)
    {
        return m_backend.LoadMesh(path, keepInMemory);
    }

    RenderHandle SceneRenderer::LoadSkeletalMesh(const std::string &path)
    {
        return m_backend.LoadSkeletalMesh(path);
    }

    // Bool for ensuring bigger mip levels are loaded, despite the performance hit
    RenderHandle SceneRenderer::LoadImage(const std::string &path, ImageType type, bool useAllMips)
    {
        return m_backend.LoadImage(path, type, useAllMips);
    }

    RenderHandle SceneRenderer::LoadFontAtlas(const std::string &path)
    {
        return m_backend.LoadFontAtlas(path);
    }

    RenderHandle SceneRenderer::LoadEnvironmentMap(const std::string &name, const std::string &irradianceFilepath, const std::string &prefilteredFilepath, const VolumeData &irradianceVolumeData, const std::vector<glm::vec3> &probePositions)
    {
        return m_backend.LoadEnvironmentMap(name, irradianceFilepath, prefilteredFilepath, irradianceVolumeData, probePositions);
    }

    RenderHandle SceneRenderer::LoadAnimation(const std::string &path)
    {
        return m_backend.LoadAnimation(path);
    }

    const VolumeData &SceneRenderer::GetIrradianceVolume(RenderHandle environmentMap) const
    {
        const VulkanEnvironmentMap &envMap = m_backend.GetResourcePool().GetEnvironmentMap(environmentMap);
        return envMap.GetIrradianceVolume();
    }

    void SceneRenderer::InvalidateBoneCache()
    {
        constexpr size_t c_maxCacheSize = 256;

        std::lock_guard<std::mutex> lock(m_boneTransformCacheMutex);
        if (m_boneTransformsCache.size() > c_maxCacheSize)
        {
            m_boneTransformsCache.clear();
            std::cout << m_numTimesCachedBoneTransform << " bone transforms were cached." << std::endl;
            m_numTimesCachedBoneTransform = 0;
        }
    }
}