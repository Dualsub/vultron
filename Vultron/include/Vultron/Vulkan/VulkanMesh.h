#pragma once

#include "Vultron/Core/Core.h"
#include "Vultron/Vulkan/VulkanBuffer.h"

#include <glm/glm.hpp>
#include "vulkan/vulkan.h"
#include "vk_mem_alloc.h"

#include <array>
#include <memory>
#include <string>

namespace Vultron
{
    struct StaticMeshVertex
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoord;

        static VkVertexInputBindingDescription GetBindingDescription()
        {
            VkVertexInputBindingDescription bindingDescription = {};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(StaticMeshVertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            return bindingDescription;
        }

        static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions()
        {
            std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};

            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[0].offset = offsetof(StaticMeshVertex, position);

            attributeDescriptions[1].binding = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[1].offset = offsetof(StaticMeshVertex, normal);

            attributeDescriptions[2].binding = 0;
            attributeDescriptions[2].location = 2;
            attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
            attributeDescriptions[2].offset = offsetof(StaticMeshVertex, texCoord);

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

        struct MeshCreateInfo
        {
            VkDevice device{VK_NULL_HANDLE};
            VkCommandPool commandPool{VK_NULL_HANDLE};
            VkQueue queue{VK_NULL_HANDLE};
            VmaAllocator allocator{VK_NULL_HANDLE};
            const std::vector<StaticMeshVertex> &vertices;
            const std::vector<uint32_t> &indices;
        };

        static VulkanMesh Create(const MeshCreateInfo &createInfo);
        static Ptr<VulkanMesh> CreatePtr(const MeshCreateInfo &createInfo);

        struct MeshFromFilesCreateInfo
        {
            VkDevice device{VK_NULL_HANDLE};
            VkCommandPool commandPool{VK_NULL_HANDLE};
            VkQueue queue{VK_NULL_HANDLE};
            VmaAllocator allocator{VK_NULL_HANDLE};
            const std::string &filepath;
        };

        static VulkanMesh CreateFromFile(const MeshFromFilesCreateInfo &createInfo);
        static Ptr<VulkanMesh> CreatePtrFromFile(const MeshFromFilesCreateInfo &createInfo);

        void Destroy(VmaAllocator allocator);

        VkBuffer GetVertexBuffer() const { return m_vertexBuffer.GetBuffer(); }
        VkBuffer GetIndexBuffer() const { return m_IndexBuffer.GetBuffer(); }

        size_t GetIndexCount() const { return m_IndexBuffer.GetSize() / sizeof(uint32_t); }
    };
}