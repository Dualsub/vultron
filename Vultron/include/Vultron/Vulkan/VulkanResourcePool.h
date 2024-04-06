#pragma once

#include "Vultron/Types.h"
#include "Vultron/Vulkan/VulkanAnimation.h"
#include "Vultron/Vulkan/VulkanContext.h"
#include "Vultron/Vulkan/VulkanMesh.h"
#include "Vultron/Vulkan/VulkanImage.h"
#include "Vultron/Vulkan/VulkanMaterial.h"
#include "Vultron/Vulkan/VulkanFontAtlas.h"

#include <unordered_map>

namespace Vultron
{
    constexpr RenderHandle c_invalidHandle = 0;

    // TODO: Make generic
    class ResourcePool
    {
    private:
        std::unordered_map<RenderHandle, VulkanMesh> m_meshes;
        std::unordered_map<RenderHandle, VulkanSkeletalMesh> m_skeletalMeshes;
        std::unordered_map<RenderHandle, VulkanImage> m_images;
        std::unordered_map<RenderHandle, VulkanMaterialInstance> m_materialInstances;
        std::unordered_map<RenderHandle, VulkanAnimation> m_animations;
        std::unordered_map<RenderHandle, VulkanFontAtlas> m_fontAtlases;

        static RenderHandle CreateHandle(const std::string &input)
        {
            uint32_t hash = 0x811c9dc5;
            uint32_t prime = 0x1000193;

            for (char c : input)
            {
                hash ^= c;
                hash *= prime;
            }

            return hash;
        }

    public:
        ResourcePool() = default;
        ~ResourcePool() = default;

        RenderHandle AddMesh(const std::string &name, const VulkanMesh &mesh)
        {
            RenderHandle handle = CreateHandle(name);
            m_meshes.insert({handle, mesh});
            return handle;
        }

        RenderHandle AddSkeletalMesh(const std::string &name, const VulkanSkeletalMesh &skeletalMesh)
        {
            RenderHandle handle = CreateHandle(name);
            m_skeletalMeshes.insert({handle, skeletalMesh});
            return handle;
        }

        RenderHandle AddImage(const std::string &name, const VulkanImage &image)
        {
            RenderHandle handle = CreateHandle(name);
            m_images.insert({handle, image});
            return handle;
        }

        RenderHandle AddMaterialInstance(const std::string &name, const VulkanMaterialInstance &materialInstance)
        {
            RenderHandle handle = CreateHandle(name);
            m_materialInstances.insert({handle, materialInstance});
            return handle;
        }

        RenderHandle AddAnimation(const std::string &name, const VulkanAnimation &animation)
        {
            RenderHandle handle = CreateHandle(name);
            m_animations.insert({handle, animation});
            return handle;
        }

        RenderHandle AddFontAtlas(const std::string &name, const VulkanFontAtlas &fontAtlas)
        {
            RenderHandle handle = CreateHandle(name);
            m_fontAtlases.insert({handle, fontAtlas});
            return handle;
        }

        const VulkanMesh &GetMesh(RenderHandle id) const { return m_meshes.at(id); }
        const VulkanSkeletalMesh &GetSkeletalMesh(RenderHandle id) const { return m_skeletalMeshes.at(id); }
        const VulkanImage &GetImage(RenderHandle id) const { return m_images.at(id); }
        const VulkanMaterialInstance &GetMaterialInstance(RenderHandle id) const { return m_materialInstances.at(id); }
        const VulkanAnimation &GetAnimation(RenderHandle id) const { return m_animations.at(id); }
        const VulkanFontAtlas &GetFontAtlas(RenderHandle id) const { return m_fontAtlases.at(id); }

        template <typename T>
        MeshDrawInfo GetMeshDrawInfo(RenderHandle id) const
        {
            assert(false && "Invalid type");
            return {};
        }

        template <>
        MeshDrawInfo GetMeshDrawInfo<VulkanMesh>(RenderHandle id) const
        {
            return m_meshes.at(id).GetDrawInfo();
        }

        template <>
        MeshDrawInfo GetMeshDrawInfo<VulkanSkeletalMesh>(RenderHandle id) const
        {
            return m_skeletalMeshes.at(id).GetDrawInfo();
        }

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

            for (auto &fontAtlas : m_fontAtlases)
            {
                fontAtlas.second.Destroy(context);
            }

            m_fontAtlases.clear();

            m_animations.clear();
        }
    };
}