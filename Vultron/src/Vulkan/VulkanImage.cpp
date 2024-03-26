#include "Vultron/Vulkan/VulkanImage.h"

#include "Vultron/Vulkan/VulkanBuffer.h"
#include "Vultron/Vulkan/VulkanBuffer.h"
#include "Vultron/Vulkan/VulkanUtils.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <fstream>
#include <iostream>
#include <algorithm>

namespace Vultron
{
    std::vector<MipInfo> VulkanImage::ReadMipsFromFile(std::fstream &file, uint32_t numMipLevels, uint32_t numChannels, uint32_t numBytesPerChannel)
    {
        struct MipLevelHeader
        {
            uint32_t width;
            uint32_t height;
        } mipLevelHeader;

        std::vector<MipInfo> mips;
        mips.reserve(numMipLevels);
        for (uint32_t i = 0; i < numMipLevels; i++)
        {
            file.read(reinterpret_cast<char *>(&mipLevelHeader), sizeof(mipLevelHeader));
            std::unique_ptr<uint8_t> data = std::unique_ptr<uint8_t>(new uint8_t[mipLevelHeader.width * mipLevelHeader.height * numChannels * numBytesPerChannel]);
            file.read(reinterpret_cast<char *>(data.get()), mipLevelHeader.width * mipLevelHeader.height * numChannels * numBytesPerChannel);
            mips.push_back({.width = mipLevelHeader.width, .height = mipLevelHeader.height, .depth = 1, .mipLevel = i, .data = std::move(data)});
        }

        return mips;
    }

    VulkanImage VulkanImage::Create(const ImageCreateInfo &createInfo)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = createInfo.info.width;
        imageInfo.extent.height = createInfo.info.height;
        imageInfo.extent.depth = createInfo.info.depth;
        imageInfo.mipLevels = createInfo.info.mipLevels;
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
        viewInfo.subresourceRange.levelCount = createInfo.info.mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView imageView;
        VK_CHECK(vkCreateImageView(createInfo.device, &viewInfo, nullptr, &imageView));

        VulkanImage outImage = VulkanImage(image, imageView, allocation, createInfo.info);

        return outImage;
    }

    Ptr<VulkanImage> VulkanImage::CreatePtr(const ImageCreateInfo &createInfo)
    {
        return MakePtr<VulkanImage>(VulkanImage::Create(createInfo));
    }

    VulkanImage VulkanImage::CreateFromFile(const ImageFromFileCreateInfo &createInfo)
    {
        std::fstream file(createInfo.filepath, std::ios::in | std::ios::binary);
        assert(file.is_open() && "Failed to open file");

        struct Header
        {
            uint32_t numChannels;
            uint32_t numBytesPerChannel;
            uint32_t numMipLevels;
        } header;

        file.seekg(0);

        file.read(reinterpret_cast<char *>(&header), sizeof(header));

        header.numMipLevels = std::clamp(header.numMipLevels, 1u, 10u);
        std::vector<MipInfo> mips = VulkanImage::ReadMipsFromFile(file, header.numMipLevels, header.numChannels, header.numBytesPerChannel);

        file.close();

        VulkanImage image = VulkanImage::Create(
            {.device = createInfo.device,
             .commandPool = createInfo.commandPool,
             .queue = createInfo.queue,
             .allocator = createInfo.allocator,
             .info = {
                 .width = static_cast<uint32_t>(mips[0].width),
                 .height = static_cast<uint32_t>(mips[0].height),
                 .depth = 1,
                 .mipLevels = static_cast<uint32_t>(mips.size()),
                 .format = createInfo.format}});

        image.UploadData(createInfo.device, createInfo.allocator, createInfo.commandPool, createInfo.queue, mips);

        return image;
    }

    void VulkanImage::UploadData(VkDevice device, VmaAllocator allocator, VkCommandPool commandPool, VkQueue queue, const std::vector<MipInfo> &mips)
    {
        uint32_t mipLevels = static_cast<uint32_t>(mips.size());

        VkUtil::TransitionImageLayout(
            device,
            commandPool,
            queue,
            m_image,
            m_info.format,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            mipLevels);

        for (uint32_t i = 0; i < mips.size(); i++)
        {
            const MipInfo &mip = mips[i];
            const size_t imageSize = mip.width * mip.height * 4 * sizeof(uint8_t); // Doing it like this for now.
            VulkanBuffer stagingBuffer = VulkanBuffer::Create({.allocator = allocator, .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT, .size = imageSize, .allocationUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});
            stagingBuffer.Write(allocator, mip.data.get(), imageSize);

            VkUtil::CopyBufferToImage(
                device,
                commandPool,
                queue,
                stagingBuffer.GetBuffer(),
                m_image,
                mip.width,
                mip.height,
                i);

            stagingBuffer.Destroy(allocator);
        }

        VkUtil::TransitionImageLayout(
            device,
            commandPool,
            queue,
            m_image,
            m_info.format,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            mipLevels);
    }

    void VulkanImage::TransitionLayout(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        VkUtil::TransitionImageLayout(device, commandPool, queue, m_image, m_info.format, oldLayout, newLayout);
    }

    Ptr<VulkanImage> VulkanImage::CreatePtrFromFile(const ImageFromFileCreateInfo &createInfo)
    {
        return MakePtr<VulkanImage>(VulkanImage::CreateFromFile(createInfo));
    }

    void VulkanImage::Destroy(const VulkanContext &context)
    {
        vkDestroyImageView(context.GetDevice(), m_imageView, nullptr);
        vmaDestroyImage(context.GetAllocator(), m_image, m_allocation);
    }
}