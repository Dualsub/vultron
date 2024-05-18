@echo off
FOR %%G IN (../assets/shaders/*.vert, ../assets/shaders/*.frag, ../assets/shaders/*.comp) DO (
    glslc.exe --target-env=vulkan ../assets/shaders/%%G -o ../assets/shaders/%%G.spv
    echo Compiled ../assets/shaders/%%G
)