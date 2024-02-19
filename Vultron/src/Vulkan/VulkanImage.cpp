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
        const size_t imageSize = createInfo.info.width * createInfo.info.height * createInfo.info.depth * 4 * sizeof(uint8_t); // Doing it like this for now.
        VulkanBuffer stagingBuffer = VulkanBuffer::Create({.allocator = createInfo.allocator, .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT, .size = imageSize, .allocationUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});
        stagingBuffer.Write(createInfo.allocator, createInfo.data, imageSize);

        VkFormat format = VK_FORMAT_UNDEFINED;
        switch (createInfo.info.format)
        {
        case ImageFormat::R8G8B8A8_SRGB:
            format = VK_FORMAT_R8G8B8A8_SRGB;
            break;
        case ImageFormat::R8G8B8_SRGB:
            format = VK_FORMAT_R8G8B8_SRGB;
            break;
        default:
            assert(false && "Unsupported image format");
            break;
        }

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = createInfo.info.width;
        imageInfo.extent.height = createInfo.info.height;
        imageInfo.extent.depth = createInfo.info.depth;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.flags = 0; // Optional

        VkImage image;
        VK_CHECK(vkCreateImage(createInfo.device, &imageInfo, nullptr, &image));

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VmaAllocation allocation;

        VK_CHECK(vmaAllocateMemoryForImage(createInfo.allocator, image, &allocInfo, &allocation, nullptr));

        vmaBindImageMemory(createInfo.allocator, allocation, image);

        VkUtil::TransitionImageLayout(
            createInfo.device,
            createInfo.commandPool,
            createInfo.queue,
            image,
            format,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkUtil::CopyBufferToImage(
            createInfo.device,
            createInfo.commandPool,
            createInfo.queue,
            stagingBuffer.GetBuffer(),
            image, createInfo.info.width,
            createInfo.info.height);

        VkUtil::TransitionImageLayout(
            createInfo.device,
            createInfo.commandPool,
            createInfo.queue,
            image,
            format,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        stagingBuffer.Destroy(createInfo.allocator);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView imageView;
        VK_CHECK(vkCreateImageView(createInfo.device, &viewInfo, nullptr, &imageView));

        return VulkanImage(image, imageView, allocation, createInfo.info);
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

        ImageFormat format = ImageFormat::R8G8B8A8_SRGB;

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
                 .format = format}});

        stbi_image_free(pixels);

        return image;
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