#pragma once

#include <vulkan/vulkan.h>

#include <cassert>
#include <optional>
#include <vector>

#define VK_CHECK(x) assert(x == VK_SUCCESS)

namespace Vultron::VkUtil
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

    struct SwapChainSupport
    {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    QueueFamilies QueryQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
    SwapChainSupport QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

    VkCommandBuffer BeginSingleTimeCommands(VkDevice device, VkCommandPool commandPool);
    void EndSingleTimeCommands(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer);

    void CopyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void CopyBufferToImage(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkBuffer srcBuffer, VkImage dstImage);
    void TransitionImageLayout(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
}