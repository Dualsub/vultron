#include "Vultron/Window.h"

namespace Vultron
{

    bool Window::Initialize()
    {
        if (!glfwInit())
        {
            return false;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        m_windowHandle = glfwCreateWindow(1600, 900, "Vultron", NULL, NULL);
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