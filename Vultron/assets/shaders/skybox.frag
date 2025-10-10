#version 450

struct PointLight {
	vec4 positionAndRadius;
	vec4 color;
};

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 viewPos;
    vec3 lightDir;
    vec3 lightColor;
    mat4 lightSpaceMatrices[4];
    vec3 lightCascadeEnds;
	PointLight pointLights[4];
    float grayscaleAmount;
} ubo;
layout (set = 1, binding = 0) uniform samplerCube samplerCubeMap;

layout (location = 0) in vec3 inUVW;

layout(location = 0) out vec4 outColor;
layout(location = 1) out float outDepth;

void main() 
{
	vec3 color = texture(samplerCubeMap, inUVW).rgb;
	color = mix(color, vec3(dot(color, vec3(0.299, 0.587, 0.114))), ubo.grayscaleAmount);
	outColor = vec4(color, 1.0);
	outDepth = 0.0;
}