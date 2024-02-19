#pragma once

#include "Vultron/Vulkan/VulkanBuffer.h"

#include <glm/glm.hpp>
#include "vulkan/vulkan.h"
#include "vk_mem_alloc.h"

#include <array>
#include <memory>

namespace Vultron
{
    struct StaticMeshVertex
    {
        glm::vec3 position;
        glm::vec3 color;

        static VkVertexInputBindingDescription GetBindingDescription()
        {
            VkVertexInputBindingDescription bindingDescription = {};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(StaticMeshVertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            return bindingDescription;
        }

        static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions()
        {
            std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions = {};

            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[0].offset = offsetof(StaticMeshVertex, position);

            attributeDescriptions[1].binding = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[1].offset = offsetof(StaticMeshVertex, color);

            return attributeDescriptions;
        }
    };

    // TODO: Combine vertex and index buffer into a single buffer
    class VulkanMesh
    {
    private:
        VulkanBuffer m_vertexBuffer;
        VulkanBuffer m_IndexBuffer;

    public:
        VulkanMesh(const VulkanBuffer &vertexBuffer, const VulkanBuffer &indexBuffer)
            : m_vertexBuffer(vertexBuffer), m_IndexBuffer(indexBuffer)
        {
        }
        ~VulkanMesh() = default;

        static std::unique_ptr<VulkanMesh> Create(VkDevice device, VkCommandPool commandPool, VkQueue queue, VmaAllocator allocator, const std::vector<StaticMeshVertex> &vertices, const std::vector<uint32_t> &indices);
        void Destroy(VmaAllocator allocator);

        VkBuffer GetVertexBuffer() const { return m_vertexBuffer.GetBuffer(); }
        VkBuffer GetIndexBuffer() const { return m_IndexBuffer.GetBuffer(); }

        size_t GetIndexCount() const { return m_IndexBuffer.GetSize() / sizeof(uint32_t); }
    };
}