#include "Vultron/Vulkan/VulkanUtils.h"
#include "Vultron/Vulkan/VulkanMesh.h"

#include "vk_mem_alloc.h"

namespace Vultron
{
    std::unique_ptr<VulkanMesh> VulkanMesh::Create(VkDevice device, VkCommandPool commandPool, VkQueue queue, VmaAllocator allocator, const std::vector<StaticMeshVertex> &vertices, const std::vector<uint32_t> &indices)
    {
        const size_t verticesSize = sizeof(vertices[0]) * vertices.size();
        auto vertexBuffer = VulkanBuffer::Create(allocator, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, verticesSize);
        vertexBuffer.UploadStaged(device, commandPool, queue, allocator, vertices.data(), verticesSize, VMA_MEMORY_USAGE_GPU_ONLY);

        const size_t indiciesSize = sizeof(indices[0]) * indices.size();
        auto indexBuffer = VulkanBuffer::Create(allocator, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, indiciesSize, VMA_MEMORY_USAGE_GPU_ONLY);
        indexBuffer.UploadStaged(device, commandPool, queue, allocator, indices.data(), indiciesSize);

        return std::make_unique<VulkanMesh>(vertexBuffer, indexBuffer);
    }

    void VulkanMesh::Destroy(VmaAllocator allocator)
    {
        m_vertexBuffer.Destroy(allocator);
        m_IndexBuffer.Destroy(allocator);
    }
}