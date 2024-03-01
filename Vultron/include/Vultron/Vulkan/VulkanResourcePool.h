#pragma once

#include "Vultron/Types.h"
#include "Vultron/Vulkan/VulkanAnimation.h"
#include "Vultron/Vulkan/VulkanContext.h"
#include "Vultron/Vulkan/VulkanMesh.h"
#include "Vultron/Vulkan/VulkanImage.h"
#include "Vultron/Vulkan/VulkanMaterial.h"

#include <unordered_map>

namespace Vultron
{
    // TODO: Make generic
    class ResourcePool
    {
        std::unordered_map<RenderHandle, VulkanMesh> m_meshes;
        std::unordered_map<RenderHandle, VulkanSkeletalMesh> m_skeletalMeshes;
        std::unordered_map<RenderHandle, VulkanImage> m_images;
        std::unordered_map<RenderHandle, VulkanMaterialInstance> m_materialInstances;
        std::unordered_map<RenderHandle, VulkanAnimation> m_animations;

        RenderHandle m_handleCounter = 0;

    public:
        ResourcePool() = default;
        ~ResourcePool() = default;

        RenderHandle AddMesh(const VulkanMesh &mesh)
        {
            m_meshes.insert({m_handleCounter, mesh});
            return m_handleCounter++;
        }

        RenderHandle AddSkeletalMesh(const VulkanSkeletalMesh &skeletalMesh)
        {
            m_skeletalMeshes.insert({m_handleCounter, skeletalMesh});
            return m_handleCounter++;
        }

        RenderHandle AddImage(const VulkanImage &image)
        {
            m_images.insert({m_handleCounter, image});
            return m_handleCounter++;
        }

        RenderHandle AddMaterialInstance(const VulkanMaterialInstance &materialInstance)
        {
            m_materialInstances.insert({m_handleCounter, materialInstance});
            return m_handleCounter++;
        }

        RenderHandle AddAnimation(const VulkanAnimation &animation)
        {
            m_animations.insert({m_handleCounter, animation});
            return m_handleCounter++;
        }

        const VulkanMesh &GetMesh(RenderHandle id) const { return m_meshes.at(id); }
        const VulkanSkeletalMesh &GetSkeletalMesh(RenderHandle id) const { return m_skeletalMeshes.at(id); }
        const VulkanImage &GetImage(RenderHandle id) const { return m_images.at(id); }
        const VulkanMaterialInstance &GetMaterialInstance(RenderHandle id) const { return m_materialInstances.at(id); }
        const VulkanAnimation &GetAnimation(RenderHandle id) const { return m_animations.at(id); }

        void Destroy(const VulkanContext &context)
        {
            // Descritor sets are destroyed when the pool is destroyed so we don't need to destroy them here
            m_materialInstances.clear();

            for (auto &mesh : m_meshes)
            {
                mesh.second.Destroy(context);
            }

            m_meshes.clear();

            for (auto &skeletalMesh : m_skeletalMeshes)
            {
                skeletalMesh.second.Destroy(context);
            }

            m_skeletalMeshes.clear();

            for (auto &image : m_images)
            {
                image.second.Destroy(context);
            }

            m_images.clear();

            m_animations.clear();
        }
    };
}