#pragma once

#include <vulkan/vulkan.h>

#include <cassert>
#include <optional>

#define VK_CHECK(x) assert(x == VK_SUCCESS)

namespace Vultron
{
    struct QueueFamilies
    {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        QueueFamilies() = default;
        ~QueueFamilies() = default;

        bool IsComplete()
        {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    QueueFamilies QueryQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
}