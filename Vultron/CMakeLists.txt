add_subdirectory(third_party)

find_package(Vulkan REQUIRED)

add_library(Vultron STATIC
    src/SceneRenderer.cpp
    src/Window.cpp
)

target_include_directories(Vultron PUBLIC include)

target_link_libraries(Vultron PUBLIC
    glfw
    glm
    Vulkan::Vulkan
    GPUOpen::VulkanMemoryAllocator
)