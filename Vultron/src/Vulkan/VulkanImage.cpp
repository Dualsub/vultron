#include "Vultron/Vulkan/VulkanImage.h"

#include "Vultron/Vulkan/VulkanBuffer.h"
#include "Vultron/Vulkan/VulkanUtils.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iostream>

namespace Vultron
{
    VulkanImage VulkanImage::Create(const ImageCreateInfo &createInfo)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = createInfo.info.width;
        imageInfo.extent.height = createInfo.info.height;
        imageInfo.extent.depth = createInfo.info.depth;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = createInfo.info.format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | createInfo.additionalUsageFlags;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

        VkImage image;
        VK_CHECK(vkCreateImage(createInfo.device, &imageInfo, nullptr, &image));

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VmaAllocation allocation;

        VK_CHECK(vmaAllocateMemoryForImage(createInfo.allocator, image, &allocInfo, &allocation, nullptr));

        vmaBindImageMemory(createInfo.allocator, allocation, image);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = createInfo.info.format;
        viewInfo.subresourceRange.aspectMask = createInfo.aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView imageView;
        VK_CHECK(vkCreateImageView(createInfo.device, &viewInfo, nullptr, &imageView));

        VulkanImage outImage = VulkanImage(image, imageView, allocation, createInfo.info);

        if (createInfo.data)
        {
            outImage.UploadData(createInfo.device, createInfo.allocator, createInfo.commandPool, createInfo.queue, createInfo.data, createInfo.info.width * createInfo.info.height * createInfo.info.depth * 4 * sizeof(uint8_t));
        }

        return outImage;
    }

    Ptr<VulkanImage> VulkanImage::CreatePtr(const ImageCreateInfo &createInfo)
    {
        return MakePtr<VulkanImage>(VulkanImage::Create(createInfo));
    }

    VulkanImage VulkanImage::CreateFromFile(const ImageFromFileCreateInfo &createInfo)
    {
        int texWidth, texHeight, texChannels;
        stbi_uc *pixels = stbi_load(createInfo.filepath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        assert(pixels);

        stbi_set_flip_vertically_on_load(true);

        VulkanImage image = VulkanImage::Create(
            {.device = createInfo.device,
             .commandPool = createInfo.commandPool,
             .queue = createInfo.queue,
             .allocator = createInfo.allocator,
             .data = pixels,
             .info = {
                 .width = static_cast<uint32_t>(texWidth),
                 .height = static_cast<uint32_t>(texHeight),
                 .depth = 1,
                 .format = createInfo.format}});

        stbi_image_free(pixels);

        return image;
    }

    void VulkanImage::UploadData(VkDevice device, VmaAllocator allocator, VkCommandPool commandPool, VkQueue queue, void *data, size_t size)
    {
        const size_t imageSize = m_info.width * m_info.height * m_info.depth * 4 * sizeof(uint8_t); // Doing it like this for now.
        assert(size <= imageSize && "Data size is larger than image size");
        VulkanBuffer stagingBuffer = VulkanBuffer::Create({.allocator = allocator, .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT, .size = imageSize, .allocationUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});
        stagingBuffer.Write(allocator, data, imageSize);

        VkUtil::TransitionImageLayout(
            device,
            commandPool,
            queue,
            m_image,
            m_info.format,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkUtil::CopyBufferToImage(
            device,
            commandPool,
            queue,
            stagingBuffer.GetBuffer(),
            m_image, m_info.width,
            m_info.height);

        VkUtil::TransitionImageLayout(
            device,
            commandPool,
            queue,
            m_image,
            m_info.format,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        stagingBuffer.Destroy(allocator);
    }

    void VulkanImage::TransitionLayout(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        VkUtil::TransitionImageLayout(device, commandPool, queue, m_image, m_info.format, oldLayout, newLayout);
    }

    Ptr<VulkanImage> VulkanImage::CreatePtrFromFile(const ImageFromFileCreateInfo &createInfo)
    {
        return MakePtr<VulkanImage>(VulkanImage::CreateFromFile(createInfo));
    }

    void VulkanImage::Destroy(VkDevice device, VmaAllocator allocator)
    {
        vkDestroyImageView(device, m_imageView, nullptr);
        vmaDestroyImage(allocator, m_image, m_allocation);
    }
}