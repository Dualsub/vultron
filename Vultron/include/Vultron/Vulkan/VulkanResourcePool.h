#pragma once

#include "Vultron/Types.h"
#include "Vultron/Vulkan/VulkanMesh.h"
#include "Vultron/Vulkan/VulkanImage.h"
#include "Vultron/Vulkan/VulkanContext.h"

#include <unordered_map>

namespace Vultron
{
    // TODO: Make generic
    class ResourcePool
    {
        std::unordered_map<RenderHandle, VulkanMesh> m_meshes;
        std::unordered_map<RenderHandle, VulkanImage> m_images;

        RenderHandle m_meshCounter = 0;
        RenderHandle m_imageCounter = 0;

    public:
        ResourcePool() = default;
        ~ResourcePool() = default;

        RenderHandle AddMesh(const VulkanMesh &mesh)
        {
            m_meshes.insert({m_meshCounter, mesh});
            return m_meshCounter++;
        }

        RenderHandle AddImage(const VulkanImage &image)
        {
            m_images.insert({m_imageCounter, image});
            return m_imageCounter++;
        }

        const VulkanMesh &GetMesh(RenderHandle id) const { return m_meshes.at(id); }
        const VulkanImage &GetImage(RenderHandle id) const { return m_images.at(id); }

        void Destroy(const VulkanContext &context)
        {
            for (auto &mesh : m_meshes)
            {
                mesh.second.Destroy(context);
            }

            m_meshes.clear();

            for (auto &image : m_images)
            {
                image.second.Destroy(context);
            }

            m_images.clear();
        }
    };
}