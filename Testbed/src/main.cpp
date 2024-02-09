#include "Vultron/Vultron.h"
#include "Vultron/SceneRenderer.h"
#include "Vultron/Window.h"

int main()
{
    Vultron::Window window;
    return window.Run();

    Vultron::SceneRenderer renderer;

    if (!renderer.Initialize()) {
        return -1;
    }

    while (true)
    {
        renderer.BeginFrame();

        /* Draw stuff here */

        renderer.EndFrame();
    }

    renderer.Shutdown();

    return 0;
}