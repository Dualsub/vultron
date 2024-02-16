#pragma once

#include "vulkan/vulkan.h"
#include "vk_mem_alloc.h"

#include <array>
#include <memory>

namespace Vultron
{
    class VulkanBuffer
    {
    private:
        VkBuffer m_buffer;
        VmaAllocation m_allocation;
        size_t m_size = 0;

    public:
        VulkanBuffer(VkBuffer buffer, VmaAllocation allocation, size_t size)
            : m_buffer(buffer), m_allocation(allocation), m_size(size)
        {
        }

        static std::unique_ptr<VulkanBuffer> Create(VmaAllocator allocator, VkBufferUsageFlags usage, size_t size);
        void Destroy(VmaAllocator allocator);

        template <typename T>
        void Write(VmaAllocator allocator, T *data, size_t size, size_t offset = 0)
        {
            void *mappedData;
            vmaMapMemory(allocator, m_allocation, &mappedData);
            std::memcpy(mappedData, data, size);
            vmaUnmapMemory(allocator, m_allocation);
        }

        size_t GetSize() { return m_size; }
        VkBuffer GetBuffer() { return m_buffer; }
        VmaAllocation GetAllocation() { return m_allocation; }
    };

}
