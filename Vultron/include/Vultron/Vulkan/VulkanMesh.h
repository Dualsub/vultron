#pragma once

#include "Vultron/Core/Core.h"
#include "Vultron/Vulkan/VulkanContext.h"
#include "Vultron/Vulkan/VulkanBuffer.h"
#include "Vultron/Vulkan/VulkanVertex.h"

#include <glm/glm.hpp>
#include "vulkan/vulkan.h"
#include "vk_mem_alloc.h"

#include <array>
#include <memory>
#include <string>

namespace Vultron
{
#pragma region StaticMesh

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
        glm::vec3 m_minBounds;
        glm::vec3 m_maxBounds;

    public:
        VulkanMesh(const VulkanBuffer &vertexBuffer, const VulkanBuffer &indexBuffer, const glm::vec3 &minBounds, const glm::vec3 &maxBounds)
            : m_vertexBuffer(vertexBuffer), m_IndexBuffer(indexBuffer), m_minBounds(minBounds), m_maxBounds(maxBounds)
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

        glm::vec3 GetMinBounds() const { return m_minBounds; }
        glm::vec3 GetMaxBounds() const { return m_maxBounds; }

        glm::vec3 GetCenter() const { return (m_minBounds + m_maxBounds) * 0.5f; }
        glm::vec3 GetSize() const { return m_maxBounds - m_minBounds; }
    };

#pragma endregion

#pragma region SkeletalMesh

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

}