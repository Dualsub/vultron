#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 viewPos;
    vec3 lightDir;
    vec3 lightColor;
    mat4 lightSpaceMatrix;
} ubo;

struct InstanceData {
    mat4 model;
    vec4 texOffsetAndSize;
    vec4 color;
};

layout(std140, set = 0, binding = 1) readonly buffer InstanceBufferObject {
    InstanceData instances[];
};

void main()  {
    gl_Position = ubo.lightSpaceMatrix * instances[gl_InstanceIndex].model * vec4(inPosition, 1.0);
}