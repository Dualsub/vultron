#include "Vultron/Vulkan/VulkanSprite.h"

#include <vector>

namespace Vultron
{
    VulkanQuadMesh VulkanQuadMesh::Create(const VulkanContext &context, VkCommandPool commandPool)
    {
        std::vector<SpriteVertex> vertices = {
            {{-0.5f, -0.5f}, {0.0f, 0.0f}},
            {{0.5f, -0.5f}, {1.0f, 0.0f}},
            {{0.5f, 0.5f}, {1.0f, 1.0f}},
            {{-0.5f, 0.5f}, {0.0f, 1.0f}},
        };

        std::vector<uint32_t> indices = {
            0,
            1,
            2,
            2,
            3,
            0,
        };

        const size_t verticesSize = sizeof(vertices[0]) * vertices.size();
        auto vertexBuffer = VulkanBuffer::Create({.allocator = context.GetAllocator(), .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, .size = verticesSize});
        vertexBuffer.UploadStaged(context.GetDevice(), commandPool, context.GetGraphicsQueue(), context.GetAllocator(), vertices.data(), verticesSize, VMA_MEMORY_USAGE_GPU_ONLY);

        const size_t indiciesSize = sizeof(indices[0]) * indices.size();
        auto indexBuffer = VulkanBuffer::Create({.allocator = context.GetAllocator(), .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, .size = indiciesSize, .allocationUsage = VMA_MEMORY_USAGE_GPU_ONLY});
        indexBuffer.UploadStaged(context.GetDevice(), commandPool, context.GetGraphicsQueue(), context.GetAllocator(), indices.data(), indiciesSize);

        return VulkanQuadMesh(vertexBuffer, indexBuffer);
    }
}