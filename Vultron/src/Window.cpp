#include "Vultron/Window.h"

namespace Vultron
{

    bool Window::Initialize(const WindowCreateInfo &createInfo)
    {
        if (!glfwInit())
        {
            return false;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        m_width = createInfo.width;
        m_height = createInfo.height;

        m_windowHandle = glfwCreateWindow(createInfo.width, createInfo.height, createInfo.title.c_str(), NULL, NULL);

        if (m_windowHandle == nullptr)
        {
            glfwTerminate();
            return false;
        }

        // Disable cursor
        glfwSetInputMode(m_windowHandle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        return true;
    }

    void Window::SwapBuffers()
    {
        glfwSwapBuffers(m_windowHandle);
    }

    void Window::PollEvents()
    {
        glfwPollEvents();
        if (glfwGetKey(m_windowHandle, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(m_windowHandle, GLFW_TRUE);
        }
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