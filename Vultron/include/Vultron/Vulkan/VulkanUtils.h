#pragma once

#include "Vultron/Vulkan/VulkanTypes.h"

#include <vulkan/vulkan.h>

#include <cassert>
#include <optional>
#include <vector>

// Check if result was ok, elese exit
#define VK_CHECK(x)                                     \
    {                                                   \
        VkResult err = x;                               \
        if (err)                                        \
        {                                               \
            printf("Detected Vulkan error: %d\n", err); \
            assert(!err);                               \
        }                                               \
    }

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

    VkFormat FindDepthFormat(VkPhysicalDevice physicalDevice);
    VkFormat FindSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    VkCommandBuffer BeginSingleTimeCommands(VkDevice device, VkCommandPool commandPool);
    void EndSingleTimeCommands(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer);

    void CopyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void CopyBufferToImage(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkBuffer srcBuffer, VkImage dstImage, uint32_t width, uint32_t height, uint32_t mipLevel = 0);
    void TransitionImageLayout(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels = 1);

    size_t GetAlignedSize(size_t offset, size_t alignment);

    VkDescriptorType GetDescriptorType(DescriptorType type);
}