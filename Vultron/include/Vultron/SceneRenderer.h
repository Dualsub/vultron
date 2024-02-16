#pragma once

#include "Vultron/Window.h"
#include "Vultron/Vulkan/VulkanBackend.h"

namespace Vultron
{
    class SceneRenderer
    {
    private:
        VulkanBackend backend;

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
