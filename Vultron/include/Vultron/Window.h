#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <stdio.h>
#include <vector>

namespace Vultron
{
    class Window
    {
    private:
        GLFWwindow *m_windowHandle = nullptr;

    public:
        Window() = default;
        ~Window() = default;

        bool Initialize();
        void SwapBuffers();
        void PollEvents();
        void Shutdown();
        bool ShouldShutdown();

        std::vector<const char *> GetWindowExtensions() const;
        GLFWwindow *GetWindowHandle() const { return m_windowHandle; }
        static void CreateVulkanSurface(const Window &window, VkInstance instance, VkSurfaceKHR *surface);
    };

}