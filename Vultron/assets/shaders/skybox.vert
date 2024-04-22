#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormal;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 viewPos;
    vec3 lightDir;
    vec3 lightColor;
    mat4 lightSpaceMatrix;
} ubo;

layout (location = 0) out vec3 outUVW;

void main() 
{
	outUVW = inPos;
	// outUVW.xy *= -1.0;
	mat4 viewMat = mat4(mat3(ubo.view));
	vec4 pos = ubo.proj * viewMat * vec4(inPos.xyz, 1.0);
    gl_Position = pos.xyww;
}