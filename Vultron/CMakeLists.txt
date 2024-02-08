add_subdirectory(third_party)

add_library(Vultron STATIC src/Vultron.cpp)

target_include_directories(Vultron PUBLIC include)