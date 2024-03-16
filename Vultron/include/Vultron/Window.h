#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// Platform-specific preprocessor directives
#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
#define VK_USE_PLATFORM_MACOS_MVK
#define GLFW_EXPOSE_NATIVE_COCOA
// Additional macOS specific includes or definitions
#endif
#include <GLFW/glfw3native.h>

#include <vector>
#include <utility>
#include <stdio.h>
#include <string>

namespace Vultron
{
    class Window
    {
    private:
        GLFWwindow *m_windowHandle = nullptr;
        uint32_t m_width = 0;
        uint32_t m_height = 0;

    public:
        Window() = default;
        ~Window() = default;

        enum class WindowMode
        {
            Windowed,
            Fullscreen
        };

        struct WindowCreateInfo
        {
            const std::string &title;
            uint32_t width = 1600;
            uint32_t height = 900;
            WindowMode mode = WindowMode::Windowed;
        };

        bool Initialize(const WindowCreateInfo &createInfo);
        void SwapBuffers();
        void PollEvents();
        void Shutdown();
        bool ShouldShutdown();

        std::vector<const char *> GetWindowExtensions() const;
        std::pair<uint32_t, uint32_t> GetExtent() const
        {
            return std::make_pair(m_width, m_height);
        }

        GLFWwindow *GetWindowHandle() const
        {
            return m_windowHandle;
        }

        void SetTitle(const std::string &title)
        {
            glfwSetWindowTitle(m_windowHandle, title.c_str());
        }

        static void CreateVulkanSurface(const Window &window, VkInstance instance, VkSurfaceKHR *surface);
    };
}