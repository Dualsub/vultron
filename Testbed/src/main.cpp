#include "Vultron/Vultron.h"
#include "Vultron/SceneRenderer.h"
#include "Vultron/Window.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <future>
#include <iostream>

struct Camera
{
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
    float pitch = 0.0f;
    float yaw = 0.0f;
} g_camera = {};

void CameraMovement(Camera &camera, float deltaTime, GLFWwindow *window)
{
    constexpr float c_cameraSpeed = 5.0f;

    const glm::quat rotation = glm::quat(glm::vec3(glm::radians(camera.pitch), glm::radians(camera.yaw), 0.0f));
    const glm::vec3 forward = rotation * glm::vec3(0.0f, 0.0f, -1.0f);
    const glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 right = glm::cross(forward, up);

    glm::vec3 direction = glm::vec3(0.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    {
        direction += forward;
    }

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    {
        direction -= forward;
    }

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    {
        direction -= right;
    }

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    {
        direction += right;
    }

    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
    {
        direction += up;
    }

    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
    {
        direction -= up;
    }

    if (glm::length(direction) > 0.0f)
    {
        direction = glm::normalize(direction);
        camera.position += direction * c_cameraSpeed * deltaTime;
    }
}

int main()
{
    Vultron::Window window;

    if (!window.Initialize({.title = "Vultron", .width = 1920, .height = 1080}))
    {
        std::cerr << "Window failed to initialize" << std::endl;
        return -1;
    }

    auto *windowHandle = window.GetWindowHandle();
    glfwSetInputMode(windowHandle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetInputMode(windowHandle, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    glfwSetKeyCallback(
        windowHandle,
        [](GLFWwindow *window, int key, int scancode, int action, int mods)
        {
            if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
        });

    glfwSetCursorPosCallback(
        windowHandle,
        [](GLFWwindow *window, double xpos, double ypos)
        {
            static bool firstMouse = true;
            static double lastX = 0.0;
            static double lastY = 0.0;

            if (firstMouse)
            {
                lastX = xpos;
                lastY = ypos;
                firstMouse = false;
            }

            double xoffset = lastX - xpos;
            double yoffset = lastY - ypos;

            lastX = xpos;
            lastY = ypos;

            constexpr float sensitivity = 0.1f;
            xoffset *= sensitivity;
            yoffset *= sensitivity;

            g_camera.yaw += static_cast<float>(xoffset);
            g_camera.pitch = glm::clamp(g_camera.pitch + static_cast<float>(yoffset), -89.9f, 89.9f);
        });

    Vultron::SceneRenderer renderer;

    if (!renderer.Initialize(window))
    {
        std::cerr << "Renderer failed to initialize" << std::endl;
        return -1;
    }

    const uint32_t numPerRow = 20;
    const float spacing = 2.5f;
    std::vector<glm::mat4> transforms;
    transforms.resize(1);

    std::atomic<bool> loaded = false;
    Vultron::RenderHandle mesh = renderer.LoadMesh(std::string(VLT_ASSETS_DIR) + "/meshes/DamagedHelmet.dat");
    Vultron::RenderHandle material = renderer.CreateMaterial<Vultron::PBRMaterial>(
        "helmet",
        {
            .albedo = renderer.LoadImage(std::string(VLT_ASSETS_DIR) + "/textures/helmet_albedo_test.dat"),
            .normal = renderer.LoadImage(std::string(VLT_ASSETS_DIR) + "/textures/helmet_normal_test.dat"),
            .metallicRoughnessAO = renderer.LoadImage(std::string(VLT_ASSETS_DIR) + "/textures/helmet_metRoughness_AO_test.dat"),
        });

    Vultron::RenderHandle skyboxTexture = renderer.LoadImage(std::string(VLT_ASSETS_DIR) + "/textures/skybox.dat");

    const auto loadFuture = std::async(
        std::launch::async,
        [&]()
        {
            const auto startTime = std::chrono::high_resolution_clock::now();
            const glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            for (uint32_t i = 0; i < transforms.size(); i++)
            {
                const float x = i * spacing;
                const float y = 0.0f;
                const glm::mat4 model = rot * glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f));
                transforms[i] = model;
            }
            loaded = true;

            const auto endTime = std::chrono::high_resolution_clock::now();

            std::cout << "Load time: " << std::chrono::duration<float, std::chrono::seconds::period>(endTime - startTime).count() << "s" << std::endl;
        });

    std::chrono::high_resolution_clock clock;
    auto lastTime = clock.now();
    auto startTime = lastTime;

    while (!window.ShouldShutdown())
    {
        auto currentTime = clock.now();
        float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
        lastTime = currentTime;
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        window.PollEvents();

        if (!loaded)
        {
            window.SwapBuffers();
            continue;
        }

        CameraMovement(g_camera, deltaTime, windowHandle);

        renderer.SetCamera({
            .position = g_camera.position,
            .rotation = glm::quat(glm::vec3(glm::radians(g_camera.pitch), glm::radians(g_camera.yaw), 0.0f)),
        });

        renderer.BeginFrame();

        for (uint32_t i = 0; i < transforms.size(); i++)
        {
            renderer.SubmitRenderJob(Vultron::StaticRenderJob{mesh, material, {transforms[i]}});
        }

        renderer.EndFrame();

        window.SwapBuffers();
    }

    renderer.Shutdown();
    window.Shutdown();

    return 0;
}