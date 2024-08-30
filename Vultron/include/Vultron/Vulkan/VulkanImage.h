#pragma once

#include "Vultron/Core/Core.h"
#include "Vultron/Vulkan/VulkanTypes.h"
#include "Vultron/Vulkan/VulkanContext.h"

#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"

#include <string>
#include <vector>

namespace Vultron
{
    enum class ImageType : uint32_t
    {
        Texture2D = 0,
        Cubemap = 1,
        Texture2DArray = 2,

        // These must be last
        MaxImageTypes,
        None,
    };

    // Used for custom image file format
    struct ImageFileHeader
    {
        uint32_t width;
        uint32_t height;
        uint32_t numChannels;
        uint32_t numBytesPerChannel;
        uint32_t numLayers;
        uint32_t numMipLevels;
        ImageType type;
    };

    struct ImageInfo
    {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t depth = 1;
        uint32_t mipLevels = 1;
        VkFormat format = VK_FORMAT_UNDEFINED;
        uint32_t layers = 1;
    };

    struct MipInfo
    {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t depth = 1;
        uint32_t mipLevel = 0;
        std::unique_ptr<uint8_t> data;

        MipInfo() = default;
        ~MipInfo() = default;

        MipInfo(MipInfo &&other) = default;
        MipInfo &operator=(MipInfo &&other) = default;

        MipInfo(const MipInfo &) = delete;
        MipInfo &operator=(const MipInfo &) = delete;
    };

    class VulkanImage
    {
    public:
        friend class VulkanFontAtlas;

    private:
        inline static size_t s_memoryUsage = 0;

        VkImage m_image{VK_NULL_HANDLE};
        VkImageView m_imageView{VK_NULL_HANDLE};
        VmaAllocation m_allocation{VK_NULL_HANDLE};
        ImageInfo m_info{};

        static std::vector<MipInfo> ReadMipsFromFile(std::fstream &file, uint32_t width, uint32_t height, uint32_t startMip, uint32_t numMipLevels, uint32_t numChannels, uint32_t numBytesPerChannel);

    public:
        VulkanImage(VkImage image, VkImageView imageView, VmaAllocation allocation, const ImageInfo &info)
            : m_image(image), m_imageView(imageView), m_allocation(allocation), m_info(info)
        {
        }
        VulkanImage() = default;
        ~VulkanImage() = default;

        struct ImageCreateInfo
        {
            ImageInfo info = {};
            ImageType type = ImageType::Texture2D;
            VkImageCreateFlags createFlags = 0;
            VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
            VkImageUsageFlags additionalUsageFlags = 0;
        };

        static VulkanImage Create(const VulkanContext &context, const ImageCreateInfo &createInfo);

        struct ImageFromFileCreateInfo
        {
            ImageType type = ImageType::None;
            VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
            bool useAllMips = false;
            const std::string &filepath;
            ImageTransitionQueue *imageTransitionQueue = nullptr;
        };

        static VulkanImage CreateFromFile(const VulkanContext &context, VkCommandPool commandPool, const ImageFromFileCreateInfo &createInfo);

        void UploadData(const VulkanContext &context, VkCommandPool commandPool, ImageTransitionQueue *imageTransitionQueue, uint32_t bytesPerPixel, const std::vector<std::vector<MipInfo>> &layers);
        void TransitionLayout(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkImageLayout oldLayout, VkImageLayout newLayout);
        void TransitionLayout(VkDevice device, VkCommandBuffer commandBuffer, VkQueue queue, VkImageLayout oldLayout, VkImageLayout newLayout);

        void Destroy(const VulkanContext &context);

        const VkImage &GetImage() const { return m_image; }
        const VkImageView &GetImageView() const { return m_imageView; }

        const ImageInfo &GetInfo() const { return m_info; }

        static void SaveImageToFile(const VulkanContext &context, VkCommandPool commandPool, const VulkanImage &image, const std::string &filepath);
    
        static size_t GetMemoryUsage() { return s_memoryUsage; }
    };
}