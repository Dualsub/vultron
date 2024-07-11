#pragma once

#include "Vultron/Types.h"
#include "Vultron/Vulkan/VulkanAnimation.h"
#include "Vultron/Vulkan/VulkanContext.h"
#include "Vultron/Vulkan/VulkanMesh.h"
#include "Vultron/Vulkan/VulkanImage.h"
#include "Vultron/Vulkan/VulkanMaterial.h"
#include "Vultron/Vulkan/VulkanFontAtlas.h"
#include "Vultron/Vulkan/VulkanEnvironmentMap.h"

#include <unordered_map>
#include <variant>
#include <set>

namespace Vultron
{
    // Font atlas is making the size of the resource repository 120 bytes vs 80 bytes without it.
    // We could remove the image from the font atlas and make it a separate image resource. Might be better.
    using VulkanResource = std::variant<VulkanMesh, VulkanSkeletalMesh, VulkanImage, VulkanMaterialInstance, VulkanAnimation, VulkanFontAtlas, VulkanEnvironmentMap>;

    class ResourceRepository
    {
    private:
        std::unordered_map<PoolHandle, std::set<RenderHandle>> m_poolResources;
        std::unordered_map<RenderHandle, VulkanResource> m_resources;

    public:
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

        ResourceRepository() = default;
        ~ResourceRepository() = default;

        RenderHandle AddResource(const std::string &name, PoolHandle poolHandle, const VulkanResource &resource)
        {
            RenderHandle handle = CreateHandle(name);
            assert(m_resources.find(handle) == m_resources.end() && "Resource already exists");
            assert(m_poolResources.find(poolHandle) != m_poolResources.end() && "Pool does not exist");
            m_resources.insert({handle, resource});
            m_poolResources[poolHandle].insert(handle);
            return handle;
        }

        template <typename T>
        const T &GetResource(RenderHandle id) const
        {
            assert(m_resources.find(id) != m_resources.end() && "Resource does not exist");
            assert(std::holds_alternative<T>(m_resources.at(id)) && "Invalid type");
            const T &resource = std::get<T>(m_resources.at(id));
            return resource;
        }

        PoolHandle CreatePool()
        {
            static PoolHandle poolCounter = 0;
            return poolCounter++;
        }

        void DestroyPool(PoolHandle poolHandle, const VulkanContext &context)
        {
            assert(m_poolResources.find(poolHandle) != m_poolResources.end() && "Pool does not exist");

            for (auto &resource : m_poolResources.at(poolHandle))
            {
                std::visit([&context](auto &&arg)
                           { arg.Destroy(context); }, m_resources.at(resource));
            }

            m_poolResources.erase(poolHandle);
        }

        template <typename T>
        MeshDrawInfo GetMeshDrawInfo(RenderHandle id) const
        {
            assert(false && "Invalid type");
            return {};
        }

        template <>
        MeshDrawInfo GetMeshDrawInfo<VulkanMesh>(RenderHandle id) const
        {
            return std::get<VulkanMesh>(m_resources.at(id)).GetDrawInfo();
        }

        template <>
        MeshDrawInfo GetMeshDrawInfo<VulkanSkeletalMesh>(RenderHandle id) const
        {
            return std::get<VulkanSkeletalMesh>(m_resources.at(id)).GetDrawInfo();
        }

        void Destroy(const VulkanContext &context)
        {
            for (auto &resource : m_resources)
            {
                std::visit([&context](auto &&arg)
                           { arg.Destroy(context); }, resource.second);
            }

            m_resources.clear();
        }
    };

    // TODO: Make generic
    class ResourcePool
    {
    private:
        std::unordered_map<RenderHandle, VulkanResource> m_resources;
        std::array<std::vector<RenderHandle>, 2> m_deletionQueue;

    public:
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

        ResourcePool() = default;
        ~ResourcePool() = default;

        RenderHandle AddResource(const std::string &name, const VulkanResource &resource)
        {
            RenderHandle handle = CreateHandle(name);
            assert(m_resources.find(handle) == m_resources.end() && "Resource already exists");
            m_resources.insert({handle, resource});
            return handle;
        }

        template <typename T>
        const T &GetResource(RenderHandle id) const
        {
            assert(m_resources.find(id) != m_resources.end() && "Resource does not exist");
            assert(std::holds_alternative<T>(m_resources.at(id)) && "Invalid type");
            const T &resource = std::get<T>(m_resources.at(id));
            return resource;
        }

        RenderHandle AddMesh(const std::string &name, const VulkanMesh &mesh)
        {
            return AddResource(name, mesh);
        }

        RenderHandle AddSkeletalMesh(const std::string &name, const VulkanSkeletalMesh &skeletalMesh)
        {
            return AddResource(name, skeletalMesh);
        }

        RenderHandle AddImage(const std::string &name, const VulkanImage &image)
        {
            return AddResource(name, image);
        }

        RenderHandle AddMaterialInstance(const std::string &name, const VulkanMaterialInstance &materialInstance)
        {
            return AddResource(name, materialInstance);
        }

        RenderHandle AddAnimation(const std::string &name, const VulkanAnimation &animation)
        {
            return AddResource(name, animation);
        }

        RenderHandle AddFontAtlas(const std::string &name, const VulkanFontAtlas &fontAtlas)
        {
            return AddResource(name, fontAtlas);
        }

        RenderHandle AddEnvironmentMap(const std::string &name, const VulkanEnvironmentMap &environmentMap)
        {
            return AddResource(name, environmentMap);
        }

        const VulkanMesh &GetMesh(RenderHandle id) const { return GetResource<VulkanMesh>(id); }
        const VulkanSkeletalMesh &GetSkeletalMesh(RenderHandle id) const { return GetResource<VulkanSkeletalMesh>(id); }
        const VulkanImage &GetImage(RenderHandle id) const { return GetResource<VulkanImage>(id); }
        const VulkanMaterialInstance &GetMaterialInstance(RenderHandle id) const { return GetResource<VulkanMaterialInstance>(id); }
        const VulkanAnimation &GetAnimation(RenderHandle id) const { return GetResource<VulkanAnimation>(id); }
        const VulkanFontAtlas &GetFontAtlas(RenderHandle id) const { return GetResource<VulkanFontAtlas>(id); }
        const VulkanEnvironmentMap &GetEnvironmentMap(RenderHandle id) const { return GetResource<VulkanEnvironmentMap>(id); }

        template <typename T>
        MeshDrawInfo GetMeshDrawInfo(RenderHandle id) const
        {
            assert(false && "Invalid type");
            return {};
        }

        template <>
        MeshDrawInfo GetMeshDrawInfo<VulkanMesh>(RenderHandle id) const
        {
            return GetMesh(id).GetDrawInfo();
        }

        template <>
        MeshDrawInfo GetMeshDrawInfo<VulkanSkeletalMesh>(RenderHandle id) const
        {
            return GetSkeletalMesh(id).GetDrawInfo();
        }

        void AddToDeletionQueue(RenderHandle id, uint32_t frameIndex)
        {
            m_deletionQueue[frameIndex].push_back(id);
        }

        void ProcessDeletionQueue(const VulkanContext &context, uint32_t frameIndex)
        {
            for (auto &id : m_deletionQueue[frameIndex])
            {
                Destroy(id, context);
            }

            m_deletionQueue[frameIndex].clear();
        }

        void Destroy(RenderHandle id, const VulkanContext &context)
        {
            assert(m_resources.find(id) != m_resources.end() && "Resource does not exist");
            std::visit([&context](auto &&arg)
                       { arg.Destroy(context); }, m_resources.at(id));
            m_resources.erase(id);
        }

        void Destroy(const VulkanContext &context)
        {
            for (auto &resource : m_resources)
            {
                std::visit([&context](auto &&arg)
                           { arg.Destroy(context); }, resource.second);
            }

            m_resources.clear();
        }
    };
}