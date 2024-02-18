#include "Vultron/Vulkan/VulkanBuffer.h"
#include "Vultron/Vulkan/Utils.h"

namespace Vultron
{
    std::unique_ptr<VulkanBuffer> VulkanBuffer::CreatePtr(VmaAllocator allocator, VkBufferUsageFlags usage, size_t size, VmaMemoryUsage allocationUsage)
    {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = allocationUsage;

        VkBuffer buffer;
        VmaAllocation allocation;
        vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);

        return std::make_unique<VulkanBuffer>(buffer, allocation, size);
    }

    VulkanBuffer VulkanBuffer::Create(VmaAllocator allocator, VkBufferUsageFlags usage, size_t size, VmaMemoryUsage allocationUsage)
    {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = allocationUsage;

        VkBuffer buffer;
        VmaAllocation allocation;
        vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);

        return VulkanBuffer(buffer, allocation, size);
    }

    void VulkanBuffer::Destroy(VmaAllocator allocator)
    {
        vmaDestroyBuffer(allocator, m_buffer, m_allocation);
    }
}
