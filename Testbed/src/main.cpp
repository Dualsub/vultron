#include "Vultron/Vultron.h"
#include "Vultron/SceneRenderer.h"
#include "Vultron/Window.h"

#include <iostream>

int main()
{
    Vultron::Window window;

    if (!window.Initialize())
    {
        std::cerr << "Window failed to initialize" << std::endl;
        return -1;
    }

    Vultron::SceneRenderer renderer;

    if (!renderer.Initialize(window))
    {
        std::cerr << "Renderer failed to initialize" << std::endl;
        return -1;
    }

    while (!window.ShouldShutdown())
    {
        window.PollEvents();

        renderer.BeginFrame();

        /* Draw stuff here */

        renderer.EndFrame();

        // window.SwapBuffers();
    }

    renderer.Shutdown();
    window.Shutdown();

    return 0;
}