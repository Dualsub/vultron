#pragma once

#include "Vultron/Vulkan/VulkanMesh.h"
#include "Vultron/Vulkan/VulkanImage.h"

#include <glm/glm.hpp>

#include <vector>

#define VLT_INVALID_HANDLE 0

#ifndef VLT_ASSETS_DIR
#define VLT_ASSETS_DIR "assets"
#endif

namespace Vultron
{

    // TODO: Make indecies instead of pointers in the future
    using RenderHandle = uint64_t;

    struct RenderBatch
    {
        RenderHandle mesh;
        RenderHandle material;
        uint32_t firstInstance;
        uint32_t instanceCount;
    };

    struct StaticRenderJob
    {
        RenderHandle mesh = {};
        RenderHandle material = {};
        glm::mat4 transform = {};

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
    };

    struct SkeletalRenderJob
    {
        RenderHandle mesh = {};
        RenderHandle material = {};
        glm::mat4 transform = {};
        std::vector<AnimationInstance> animations = {};

        // Compute hash of mesh and texture
        uint64_t GetHash() const
        {
            return static_cast<uint64_t>(mesh) ^ static_cast<uint64_t>(material);
        }
    };
}