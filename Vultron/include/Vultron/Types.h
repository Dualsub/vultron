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
}