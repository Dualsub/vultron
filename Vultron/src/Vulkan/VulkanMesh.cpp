#include "Vultron/Vulkan/VulkanMesh.h"
#include "Vultron/Vulkan/VulkanUtils.h"

#include "vk_mem_alloc.h"

#include <vector>
#include <fstream>
#include <iostream>

namespace Vultron
{
    VulkanMesh VulkanMesh::Create(const MeshCreateInfo &createInfo)
    {
        const size_t verticesSize = sizeof(createInfo.vertices[0]) * createInfo.vertices.size();
        auto vertexBuffer = VulkanBuffer::Create({.allocator = createInfo.allocator, .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, .size = verticesSize});
        vertexBuffer.UploadStaged(createInfo.device, createInfo.commandPool, createInfo.queue, createInfo.allocator, createInfo.vertices.data(), verticesSize, VMA_MEMORY_USAGE_GPU_ONLY);

        const size_t indiciesSize = sizeof(createInfo.indices[0]) * createInfo.indices.size();
        auto indexBuffer = VulkanBuffer::Create({.allocator = createInfo.allocator, .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, .size = indiciesSize, .allocationUsage = VMA_MEMORY_USAGE_GPU_ONLY});
        indexBuffer.UploadStaged(createInfo.device, createInfo.commandPool, createInfo.queue, createInfo.allocator, createInfo.indices.data(), indiciesSize);

        return VulkanMesh(vertexBuffer, indexBuffer);
    }

    Ptr<VulkanMesh> VulkanMesh::CreatePtr(const MeshCreateInfo &createInfo)
    {
        return MakePtr<VulkanMesh>(Create(createInfo));
    }

    VulkanMesh VulkanMesh::CreateFromFile(const MeshFromFilesCreateInfo &createInfo)
    {
        std::vector<StaticMeshVertex> vertices;
        std::vector<uint32_t> indices;

        std::ifstream file(createInfo.filepath, std::ios::ate | std::ios::binary);

        assert(file.is_open() && "Failed to open file");

        file.seekg(0);

        // Read the file from the end to get the size
        uint32_t vertexCount = 0;
        file.read(reinterpret_cast<char *>(&vertexCount), sizeof(vertexCount));

        vertices.resize(vertexCount);
        file.read(reinterpret_cast<char *>(vertices.data()), vertices.size() * sizeof(StaticMeshVertex));

        uint32_t indexCount = 0;
        file.read(reinterpret_cast<char *>(&indexCount), sizeof(indexCount));

        indices.resize(indexCount);
        file.read(reinterpret_cast<char *>(indices.data()), indices.size() * sizeof(uint32_t));

        file.close();

        return VulkanMesh::Create({.device = createInfo.device, .commandPool = createInfo.commandPool, .queue = createInfo.queue, .allocator = createInfo.allocator, .vertices = vertices, .indices = indices});
    }

    Ptr<VulkanMesh> VulkanMesh::CreatePtrFromFile(const MeshFromFilesCreateInfo &createInfo)
    {
        return MakePtr<VulkanMesh>(CreateFromFile(createInfo));
    }

    void VulkanMesh::Destroy(const VulkanContext &context)
    {
        m_vertexBuffer.Destroy(context.GetAllocator());
        m_IndexBuffer.Destroy(context.GetAllocator());
    }
}