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
        // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        if (createInfo.mode == WindowMode::Windowed)
        {
            m_width = createInfo.width;
            m_height = createInfo.height;

            m_windowHandle = glfwCreateWindow(createInfo.width, createInfo.height, createInfo.title.c_str(), NULL, NULL);
        }
        else if (createInfo.mode == WindowMode::Fullscreen)
        {
            const GLFWvidmode *mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
            m_width = mode->width;
            m_height = mode->height;

            m_windowHandle = glfwCreateWindow(m_width, m_height, createInfo.title.c_str(), glfwGetPrimaryMonitor(), NULL);
        }

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
        int width, height;
        glfwGetFramebufferSize(m_windowHandle, &width, &height);
        if (width != m_width || height != m_height)
        {
            m_width = static_cast<uint32_t>(width);
            m_height = static_cast<uint32_t>(height);
            m_resized = true;
        }
        else
        {
            m_resized = false;
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