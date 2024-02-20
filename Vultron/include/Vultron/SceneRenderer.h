#pragma once

#include "Vultron/Window.h"
#include "Vultron/Vulkan/VulkanRenderer.h"

namespace Vultron
{
    class SceneRenderer
    {
    private:
        VulkanRenderer backend;

    public:
        SceneRenderer() = default;
        ~SceneRenderer() = default;

        bool Initialize(const Window &window)
        {
            return backend.Initialize(window);
        }

        void BeginFrame()
        {
            backend.BeginFrame();
        }

        void EndFrame()
        {
            backend.EndFrame();
        }

        void Shutdown()
        {
            backend.Shutdown();
        }
    };

}
