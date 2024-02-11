#include "Vultron/Window.h"

namespace Vultron {

    bool Window::Initialize()
    {
        if(!glfwInit())
        {
            return false;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        m_windowHandle = glfwCreateWindow(640, 480, "Simple GLFW App without OpenGL", NULL, NULL);
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

}