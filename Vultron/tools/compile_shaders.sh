# Compile all the shaders
for file in ../assets/shaders/*.{vert,frag}; do
    glslc $file -o $file.spv
    echo "Compiled $file"
done
