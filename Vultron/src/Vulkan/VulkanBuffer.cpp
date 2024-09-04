#include "Vultron/Vulkan/VulkanBuffer.h"

#include "Vultron/Vulkan/VulkanUtils.h"

namespace Vultron
{
    Ptr<VulkanBuffer> VulkanBuffer::CreatePtr(const BufferCreateInfo &createInfo)
    {
        return MakePtr<VulkanBuffer>(Create(createInfo));
    }

    VulkanBuffer VulkanBuffer::Create(const BufferCreateInfo &createInfo)
    {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = createInfo.size;
        bufferInfo.usage = createInfo.usage;
        bufferInfo.sharingMode = createInfo.sharingMode;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = createInfo.allocationUsage;
        allocInfo.requiredFlags = createInfo.requiredFlags;

        VkBuffer buffer;
        VmaAllocation allocation;
        vmaCreateBuffer(createInfo.allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);

        s_memoryUsage += createInfo.size;

        return VulkanBuffer(buffer, allocation, createInfo.size);
    }

    void VulkanBuffer::Destroy(VmaAllocator allocator)
    {
        vmaDestroyBuffer(allocator, m_buffer, m_allocation);

        s_memoryUsage -= m_size;
    }
}
