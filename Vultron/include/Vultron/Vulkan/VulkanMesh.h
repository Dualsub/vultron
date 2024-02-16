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

    class VulkanMesh
    {
    private:
        std::unique_ptr<VulkanBuffer> m_vertexBuffer;
        std::unique_ptr<VulkanBuffer> m_IndexBuffer;

    public:
        VulkanMesh(std::unique_ptr<VulkanBuffer> vertexBuffer, std::unique_ptr<VulkanBuffer> indexBuffer)
            : m_vertexBuffer(std::move(vertexBuffer)), m_IndexBuffer(std::move(indexBuffer))
        {
        }
        ~VulkanMesh() = default;

        static std::unique_ptr<VulkanMesh> Create(VmaAllocator allocator, const std::vector<StaticMeshVertex> &vertices, const std::vector<uint32_t> &indices);
        void Destroy(VmaAllocator allocator);
    };
}