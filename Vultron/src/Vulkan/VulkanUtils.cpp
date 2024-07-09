#include "Vultron/Vulkan/VulkanUtils.h"

#include "Vultron/Vulkan/VulkanInitializers.h"

#include <iostream>
#include <vector>

namespace Vultron::VkUtil
{
    QueueFamilies QueryQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
    {
        QueueFamilies families;
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto &queueFamily : queueFamilies)
        {
            if (
                queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT &&
                queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT &&
                queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT &&
                queueFamily.queueCount >= 2)
            {
                families.graphicsFamily = i;
                families.computeFamily = i;
            }

            if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT && !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
            {
                families.transferFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

            if (presentSupport)
            {
                families.presentFamily = i;
            }

            if (families.IsComplete())
            {
                break;
            }

            i++;
        }

        return families;
    }

    SwapChainSupport QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
    {
        SwapChainSupport support;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &support.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

        if (formatCount > 0)
        {
            support.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, support.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
        if (presentModeCount > 0)
        {
            support.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, support.presentModes.data());
        }

        return support;
    }

    VkFormat FindDepthFormat(VkPhysicalDevice physicalDevice)
    {
        return FindSupportedFormat(physicalDevice, {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    VkFormat FindSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
    {
        for (VkFormat format : candidates)
        {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
            {
                return format;
            }
            else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
            {
                return format;
            }
        }

        assert(false && "Failed to find supported format");
        return VK_FORMAT_UNDEFINED;
    }

    VkCommandBuffer BeginSingleTimeCommands(VkDevice device, VkCommandPool commandPool)
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    void EndSingleTimeCommands(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer, VkSemaphore signalSemaphore, VkSemaphore waitSemaphore, VkPipelineStageFlags waitDstStageMask)
    {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        if (waitSemaphore != VK_NULL_HANDLE)
        {
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &waitSemaphore;
            submitInfo.pWaitDstStageMask = &waitDstStageMask;
        }

        if (signalSemaphore != VK_NULL_HANDLE)
        {
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &signalSemaphore;
        }

        vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);

        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }

    void CopyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
    {
        VkCommandBuffer commandBuffer = BeginSingleTimeCommands(device, commandPool);

        VkBufferCopy copyRegion = {};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        EndSingleTimeCommands(device, commandPool, queue, commandBuffer);
    }

    void CopyBufferToImage(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkBuffer srcBuffer, VkImage dstImage, const std::vector<BufferToImageRegion> &regions)
    {
        VkCommandBuffer commandBuffer = BeginSingleTimeCommands(device, commandPool);

        std::vector<VkBufferImageCopy> vkRegions(regions.size());

        for (size_t i = 0; i < regions.size(); i++)
        {
            const BufferToImageRegion &region = regions[i];
            vkRegions[i].bufferOffset = 0;
            vkRegions[i].bufferRowLength = 0;
            vkRegions[i].bufferImageHeight = 0;

            vkRegions[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            vkRegions[i].imageSubresource.mipLevel = region.mipLevel;
            vkRegions[i].imageSubresource.baseArrayLayer = region.layer;
            vkRegions[i].imageSubresource.layerCount = 1;

            vkRegions[i].imageOffset = {0, 0, 0};
            vkRegions[i].imageExtent = {region.width, region.height, 1};
        }

        vkCmdCopyBufferToImage(commandBuffer, srcBuffer, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(vkRegions.size()), vkRegions.data());

        EndSingleTimeCommands(device, commandPool, queue, commandBuffer);
    }

    void CopyImage(VkDevice device, VkCommandBuffer commandBuffer, VkQueue queue, VkImage srcImage, VkImage dstImage, VkImageLayout srcLayout, VkImageLayout dstLayout, VkExtent3D extent, uint32_t mipLevel, uint32_t srcLayer, uint32_t dstLayer, uint32_t numLayers)
    {
        VkImageCopy copyRegion = {};
        copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.srcSubresource.mipLevel = mipLevel;
        copyRegion.srcSubresource.baseArrayLayer = srcLayer;
        copyRegion.srcSubresource.layerCount = numLayers;
        copyRegion.srcOffset = {0, 0, 0};

        copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.dstSubresource.mipLevel = mipLevel;
        copyRegion.dstSubresource.baseArrayLayer = dstLayer;
        copyRegion.dstSubresource.layerCount = numLayers;
        copyRegion.dstOffset = {0, 0, 0};

        copyRegion.extent = extent;

        vkCmdCopyImage(commandBuffer, srcImage, srcLayout, dstImage, dstLayout, 1, &copyRegion);
    }

    void TransitionImageLayout(VkDevice device, VkCommandBuffer commandBuffer, const ImageTransition &transition)
    {
        vkCmdPipelineBarrier(commandBuffer, transition.srcStage, transition.dstStage, 0, 0, nullptr, 0, nullptr, 1, &transition.barrier);
    }

    void TransitionImageLayout(VkDevice device, VkCommandBuffer commandBuffer, VkQueue queue, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels, uint32_t layers)
    {
        ImageTransition transition = VkInit::CreateImageTransitionBarrier(image, format, oldLayout, newLayout, mipLevels, layers);
        TransitionImageLayout(device, commandBuffer, transition);
    }

    void TransitionImageLayout(VkDevice device, VkCommandPool commandPool, VkQueue queue, const ImageTransition &transition)
    {
        VkCommandBuffer commandBuffer = BeginSingleTimeCommands(device, commandPool);

        TransitionImageLayout(device, commandBuffer, transition);

        EndSingleTimeCommands(device, commandPool, queue, commandBuffer);
    }

    void TransitionImageLayout(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels, uint32_t layers)
    {
        VkCommandBuffer commandBuffer = BeginSingleTimeCommands(device, commandPool);

        TransitionImageLayout(device, commandBuffer, queue, image, format, oldLayout, newLayout, mipLevels, layers);

        EndSingleTimeCommands(device, commandPool, queue, commandBuffer);
    }

    size_t GetAlignedSize(size_t offset, size_t alignment)
    {
        return (offset + alignment - 1) & ~(alignment - 1);
    }

    VkDescriptorType GetDescriptorType(DescriptorType type)
    {
        switch (type)
        {
        case DescriptorType::UniformBuffer:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case DescriptorType::CombinedImageSampler:
            return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case DescriptorType::StorageBuffer:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        default:
            std::cerr << "Unknown descriptor type" << std::endl;
            abort();
            break;
        }

        return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }

}