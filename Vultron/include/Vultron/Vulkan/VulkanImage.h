#pragma once

#include "Vultron/Core/Core.h"

#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"

namespace Vultron
{

    enum class ImageFormat
    {
        NONE = 0,
        R8G8B8A8_SRGB = 1,
    };

    struct ImageInfo
    {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t depth = 0;
        ImageFormat format = ImageFormat::NONE;
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
        VulkanImage(const VulkanImage &other) = default;
        VulkanImage(VulkanImage &&other) = default;
        ~VulkanImage() = default;

        struct ImageCreateInfo
        {
            VkDevice device = VK_NULL_HANDLE;
            VkCommandPool commandPool = VK_NULL_HANDLE;
            VkQueue queue = VK_NULL_HANDLE;
            VmaAllocator allocator = VK_NULL_HANDLE;
            void *data = nullptr;
            ImageInfo info = {};
        };

        static VulkanImage Create(const ImageCreateInfo &createInfo);
        static Ptr<VulkanImage> CreatePtr(const ImageCreateInfo &createInfo);
        void Destroy(VkDevice device, VmaAllocator allocator);
    };
}