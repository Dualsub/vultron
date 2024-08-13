#pragma once

#include "Vultron/Core/Core.h"
#include "Vultron/Vulkan/VulkanContext.h"
#include "Vultron/Vulkan/VulkanBuffer.h"

#include <glm/glm.hpp>
#include "vulkan/vulkan.h"
#include "vk_mem_alloc.h"

#include <array>
#include <memory>
#include <string>

namespace Vultron
{
#pragma region StaticMesh

    struct VertexDescription
    {
        VkVertexInputBindingDescription bindingDescription;
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    };

    struct StaticMeshVertex
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 texCoord;

        static VkVertexInputBindingDescription GetBindingDescription()
        {
            VkVertexInputBindingDescription bindingDescription = {};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(StaticMeshVertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            return bindingDescription;
        }

        static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions()
        {
            std::vector<VkVertexInputAttributeDescription> attributeDescriptions(3);

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
            attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[2].offset = offsetof(StaticMeshVertex, texCoord);

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

    struct MeshDrawInfo
    {
        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkBuffer indexBuffer = VK_NULL_HANDLE;
        uint32_t indexCount = 0;
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
        VulkanMesh() = default;
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

        void Destroy(const VulkanContext &context);

        VkBuffer GetVertexBuffer() const { return m_vertexBuffer.GetBuffer(); }
        VkBuffer GetIndexBuffer() const { return m_IndexBuffer.GetBuffer(); }

        size_t GetIndexCount() const { return m_IndexBuffer.GetSize() / sizeof(uint32_t); }

        MeshDrawInfo GetDrawInfo() const
        {
            return {
                .vertexBuffer = m_vertexBuffer.GetBuffer(),
                .indexBuffer = m_IndexBuffer.GetBuffer(),
                .indexCount = static_cast<uint32_t>(GetIndexCount()),
            };
        }
    };

#pragma endregion

#pragma region SkeletalMesh

    struct SkeletalMeshVertex
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 texCoord;
        int32_t boneIDs[4];
        float boneWeights[4];

        static VkVertexInputBindingDescription GetBindingDescription()
        {
            VkVertexInputBindingDescription bindingDescription = {};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(SkeletalMeshVertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            return bindingDescription;
        }

        static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions()
        {
            std::vector<VkVertexInputAttributeDescription> attributeDescriptions(5);

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
            attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[2].offset = offsetof(StaticMeshVertex, texCoord);

            attributeDescriptions[3].binding = 0;
            attributeDescriptions[3].location = 3;
            attributeDescriptions[3].format = VK_FORMAT_R32G32B32A32_SINT;
            attributeDescriptions[3].offset = offsetof(SkeletalMeshVertex, boneIDs);

            attributeDescriptions[4].binding = 0;
            attributeDescriptions[4].location = 4;
            attributeDescriptions[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attributeDescriptions[4].offset = offsetof(SkeletalMeshVertex, boneWeights);

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

    struct SkeletonBone
    {
        int32_t id;
        int32_t parentID;
        glm::mat4 offset;
    };

    struct SkeletalBoneData
    {
        glm::mat4 offset = glm::mat4(1.0f);
        int32_t parentID = -1;
        float padding[3];
    };

    class VulkanSkeletalMesh
    {
    private:
        VulkanBuffer m_vertexBuffer;
        VulkanBuffer m_IndexBuffer;

        uint32_t m_boneOffset = 0;
        uint32_t m_boneCount = 0;

    public:
        VulkanSkeletalMesh(const VulkanBuffer &vertexBuffer, const VulkanBuffer &indexBuffer, uint32_t boneOffset, uint32_t boneCount)
            : m_vertexBuffer(vertexBuffer), m_IndexBuffer(indexBuffer), m_boneOffset(boneOffset), m_boneCount(boneCount)
        {
        }
        VulkanSkeletalMesh() = default;
        ~VulkanSkeletalMesh() = default;

        struct MeshCreateInfo
        {
            const std::vector<SkeletalMeshVertex> &vertices;
            const std::vector<uint32_t> &indices;
            uint32_t boneOffset;
            uint32_t boneCount;
        };

        static VulkanSkeletalMesh Create(const VulkanContext &context, VkCommandPool commandPool, const MeshCreateInfo &createInfo);

        struct MeshFromFilesCreateInfo
        {
            const std::string &filepath;
        };

        static VulkanSkeletalMesh CreateFromFile(const VulkanContext &context, VkCommandPool commandPool, std::vector<SkeletonBone> &boneBuffer, const MeshFromFilesCreateInfo &createInfo);
        void Destroy(const VulkanContext &context);

        inline VkBuffer GetVertexBuffer() const { return m_vertexBuffer.GetBuffer(); }
        inline VkBuffer GetIndexBuffer() const { return m_IndexBuffer.GetBuffer(); }
        inline size_t GetIndexCount() const { return m_IndexBuffer.GetSize() / sizeof(uint32_t); }
        inline uint32_t GetBoneOffset() const { return m_boneOffset; }
        inline uint32_t GetBoneCount() const { return m_boneCount; }

        MeshDrawInfo GetDrawInfo() const
        {
            return {
                .vertexBuffer = m_vertexBuffer.GetBuffer(),
                .indexBuffer = m_IndexBuffer.GetBuffer(),
                .indexCount = static_cast<uint32_t>(GetIndexCount()),
            };
        }
    };

#pragma endregion

#pragma region Line

    struct LineVertex
    {
        glm::vec3 position;
        glm::vec4 color;

        static VkVertexInputBindingDescription GetBindingDescription()
        {
            VkVertexInputBindingDescription bindingDescription = {};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(LineVertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            return bindingDescription;
        }

        static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions()
        {
            std::vector<VkVertexInputAttributeDescription> attributeDescriptions(2);

            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[0].offset = offsetof(LineVertex, position);

            attributeDescriptions[1].binding = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attributeDescriptions[1].offset = offsetof(LineVertex, color);

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

#pragma endregion
}