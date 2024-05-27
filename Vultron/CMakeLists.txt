add_subdirectory(third_party)

find_package(Vulkan REQUIRED)

add_library(Vultron STATIC
    src/SceneRenderer.cpp
    src/Window.cpp
    src/Vulkan/Debug.cpp
    src/Vulkan/VulkanUtils.cpp
    src/Vulkan/VulkanAnimation.cpp
    src/Vulkan/VulkanRenderer.cpp
    src/Vulkan/VulkanBuffer.cpp
    src/Vulkan/VulkanImage.cpp
    src/Vulkan/VulkanMesh.cpp
    src/Vulkan/VulkanInitializers.cpp
    src/Vulkan/VulkanShader.cpp
    src/Vulkan/VulkanContext.cpp
    src/Vulkan/VulkanSwapchain.cpp
    src/Vulkan/VulkanMaterial.cpp
    src/Vulkan/VulkanRenderPass.cpp
    src/Vulkan/VulkanResourcePool.cpp
    src/Vulkan/VulkanSprite.cpp
    src/Vulkan/VulkanFontAtlas.cpp
    src/Vulkan/VulkanComputePipeline.cpp
)

target_include_directories(Vultron PUBLIC include)

target_link_libraries(Vultron PUBLIC
    glfw
    glm
    Vulkan::Vulkan
    GPUOpen::VulkanMemoryAllocator
    stb_image
)

# Set if not defined externally
if(NOT DEFINED VLT_ASSET_DIR)
    set(VLT_ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets") 
endif()

message(STATUS "VLT_ASSET_DIR: ${VLT_ASSET_DIR}")

target_compile_definitions(Vultron PUBLIC VLT_ASSETS_DIR="${VLT_ASSET_DIR}")