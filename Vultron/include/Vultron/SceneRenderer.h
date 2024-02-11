#pragma once

#include <vulkan/vulkan.h>

namespace Vultron
{

    class SceneRenderer
    {
    private:
        VkInstance m_instance;
    public:
        SceneRenderer() = default;
        ~SceneRenderer() = default;

        bool Initialize();

        void BeginFrame();
        void EndFrame();



        void Shutdown();
    };

}
