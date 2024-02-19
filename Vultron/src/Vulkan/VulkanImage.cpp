#include "Vultron/Vulkan/VulkanImage.h"

#include "Vultron/Vulkan/VulkanBuffer.h"
#include "Vultron/Vulkan/VulkanUtils.h"

namespace Vultron
{
    VulkanImage VulkanImage::Create(VkDevice device, VkCommandPool commandPool, VkQueue queue, VmaAllocator allocator, void *data, const ImageInfo &info)
    {
        const size_t imageSize = info.width * info.height * info.depth * sizeof(uint8_t); // Doing it like this for now.
        VulkanBuffer stagingBuffer = VulkanBuffer::Create(allocator, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, imageSize, VMA_MEMORY_USAGE_CPU_TO_GPU);
        stagingBuffer.Write(allocator, data, imageSize);

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = info.width;
        imageInfo.extent.height = info.height;
        imageInfo.extent.depth = info.depth;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.flags = 0; // Optional

        VkImage image;
        VK_CHECK(vkCreateImage(device, &imageInfo, nullptr, &image));

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VmaAllocation allocation;

        VK_CHECK(vmaAllocateMemoryForImage(allocator, image, &allocInfo, &allocation, nullptr));

        VkUtil::TransitionImageLayout(
            device,
            commandPool,
            queue,
            image,
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkUtil::CopyBufferToImage(
            device,
            commandPool,
            queue,
            stagingBuffer.GetBuffer(),
            image);

        VkUtil::TransitionImageLayout(
            device,
            commandPool,
            queue,
            image,
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        stagingBuffer.Destroy(allocator);

        return VulkanImage(image, allocation, info);
    }

    void VulkanImage::Destroy(VmaAllocator allocator)
    {
        vmaDestroyImage(allocator, m_image, m_allocation);
    }
}