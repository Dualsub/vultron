#pragma once

#include "Vultron/Types.h"
#include "Vultron/Window.h"
#include "Vultron/Vulkan/VulkanRenderer.h"

#include <glm/glm.hpp>

#include <map>
#include <vector>
#include <iostream>

namespace Vultron
{
    /* Idea: Frontend renderer idea: Take in render jobs */

    struct RenderJob
    {
        RenderHandle mesh = {};
        RenderHandle material = {};
        std::vector<glm::mat4> transforms = {};

        // Compute hash of mesh and texture
        uint64_t GetHash() const
        {
            return static_cast<uint64_t>(mesh) ^ static_cast<uint64_t>(material);
        }
    };

    // Render batch but it has a vector of transforms
    struct InstancedRenderJob
    {
        RenderHandle mesh = {};
        RenderHandle material = {};
        std::vector<glm::mat4> transforms = {};
    };

    class SceneRenderer
    {
    private:
        VulkanRenderer backend;
        std::map<uint64_t, InstancedRenderJob> renderJobs;

    public:
        SceneRenderer() = default;
        ~SceneRenderer() = default;

        bool Initialize(const Window &window)
        {
            return backend.Initialize(window);
        }

        void BeginFrame()
        {
            renderJobs.clear();
        }

        void SubmitRenderJob(const RenderJob &job)
        {
            uint64_t hash = job.GetHash();
            if (renderJobs.find(hash) == renderJobs.end())
            {
                renderJobs.insert({hash, InstancedRenderJob{job.mesh, job.material, {}}});
            }

            renderJobs[hash].transforms.insert(renderJobs[hash].transforms.end(), job.transforms.begin(), job.transforms.end());
        }

        void EndFrame()
        {
            std::vector<glm::mat4> instanceBuffer;
            std::vector<RenderBatch> batches;

            for (auto &job : renderJobs)
            {
                instanceBuffer.insert(instanceBuffer.end(), job.second.transforms.begin(), job.second.transforms.end());
                batches.push_back({job.second.mesh, job.second.material, static_cast<uint32_t>(instanceBuffer.size() - job.second.transforms.size()), static_cast<uint32_t>(job.second.transforms.size())});
            }

            backend.Draw(batches, instanceBuffer);
        }

        void Shutdown()
        {
            backend.Shutdown();
        }

        RenderHandle LoadMesh(const std::string &path)
        {
            return backend.LoadMesh(path);
        }

        RenderHandle LoadImage(const std::string &path)
        {
            return backend.LoadImage(path);
        }

        template <typename T>
        RenderHandle CreateMaterial(const T &materialCreateInfo)
        {
            return backend.CreateMaterial(materialCreateInfo);
        }
    };

}
