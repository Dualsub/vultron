#include "Vultron/Vulkan/VulkanBuffer.h"

namespace Vultron
{
    std::unique_ptr<VulkanBuffer> VulkanBuffer::Create(VmaAllocator allocator, VkBufferUsageFlags usage, size_t size)
    {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        VkBuffer buffer;
        VmaAllocation allocation;
        vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);

        return std::make_unique<VulkanBuffer>(buffer, allocation, size);
    }

    void VulkanBuffer::Destroy(VmaAllocator allocator)
    {
        vmaDestroyBuffer(allocator, m_buffer, m_allocation);
    }
}
