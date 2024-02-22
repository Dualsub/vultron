#include "Vultron/Window.h"

namespace Vultron
{

    bool Window::Initialize()
    {
        // FOr now we do this.
        const uint32_t width = 1920;
        const uint32_t height = 1080;

        if (!glfwInit())
        {
            return false;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        m_width = width;
        m_height = height;

        m_windowHandle = glfwCreateWindow(width, height, "Vultron", NULL, NULL);

        if (m_windowHandle == nullptr)
        {
            glfwTerminate();
            return false;
        }

        return true;
    }

    void Window::SwapBuffers()
    {
        glfwSwapBuffers(m_windowHandle);
    }

    void Window::PollEvents()
    {
        glfwPollEvents();
    }

    void Window::Shutdown()
    {
        glfwDestroyWindow(m_windowHandle);
        glfwTerminate();
    }

    bool Window::ShouldShutdown()
    {
        return glfwWindowShouldClose(m_windowHandle);
    }

    std::vector<const char *> Window::GetWindowExtensions() const
    {
        uint32_t extensionCount = 0;
        const char **extentions = glfwGetRequiredInstanceExtensions(&extensionCount);

        std::vector<const char *> extensions(extentions, extentions + extensionCount);

        return extensions;
    }

    void Window::CreateVulkanSurface(const Window &window, VkInstance instance, VkSurfaceKHR *surface)
    {
        glfwCreateWindowSurface(instance, window.GetWindowHandle(), nullptr, surface);
    }

}