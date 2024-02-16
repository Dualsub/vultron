#include "Vultron/Vulkan/Utils.h"
#include "Vultron/Vulkan/VulkanMesh.h"

#include "vk_mem_alloc.h"

namespace Vultron
{
    std::unique_ptr<VulkanMesh> VulkanMesh::Create(VmaAllocator allocator, const std::vector<StaticMeshVertex> &vertices, const std::vector<uint32_t> &indices)
    {
        const size_t verticesSize = sizeof(vertices[0]) * vertices.size();
        auto vertexBuffer = VulkanBuffer::Create(allocator, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, verticesSize);
        vertexBuffer->Write(allocator, vertices.data(), verticesSize);

        const size_t indiciesSize = sizeof(indices[0]) * indices.size();
        auto indexBuffer = VulkanBuffer::Create(allocator, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indiciesSize);
        vertexBuffer->Write(allocator, indices.data(), indiciesSize);

        return std::make_unique<VulkanMesh>(std::move(vertexBuffer), std::move(indexBuffer));
    }

    void VulkanMesh::Destroy(VmaAllocator allocator)
    {
        m_vertexBuffer->Destroy(allocator);
        m_IndexBuffer->Destroy(allocator);
    }
}