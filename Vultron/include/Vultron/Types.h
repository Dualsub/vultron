#pragma once

#include "Vultron/Vulkan/VulkanMesh.h"
#include "Vultron/Vulkan/VulkanImage.h"

#include <glm/glm.hpp>

#include <vector>
#include <optional>

#define VLT_INVALID_HANDLE 0

#ifndef VLT_ASSETS_DIR
#define VLT_ASSETS_DIR "assets"
#endif

namespace Vultron
{
    using RenderHandle = uint64_t;
    using PoolHandle = uint32_t;

    constexpr RenderHandle c_invalidHandle = 0;
    constexpr PoolHandle c_invalidPoolHandle = 0;

    struct RenderBatch
    {
        RenderHandle mesh;
        RenderHandle material;
        uint32_t firstInstance;
        uint32_t instanceCount;

        // We have some instances that should not be submitted to the shadow pass.
        // These are stored at the beginning of the batch.
        uint32_t nonShadowCasterCount = 0;
    };

    struct StaticRenderJob
    {
        RenderHandle mesh = {};
        RenderHandle material = {};
        glm::mat4 transform = {};
        glm::vec2 texCoord = glm::vec2(0.0f);
        glm::vec2 texSize = glm::vec2(1.0f);
        glm::vec4 color = glm::vec4(1.0f);
        bool castShadows = true;

        // Compute hash of mesh and texture
        uint64_t GetHash() const
        {
            const uint64_t prime = 0x100000001b3;

            uint64_t hash = 1;
            hash = hash * prime + static_cast<uint64_t>(mesh);
            hash = hash * prime + static_cast<uint64_t>(material);
            return hash;
        }
    };

    struct AnimationInstance
    {
        RenderHandle animation = {};
        uint32_t frame1 = 0;
        uint32_t frame2 = 0;
        float frameBlendFactor = 0.0f;
        float blendFactor = 0.0f;

        RenderHandle referenceAnimation = {};
        int32_t referenceFrame = -1;
    };

    struct SkeletalRenderJob
    {
        RenderHandle mesh = {};
        RenderHandle material = {};
        glm::mat4 transform = {};
        std::vector<AnimationInstance> animations = {};
        std::array<int32_t, 3> bonesToIgnore = {-1, -1, -1};
        glm::vec4 color = glm::vec4(1.0f);

        // Compute hash of mesh and texture
        uint64_t GetHash() const
        {
            return static_cast<uint64_t>(mesh) ^ static_cast<uint64_t>(material);
        }
    };

    struct SpriteRenderJob
    {
        RenderHandle material = {};
        glm::vec2 position = {};
        glm::vec2 size = {};
        glm::vec2 texCoord = glm::vec2(0.0f);
        glm::vec2 texSize = glm::vec2(1.0f);
        glm::vec4 color = glm::vec4(1.0f);
        float rotation = 0.0f;

        // Compute hash of mesh and texture
        uint64_t GetHash() const
        {
            return static_cast<uint64_t>(material);
        }
    };

    struct FontRenderJob
    {
        RenderHandle material = {};
        glm::vec2 position = {};
        glm::vec2 size = {};
        glm::vec2 texCoord = glm::vec2(0.0f);
        glm::vec2 texSize = glm::vec2(1.0f);
        glm::vec4 color = glm::vec4(1.0f);

        // Compute hash of mesh and texture
        uint64_t GetHash() const
        {
            return static_cast<uint64_t>(material);
        }
    };

    struct ParticleEmitJob
    {
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec2 size = glm::vec2(100.0f);
        float rotation = 0.0f;
        glm::vec2 sizeSpan = glm::vec2(0.0f);
        float lifetime = 1.0f;
        glm::vec3 initialVelocity = glm::vec3(0.0f, 1.0f, 0.0f);
        float velocitySpan = 0.0f;
        float gravityFactor = 1.0f;
        float phiSpan = 0.0f;
        float thetaSpan = 0.0f;
        glm::vec2 texCoord = glm::vec2(0.0f);
        glm::vec2 texSize = glm::vec2(1.0f);
        glm::vec2 texCoordSpan = glm::vec2(0.0f);
        glm::vec4 startColor = glm::vec4(1.0f);
        std::optional<glm::vec4> endColor = std::nullopt;
        uint32_t numFrames = 1;
        float framesPerSecond = 0.0f;
        uint32_t numParticles = 1;
        float scaleIn = 0.0f;
        float scaleOut = 0.0f;
        float opacityIn = 0.0f;
        float opacityOut = 0.0f;
    };

    struct EnvironmentMapRenderJob
    {
        RenderHandle environmentMap = {};
    };

    struct LineRenderJob
    {
        glm::vec3 start = {};
        glm::vec3 end = {};
        glm::vec4 color = glm::vec4(1.0f);
    };

}