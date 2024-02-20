#version 460

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 lightDir;
    vec3 viewPos;
} ubo;
layout(binding = 2) uniform sampler2D texSampler;

void main() {
    vec3 normal = normalize(fragNormal);
    float intensity = max(dot(normal, -ubo.lightDir), 0.0);
    vec3 lightColor = vec3(1.0, 1.0, 1.0);
    vec3 ambient = 0.1 * lightColor;
    vec3 diffuse = intensity * lightColor;
    vec3 result = (ambient + diffuse) * texture(texSampler, fragTexCoord).rgb;
    outColor = vec4(result, 1.0);
}