#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <stdio.h>

namespace Vultron
{
    class Window
    {
    private:

    public:
        Window() = default;
        ~Window() = default;

        bool Initialize() { return true; }
        int Run();
    };

    void error_callback(int error, const char *description)
    {
        fputs(description, stderr);
    }

    void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
    {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    int Window::Run()
    {
        GLFWwindow *window;

        glfwSetErrorCallback(error_callback);

        if (!glfwInit())
            return -1;

        // Create a windowed mode window and its OpenGL context
        window = glfwCreateWindow(640, 480, "Simple GLFW App without OpenGL", NULL, NULL);
        if (!window)
        {
            glfwTerminate();
            return -1;
        }

        // Set the key callback
        glfwSetKeyCallback(window, key_callback);

        // Loop until the user closes the window
        while (!glfwWindowShouldClose(window))
        {
            // Poll for and process events
            glfwPollEvents();
        }

        glfwDestroyWindow(window);
        glfwTerminate();
        return 0;
    }

}