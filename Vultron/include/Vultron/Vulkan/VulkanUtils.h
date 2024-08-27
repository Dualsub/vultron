#pragma once

#include "Vultron/Vulkan/VulkanTypes.h"
#include "Vultron/Vulkan/VulkanImage.h"

#include <vulkan/vulkan.h>

#include <cassert>
#include <optional>
#include <vector>
#include <atomic>

// Check if result was ok, elese exit
#define VK_CHECK(x)                                     \
    {                                                   \
        VkResult err = x;                               \
        if (err != VK_SUCCESS)                          \
        {                                               \
            printf("Detected Vulkan error: %d\n", err); \
            abort();                                    \
        }                                               \
    }

namespace Vultron::VkUtil
{
    struct SwapChainSupport
    {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    struct BufferToImageRegion
    {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t mipLevel = 0;
        uint32_t layer = 0;
    };

    QueueFamilies QueryQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
    SwapChainSupport QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

    VkFormat FindDepthFormat(VkPhysicalDevice physicalDevice);
    VkFormat FindSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    VkCommandBuffer BeginSingleTimeCommands(VkDevice device, VkCommandPool commandPool);
    void EndSingleTimeCommands(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer, VkSemaphore signalSemaphore = VK_NULL_HANDLE, VkSemaphore waitSemaphore = VK_NULL_HANDLE, VkPipelineStageFlags waitDstStageMask = 0);

    void CopyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void CopyBufferToImage(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkBuffer srcBuffer, VkImage dstImage, const std::vector<BufferToImageRegion> &regions);
    void CopyImage(VkDevice device, VkCommandBuffer commandBuffer, VkQueue queue, VkImage srcImage, VkImage dstImage, VkImageLayout srcLayout, VkImageLayout dstLayout, VkExtent3D extent, uint32_t mipLevel = 0, uint32_t srcLayer = 0, uint32_t dstLayer = 0, uint32_t numLayers = 1);
    // If you pass a command pool, the function will create a command buffer and submit it to the queue
    void TransitionImageLayout(VkDevice device, VkCommandPool commandPool, VkQueue queue, const ImageTransition &transition);
    void TransitionImageLayout(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels = 1, uint32_t layers = 1);
    void TransitionImageLayout(VkDevice device, VkCommandBuffer commandBuffer, const ImageTransition &transition);
    void TransitionImageLayout(VkDevice device, VkCommandBuffer commandBuffer, VkQueue queue, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels = 1, uint32_t layers = 1);
    size_t GetAlignedSize(size_t offset, size_t alignment);

    void BufferBarrier(VkCommandBuffer commandBuffer, VkBuffer buffer, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
    
    struct ImageBarrierInfo
    {
        VkImageLayout oldLayout;
        VkImageLayout newLayout;
        VkAccessFlags srcAccessMask;
        VkAccessFlags dstAccessMask;
        VkPipelineStageFlags srcStageMask;
        VkPipelineStageFlags dstStageMask;
        VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    };

    void ImageBarrier(VkCommandBuffer commandBuffer, const VulkanImage& image, const ImageBarrierInfo &info);

    VkDescriptorType GetDescriptorType(DescriptorType type);
}