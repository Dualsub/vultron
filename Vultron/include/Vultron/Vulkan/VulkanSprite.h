#pragma once

#include <glm/glm.hpp>
#include "vulkan/vulkan.h"

#include "Vultron/Vulkan/VulkanMesh.h"

namespace Vultron
{
    struct SpriteVertex
    {
        glm::vec2 position;
        glm::vec2 texCoord;

        static VkVertexInputBindingDescription GetBindingDescription()
        {
            VkVertexInputBindingDescription bindingDescription = {};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(SpriteVertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            return bindingDescription;
        }

        static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions()
        {
            std::vector<VkVertexInputAttributeDescription> attributeDescriptions(2);

            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
            attributeDescriptions[0].offset = offsetof(SpriteVertex, position);

            attributeDescriptions[1].binding = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
            attributeDescriptions[1].offset = offsetof(SpriteVertex, texCoord);

            return attributeDescriptions;
        }

        static VertexDescription GetVertexDescription()
        {
            return {
                .bindingDescription = GetBindingDescription(),
                .attributeDescriptions = GetAttributeDescriptions(),
            };
        }
    };

    class VulkanQuadMesh
    {
    private:
        VulkanBuffer m_vertexBuffer;
        VulkanBuffer m_indexBuffer;

    public:
        VulkanQuadMesh(VulkanBuffer vertexBuffer, VulkanBuffer indexBuffer)
            : m_vertexBuffer(vertexBuffer), m_indexBuffer(indexBuffer) {}
        VulkanQuadMesh() = default;
        ~VulkanQuadMesh() = default;

        size_t GetIndexCount() const { return m_indexBuffer.GetSize() / sizeof(uint32_t); }

        MeshDrawInfo GetDrawInfo() const
        {
            return {
                .vertexBuffer = m_vertexBuffer.GetBuffer(),
                .indexBuffer = m_indexBuffer.GetBuffer(),
                .indexCount = static_cast<uint32_t>(GetIndexCount()),
            };
        }

        static VulkanQuadMesh Create(const VulkanContext &context, VkCommandPool commandPool);

        void Destroy(const VulkanContext &context)
        {
            m_vertexBuffer.Destroy(context.GetAllocator());
            m_indexBuffer.Destroy(context.GetAllocator());
        }
    };
}