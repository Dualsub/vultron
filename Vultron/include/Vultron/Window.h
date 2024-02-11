#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdio.h>

namespace Vultron
{
    class Window
    {
    private:
        GLFWwindow* m_windowHandle = nullptr;
    public:
        Window() = default;
        ~Window() = default;

        bool Initialize();
        void SwapBuffers();
        void PollEvents();
        void Shutdown();
        bool ShouldShutdown();
    };

}