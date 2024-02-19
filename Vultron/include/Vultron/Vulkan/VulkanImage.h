#pragma once

#include "Vultron/Core/Core.h"

#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"

#include <string>

namespace Vultron
{

    struct ImageInfo
    {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t depth = 0;
        VkFormat format = VK_FORMAT_UNDEFINED;
    };

    class VulkanImage
    {
    public:
    private:
        VkImage m_image{VK_NULL_HANDLE};
        VkImageView m_imageView{VK_NULL_HANDLE};
        VmaAllocation m_allocation{VK_NULL_HANDLE};
        ImageInfo m_info{};

    public:
        VulkanImage(VkImage image, VkImageView imageView, VmaAllocation allocation, const ImageInfo &info)
            : m_image(image), m_imageView(imageView), m_allocation(allocation), m_info(info)
        {
        }
        VulkanImage() = default;
        ~VulkanImage() = default;

        struct ImageCreateInfo
        {
            VkDevice device = VK_NULL_HANDLE;
            VkCommandPool commandPool = VK_NULL_HANDLE;
            VkQueue queue = VK_NULL_HANDLE;
            VmaAllocator allocator = VK_NULL_HANDLE;
            void *data = nullptr;
            ImageInfo info = {};
            VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
            VkImageUsageFlags additionalUsageFlags = 0;
        };

        static VulkanImage Create(const ImageCreateInfo &createInfo);
        static Ptr<VulkanImage> CreatePtr(const ImageCreateInfo &createInfo);

        struct ImageFromFileCreateInfo
        {
            VkDevice device = VK_NULL_HANDLE;
            VkCommandPool commandPool = VK_NULL_HANDLE;
            VkQueue queue = VK_NULL_HANDLE;
            VmaAllocator allocator = VK_NULL_HANDLE;
            VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
            const std::string &filepath;
        };

        static VulkanImage CreateFromFile(const ImageFromFileCreateInfo &createInfo);
        static Ptr<VulkanImage> CreatePtrFromFile(const ImageFromFileCreateInfo &createInfo);

        void UploadData(VkDevice device, VmaAllocator allocator, VkCommandPool commandPool, VkQueue queue, void *data, size_t size);
        void TransitionLayout(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkImageLayout oldLayout, VkImageLayout newLayout);

        void Destroy(VkDevice device, VmaAllocator allocator);

        VkImage GetImage() const { return m_image; }
        VkImageView GetImageView() const { return m_imageView; }
    };
}