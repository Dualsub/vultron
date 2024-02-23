add_subdirectory(third_party)

find_package(Vulkan REQUIRED)

add_library(Vultron STATIC
    src/SceneRenderer.cpp
    src/Window.cpp
    src/Vulkan/Debug.cpp
    src/Vulkan/VulkanUtils.cpp
    src/Vulkan/VulkanRenderer.cpp
    src/Vulkan/VulkanBuffer.cpp
    src/Vulkan/VulkanImage.cpp
    src/Vulkan/VulkanMesh.cpp
    src/Vulkan/VulkanInitializers.cpp
    src/Vulkan/VulkanShader.cpp
    src/Vulkan/VulkanContext.cpp
    src/Vulkan/VulkanSwapchain.cpp
    src/Vulkan/VulkanResourcePool.cpp
)

target_include_directories(Vultron PUBLIC include)

target_link_libraries(Vultron PUBLIC
    glfw
    glm
    Vulkan::Vulkan
    GPUOpen::VulkanMemoryAllocator
    stb_image
)

# Define a macro for the absolute path to the assets directory
target_compile_definitions(Vultron PUBLIC VLT_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")