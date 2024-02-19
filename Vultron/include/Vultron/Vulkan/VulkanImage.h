#pragma once

#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"

namespace Vultron
{

    enum class ImageFormat
    {
        NONE = 0,
    };

    struct ImageInfo
    {
        uint32_t width;
        uint32_t height;
        uint32_t depth;
        ImageFormat format;
    };

    class VulkanImage
    {
    public:
    private:
        VkImage m_image{VK_NULL_HANDLE};
        VmaAllocation m_allocation{VK_NULL_HANDLE};
        ImageInfo m_info{};

    public:
        VulkanImage(VkImage image, VmaAllocation allocation, const ImageInfo &info)
            : m_image(image), m_allocation(allocation), m_info(info)
        {
        }
        ~VulkanImage() = default;

        static VulkanImage Create(VkDevice device, VkCommandPool commandPool, VkQueue queue, VmaAllocator allocator, void *data, const ImageInfo &info);
        void Destroy(VmaAllocator allocator);
    };
}