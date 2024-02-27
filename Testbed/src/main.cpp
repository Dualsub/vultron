#include "Vultron/Vultron.h"
#include "Vultron/SceneRenderer.h"
#include "Vultron/Window.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <future>
#include <iostream>

int main()
{
    Vultron::Window window;

    if (!window.Initialize())
    {
        std::cerr << "Window failed to initialize" << std::endl;
        return -1;
    }

    Vultron::SceneRenderer renderer;

    if (!renderer.Initialize(window))
    {
        std::cerr << "Renderer failed to initialize" << std::endl;
        return -1;
    }

    Vultron::RenderHandle mesh = renderer.LoadMesh(std::string(VLT_ASSETS_DIR) + "/meshes/DamagedHelmet.dat");
    Vultron::RenderHandle material = renderer.CreateMaterial<Vultron::PBRMaterial>({
        .albedo = renderer.LoadImage(std::string(VLT_ASSETS_DIR) + "/textures/helmet_albedo.dat"),
        .normal = renderer.LoadImage(std::string(VLT_ASSETS_DIR) + "/textures/helmet_normal.dat"),
        .metallicRoughnessAO = renderer.LoadImage(std::string(VLT_ASSETS_DIR) + "/textures/helmet_metRoughness_AO.dat"),
    });

    const uint32_t numPerRow = 20;
    const float spacing = 2.5f;
    std::vector<glm::mat4> transforms;
    transforms.resize(Vultron::c_maxInstances);

    const glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    for (uint32_t i = 0; i < transforms.size(); i++)
    {
        const float x = (i % numPerRow) * spacing - (numPerRow * spacing) / 2.0f;
        const float y = (i / numPerRow) * spacing * -1.0f;
        const glm::mat4 model = rot * glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f));
        transforms[i] = model;
    }

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

        renderer.BeginFrame();

        renderer.SubmitRenderJob({mesh, material, transforms});

        renderer.EndFrame();

        window.SwapBuffers();
    }

    renderer.Shutdown();
    window.Shutdown();

    return 0;
}