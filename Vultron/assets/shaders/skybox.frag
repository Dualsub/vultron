#version 450

layout (set = 1, binding = 0) uniform samplerCube samplerCubeMap;

layout (location = 0) in vec3 inUVW;

layout(location = 0) out vec4 outColor;
layout(location = 1) out float outDepth;

void main() 
{
	vec3 color = texture(samplerCubeMap, inUVW).rgb;
	outColor = vec4(color, 1.0);
	outDepth = 0.0;
}