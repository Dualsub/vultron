#pragma once

#include "Vultron/Core/Core.h"
#include "Vultron/Vulkan/VulkanUtils.h"

#include "vulkan/vulkan.h"
#include "vk_mem_alloc.h"

#include <array>
#include <cassert>
#include <memory>

namespace Vultron
{
    // TODO: Update to use new abstractions
    class VulkanBuffer
    {
    private:
        VkBuffer m_buffer{VK_NULL_HANDLE};
        VmaAllocation m_allocation{VK_NULL_HANDLE};
        size_t m_size = 0;
        void *mapped = nullptr;

    public:
        VulkanBuffer(VkBuffer buffer, VmaAllocation allocation, size_t size)
            : m_buffer(buffer), m_allocation(allocation), m_size(size)
        {
        }
        VulkanBuffer() = default;
        ~VulkanBuffer() = default;

        struct BufferCreateInfo
        {
            VkDevice device = VK_NULL_HANDLE;
            VmaAllocator allocator = VK_NULL_HANDLE;
            VkBufferUsageFlags usage = 0;
            size_t size = 0;
            VmaMemoryUsage allocationUsage = VMA_MEMORY_USAGE_AUTO;
        };
        static Ptr<VulkanBuffer> CreatePtr(const BufferCreateInfo &createInfo);
        static VulkanBuffer Create(const BufferCreateInfo &createInfo);
        void Destroy(VmaAllocator allocator);

        template <typename T>
        void Write(VmaAllocator allocator, T *data, size_t size, size_t offset = 0)
        {
            assert(mapped == nullptr && "Buffer is already mapped.");
            void *mappedData;
            vmaMapMemory(allocator, m_allocation, &mappedData);
            std::memcpy(mappedData, data, size);
            vmaUnmapMemory(allocator, m_allocation);
        }

        void Map(VmaAllocator allocator)
        {
            assert(mapped == nullptr && "Buffer is already mapped.");
            vmaMapMemory(allocator, m_allocation, &mapped);
        }

        void Unmap(VmaAllocator allocator)
        {
            vmaUnmapMemory(allocator, m_allocation);
        }

        template <typename T>
        void CopyData(T *data, size_t size, size_t offset = 0) const
        {
            assert(mapped != nullptr && "Buffer is not mapped.");
            std::memcpy(mapped, data, size);
        }

        template <typename T>
        void UploadStaged(VkDevice device, VkCommandPool commandPool, VkQueue queue, VmaAllocator allocator, T *data, size_t size, size_t offset = 0)
        {
            VulkanBuffer stagingBuffer = Create({.allocator = allocator, .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT, .size = size, .allocationUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});
            stagingBuffer.Write(allocator, data, size);

            VkUtil::CopyBuffer(device, commandPool, queue, stagingBuffer.GetBuffer(), m_buffer, size);

            vmaDestroyBuffer(allocator, stagingBuffer.GetBuffer(), stagingBuffer.GetAllocation());
        }

        template <typename T>
        T *GetMapped() const { return (T *)mapped; }
        size_t GetSize() const { return m_size; }
        VkBuffer GetBuffer() const { return m_buffer; }
        VmaAllocation GetAllocation() const { return m_allocation; }
    };

}
