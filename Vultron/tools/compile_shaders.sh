# Compile all the shaders for vulkan, targeting a vulkan environment
for file in ../assets/shaders/*.{vert,frag}; do
    glslc --target-env=vulkan $file -o $file.spv
    echo "Compiled $file"
done
