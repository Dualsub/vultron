#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragNormal;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 lightDir;
    vec3 viewPos;
} ubo;

struct InstanceData {
    mat4 model;
};

layout(std140, set = 0, binding = 1) readonly buffer InstanceBufferObject {
    InstanceData instances[];
};

void main()  {
    gl_Position = ubo.proj * ubo.view * instances[gl_InstanceIndex].model * vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord;
    fragNormal = inNormal;
}