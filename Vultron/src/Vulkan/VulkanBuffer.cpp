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
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = createInfo.allocationUsage;

        VkBuffer buffer;
        VmaAllocation allocation;
        vmaCreateBuffer(createInfo.allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);

        return VulkanBuffer(buffer, allocation, createInfo.size);
    }

    void VulkanBuffer::Destroy(VmaAllocator allocator)
    {
        vmaDestroyBuffer(allocator, m_buffer, m_allocation);
    }
}
