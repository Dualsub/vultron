add_subdirectory(third_party)

add_library(Vultron STATIC
    src/SceneRenderer.cpp
)

target_include_directories(Vultron PUBLIC include)

target_link_libraries(Vultron PUBLIC
    glfw
)