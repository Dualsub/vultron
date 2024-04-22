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
    std::vector<MipInfo> VulkanImage::ReadMipsFromFile(std::fstream &file, uint32_t width, uint32_t height, uint32_t numMipLevels, uint32_t numChannels, uint32_t numBytesPerChannel)
    {
        std::vector<MipInfo> mips;
        mips.reserve(numMipLevels);

        for (int32_t i = 0; i < numMipLevels; i++)
        {
            uint32_t mipWidth = width >> i;
            uint32_t mipHeight = height >> i;

            std::unique_ptr<uint8_t> data = std::unique_ptr<uint8_t>(new uint8_t[mipWidth * mipHeight * numChannels * numBytesPerChannel]);
            file.read(reinterpret_cast<char *>(data.get()), mipWidth * mipHeight * numChannels * numBytesPerChannel);
            MipInfo mip;
            mip.width = mipWidth;
            mip.height = mipHeight;
            mip.depth = 1;
            mip.mipLevel = i;
            mip.data = std::move(data);
            mips.push_back(std::move(mip));
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
        imageInfo.arrayLayers = createInfo.info.layers;
        imageInfo.format = createInfo.info.format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | createInfo.additionalUsageFlags;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.flags = createInfo.createFlags;
        if (createInfo.type == ImageType::Cubemap)
        {
            imageInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            assert(createInfo.info.layers == 6 && "Cubemaps must have 6 layers");
        }

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

        switch (createInfo.type)
        {
        case ImageType::Texture2D:
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            break;
        case ImageType::Cubemap:
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
            break;
        default:
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            break;
        }

        viewInfo.format = createInfo.info.format;
        viewInfo.subresourceRange.aspectMask = createInfo.aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = createInfo.info.mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = createInfo.info.layers;

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

        ImageFileHeader header;

        file.seekg(0);

        file.read(reinterpret_cast<char *>(&header), sizeof(header));

        if (header.numMipLevels > 16 || header.numMipLevels <= 0)
        {
            std::cout << "Image " << createInfo.filepath << " has " << header.numMipLevels << " mips." << std::endl;
            std::cout << "Mip levels greater than 16 are not supported." << std::endl;

            abort();
        }

        VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
        if (header.numBytesPerChannel == 4) // We are using hdr images
        {
            if (header.numChannels == 3)
                format = VK_FORMAT_R32G32B32_SFLOAT;
            else if (header.numChannels == 4)
                format = VK_FORMAT_R32G32B32A32_SFLOAT;
        }

        VulkanImage image = VulkanImage::Create(
            {
                .device = createInfo.device,
                .commandPool = createInfo.commandPool,
                .queue = createInfo.queue,
                .allocator = createInfo.allocator,
                .info = {
                    .width = static_cast<uint32_t>(header.width),
                    .height = static_cast<uint32_t>(header.height),
                    .mipLevels = static_cast<uint32_t>(header.numMipLevels),
                    .format = format,
                    .layers = static_cast<uint32_t>(header.numLayers),
                },
                .type = header.type,
            });

        std::vector<std::vector<MipInfo>> layers;
        layers.reserve(header.numLayers);
        for (uint32_t i = 0; i < header.numLayers; i++)
        {
            std::vector<MipInfo> mips = VulkanImage::ReadMipsFromFile(file, header.width, header.height, header.numMipLevels, header.numChannels, header.numBytesPerChannel);
            layers.push_back(std::move(mips));
        }

        image.UploadData(createInfo.device, createInfo.allocator, createInfo.commandPool, createInfo.queue, header.numBytesPerChannel * header.numChannels, layers);

        file.close();

        return image;
    }

    void VulkanImage::UploadData(VkDevice device, VmaAllocator allocator, VkCommandPool commandPool, VkQueue queue, uint32_t bytesPerPixel, const std::vector<std::vector<MipInfo>> &layers)
    {
        const uint32_t layersCount = static_cast<uint32_t>(layers.size());
        const uint32_t mipLevels = static_cast<uint32_t>(layers[0].size());
        const uint32_t width = layers[0][0].width;
        const uint32_t height = layers[0][0].height;

        VkUtil::TransitionImageLayout(
            device,
            commandPool,
            queue,
            m_image,
            m_info.format,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            mipLevels,
            layersCount);

        VulkanBuffer stagingBuffer = VulkanBuffer::Create({.allocator = allocator, .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT, .size = width * height * bytesPerPixel, .allocationUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});

        for (uint32_t i = 0; i < layers.size(); i++)
        {
            for (uint32_t j = 0; j < layers[i].size(); j++)
            {
                const MipInfo &mip = layers[i][j];
                const size_t imageSize = mip.width * mip.height * bytesPerPixel; // Doing it like this for now.
                stagingBuffer.Write(allocator, mip.data.get(), imageSize);

                VkUtil::CopyBufferToImage(
                    device,
                    commandPool,
                    queue,
                    stagingBuffer.GetBuffer(),
                    m_image,
                    {{.width = mip.width, .height = mip.height, .mipLevel = j, .layer = i}});
            }
        }

        stagingBuffer.Destroy(allocator);

        VkUtil::TransitionImageLayout(
            device,
            commandPool,
            queue,
            m_image,
            m_info.format,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            mipLevels,
            layersCount);
    }

    void VulkanImage::TransitionLayout(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        VkUtil::TransitionImageLayout(device, commandPool, queue, m_image, m_info.format, oldLayout, newLayout, m_info.mipLevels, m_info.layers);
    }

    void VulkanImage::TransitionLayout(VkDevice device, VkCommandBuffer commandBuffer, VkQueue queue, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        VkUtil::TransitionImageLayout(device, commandBuffer, queue, m_image, m_info.format, oldLayout, newLayout, m_info.mipLevels, m_info.layers);
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