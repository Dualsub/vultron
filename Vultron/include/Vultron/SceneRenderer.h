#pragma once

#include "Vultron/Core/Singleton.h"

namespace Vultron
{

    class SceneRenderer
    {
    private:

    public:
        SceneRenderer() = default;
        ~SceneRenderer() = default;

        bool Initialize();

        void BeginFrame();
        void EndFrame();



        void Shutdown();
    };

}
