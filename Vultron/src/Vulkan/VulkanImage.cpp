#include "Vultron/Vulkan/VulkanImage.h"

#include "Vultron/Vulkan/VulkanBuffer.h"
#include "Vultron/Vulkan/VulkanBuffer.h"
#include "Vultron/Vulkan/VulkanUtils.h"
#include "Vultron/Vulkan/VulkanInitializers.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <fstream>
#include <iostream>
#include <algorithm>

namespace Vultron
{
    std::vector<MipInfo> VulkanImage::ReadMipsFromFile(std::fstream &file, uint32_t width, uint32_t height, uint32_t startMip, uint32_t numMipLevels, uint32_t numChannels, uint32_t numBytesPerChannel)
    {
        std::vector<MipInfo> mips;
        mips.reserve(numMipLevels);

        for (uint32_t i = 0; i < startMip; i++)
        {
            uint32_t mipWidth = width >> i;
            uint32_t mipHeight = height >> i;
            file.seekg(mipWidth * mipHeight * numChannels * numBytesPerChannel, std::ios::cur);
        }

        for (int32_t i = startMip; i < numMipLevels; i++)
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

    VulkanImage VulkanImage::Create(const VulkanContext &context, const ImageCreateInfo &createInfo)
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
        VK_CHECK(vkCreateImage(context.GetDevice(), &imageInfo, nullptr, &image));

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VmaAllocation allocation;

        VK_CHECK(vmaAllocateMemoryForImage(context.GetAllocator(), image, &allocInfo, &allocation, nullptr));

        vmaBindImageMemory(context.GetAllocator(), allocation, image);

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
        case ImageType::Texture2DArray:
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
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
        VK_CHECK(vkCreateImageView(context.GetDevice(), &viewInfo, nullptr, &imageView));

        VulkanImage outImage = VulkanImage(image, imageView, allocation, createInfo.info);

        return outImage;
    }

    VulkanImage VulkanImage::CreateFromFile(const VulkanContext &context, VkCommandPool commandPool, const ImageFromFileCreateInfo &createInfo)
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
            else if (header.numChannels == 2)
                format = VK_FORMAT_R32G32_SFLOAT;
        }
        else if (header.numBytesPerChannel == 2)
        {
            if (header.numChannels == 2)
                format = VK_FORMAT_R16G16_SFLOAT;
        }

        const uint32_t targetStartMip = 2u;
        // Make sure that the target start mip is not greater than the number of mips in the image.
        const uint32_t startMip = std::min(createInfo.useAllMips ? 0 : targetStartMip, header.numMipLevels - 1);
        VulkanImage image = VulkanImage::Create(
            context,
            {
                .info = {
                    .width = header.width >> startMip,
                    .height = header.height >> startMip,
                    .mipLevels = header.numMipLevels - startMip,
                    .format = format,
                    .layers = header.numLayers,
                },
                .type = createInfo.type == ImageType::None ? header.type : createInfo.type,
            });

        std::vector<std::vector<MipInfo>> layers;
        layers.reserve(header.numLayers);
        for (uint32_t i = 0; i < header.numLayers; i++)
        {
            std::vector<MipInfo> mips = VulkanImage::ReadMipsFromFile(file, header.width, header.height, startMip, header.numMipLevels, header.numChannels, header.numBytesPerChannel);
            layers.push_back(std::move(mips));
        }

        image.UploadData(context, commandPool, createInfo.imageTransitionQueue, header.numBytesPerChannel * header.numChannels, layers);

        file.close();

        s_memoryUsage += ((header.width * header.height) >> startMip) * header.numChannels * header.numBytesPerChannel * (header.numLayers - startMip) * 2.0f * (1.0f - std::pow(0.5f, header.numMipLevels));

        return image;
    }

    void VulkanImage::UploadData(const VulkanContext &context, VkCommandPool commandPool, ImageTransitionQueue *imageTransitionQueue, uint32_t bytesPerPixel, const std::vector<std::vector<MipInfo>> &layers)
    {
        const uint32_t layersCount = static_cast<uint32_t>(layers.size());
        const uint32_t mipLevels = static_cast<uint32_t>(layers[0].size());
        const uint32_t width = layers[0][0].width;
        const uint32_t height = layers[0][0].height;

        // If the image transition queue is a parameter we will upload the data with
        // the transfer queue and the final transition on the graphics queue,
        // otherwise we will do everything on the graphics queue.

        VkQueue queue = imageTransitionQueue ? context.GetTransferQueue() : context.GetGraphicsQueue();

        VkUtil::TransitionImageLayout(
            context.GetDevice(),
            commandPool,
            queue,
            m_image,
            m_info.format,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            mipLevels,
            layersCount);

        VulkanBuffer stagingBuffer = VulkanBuffer::Create({.allocator = context.GetAllocator(), .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT, .size = width * height * bytesPerPixel, .allocationUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});

        for (uint32_t i = 0; i < layers.size(); i++)
        {
            for (uint32_t j = 0; j < layers[i].size(); j++)
            {
                const MipInfo &mip = layers[i][j];
                const size_t imageSize = mip.width * mip.height * bytesPerPixel; // Doing it like this for now.
                stagingBuffer.Write(context.GetAllocator(), mip.data.get(), imageSize);

                VkUtil::CopyBufferToImage(
                    context.GetDevice(),
                    commandPool,
                    queue,
                    stagingBuffer.GetBuffer(),
                    m_image,
                    {{.width = mip.width, .height = mip.height, .mipLevel = j, .layer = i}});
            }
        }

        stagingBuffer.Destroy(context.GetAllocator());

        ImageTransition transitionForGraphics = VkInit::CreateImageTransitionBarrier(
            m_image,
            m_info.format,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            mipLevels,
            layersCount,
            imageTransitionQueue ? context.GetTransferQueueFamily() : VK_QUEUE_FAMILY_IGNORED,
            imageTransitionQueue ? context.GetGraphicsQueueFamily() : VK_QUEUE_FAMILY_IGNORED);

        if (imageTransitionQueue)
        {
            VkSemaphoreCreateInfo semaphoreInfo = {};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

            VK_CHECK(vkCreateSemaphore(context.GetDevice(), &semaphoreInfo, nullptr, &transitionForGraphics.semaphore));

            transitionForGraphics.barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            transitionForGraphics.srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            transitionForGraphics.barrier.dstAccessMask = VK_IMAGE_LAYOUT_UNDEFINED;
            transitionForGraphics.dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        }

        VkCommandBuffer commandBuffer = VkUtil::BeginSingleTimeCommands(context.GetDevice(), commandPool);

        VkUtil::TransitionImageLayout(
            context.GetDevice(),
            commandBuffer,
            transitionForGraphics);

        VkUtil::EndSingleTimeCommands(
            context.GetDevice(),
            commandPool,
            queue,
            commandBuffer,
            transitionForGraphics.semaphore);

        if (imageTransitionQueue)
        {
            transitionForGraphics.barrier.srcAccessMask = VK_IMAGE_LAYOUT_UNDEFINED;
            transitionForGraphics.srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            transitionForGraphics.barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            transitionForGraphics.dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            imageTransitionQueue->Push(transitionForGraphics);
        }
    }

    void VulkanImage::TransitionLayout(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        VkUtil::TransitionImageLayout(device, commandPool, queue, m_image, m_info.format, oldLayout, newLayout, m_info.mipLevels, m_info.layers);
    }

    void VulkanImage::TransitionLayout(VkDevice device, VkCommandBuffer commandBuffer, VkQueue queue, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        VkUtil::TransitionImageLayout(device, commandBuffer, queue, m_image, m_info.format, oldLayout, newLayout, m_info.mipLevels, m_info.layers);
    }

    void VulkanImage::Destroy(const VulkanContext &context)
    {
        vkDestroyImageView(context.GetDevice(), m_imageView, nullptr);
        vmaDestroyImage(context.GetAllocator(), m_image, m_allocation);
    }

    void VulkanImage::SaveImageToFile(const VulkanContext &context, VkCommandPool commandPool, const VulkanImage &image, const std::string &filepath)
    {
        std::fstream file(filepath, std::ios::out | std::ios::binary);

        ImageFileHeader header;
        header.width = image.m_info.width;
        header.height = image.m_info.height;
        header.numMipLevels = image.m_info.mipLevels;
        header.numLayers = image.m_info.layers;

        switch (image.m_info.format)
        {
        case VK_FORMAT_R8G8B8A8_UNORM:
            header.numChannels = 4;
            header.numBytesPerChannel = 1;
            break;
        case VK_FORMAT_R32G32B32_SFLOAT:
            header.numChannels = 3;
            header.numBytesPerChannel = 4;
            break;
        case VK_FORMAT_R32G32B32A32_SFLOAT:
            header.numChannels = 4;
            header.numBytesPerChannel = 4;
            break;
        case VK_FORMAT_R32G32_SFLOAT:
            header.numChannels = 2;
            header.numBytesPerChannel = 4;
            break;
        case VK_FORMAT_R16G16_SFLOAT:
            header.numChannels = 2;
            header.numBytesPerChannel = 2;
            break;
        default:
            header.numChannels = 4;
            header.numBytesPerChannel = 1;
            break;
        }
        header.type = image.m_info.layers == 6 ? ImageType::Cubemap : ImageType::Texture2D;

        file.write(reinterpret_cast<char *>(&header), sizeof(header));

        ImageTransition transition = VkInit::CreateImageTransitionBarrier(
            image.GetImage(),
            image.m_info.format,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image.m_info.mipLevels,
            image.m_info.layers);

        VkUtil::TransitionImageLayout(
            context.GetDevice(),
            commandPool,
            context.GetGraphicsQueue(),
            transition);

        // Copy image to buffer
        VulkanBuffer stagingBuffer = VulkanBuffer::Create({.allocator = context.GetAllocator(), .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT, .size = image.m_info.width * image.m_info.height * header.numChannels * header.numBytesPerChannel, .allocationUsage = VMA_MEMORY_USAGE_GPU_TO_CPU});

        const char *data;
        vmaMapMemory(context.GetAllocator(), stagingBuffer.GetAllocation(), (void **)&data);
        for (uint32_t i = 0; i < header.numLayers; i++)
        {
            for (uint32_t j = 0; j < header.numMipLevels; j++)
            {
                size_t mipWidth = header.width >> j;
                size_t mipHeight = header.height >> j;
                size_t mipSize = mipWidth * mipHeight * header.numChannels * header.numBytesPerChannel;

                VkCommandBuffer commandBuffer = VkUtil::BeginSingleTimeCommands(context.GetDevice(), commandPool);

                VkBufferImageCopy region{};
                region.bufferOffset = 0;
                region.bufferRowLength = 0;
                region.bufferImageHeight = 0;

                region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                region.imageSubresource.mipLevel = j;
                region.imageSubresource.baseArrayLayer = i;
                region.imageSubresource.layerCount = 1;

                region.imageOffset = {0, 0, 0};
                region.imageExtent = {static_cast<uint32_t>(mipWidth), static_cast<uint32_t>(mipHeight), 1};

                vkCmdCopyImageToBuffer(commandBuffer,
                                       image.GetImage(),
                                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                       stagingBuffer.GetBuffer(),
                                       1,
                                       &region);

                VkUtil::EndSingleTimeCommands(
                    context.GetDevice(),
                    commandPool,
                    context.GetGraphicsQueue(),
                    commandBuffer);

                file.write(data, mipSize);
            }
        }

        file.close();

        vmaUnmapMemory(context.GetAllocator(), stagingBuffer.GetAllocation());

        stagingBuffer.Destroy(context.GetAllocator());

        transition = VkInit::CreateImageTransitionBarrier(
            image.m_image,
            image.m_info.format,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            image.m_info.mipLevels,
            image.m_info.layers);

        VkUtil::TransitionImageLayout(
            context.GetDevice(),
            commandPool,
            context.GetGraphicsQueue(),
            transition);

        std::cout << "Saved image to " << filepath << std::endl;
    }
}