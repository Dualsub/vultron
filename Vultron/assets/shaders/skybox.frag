#version 450

layout (set = 1, binding = 0) uniform samplerCube samplerCubeMap;

layout (location = 0) in vec3 inUVW;

layout (location = 0) out vec4 outFragColor;

vec3 Tonemap(vec3 x)
{
    const float L_white = 4.0;
    return (x * (1.0 + x / (L_white * L_white))) / (1.0 + x);
}

void main() 
{
	vec3 color = texture(samplerCubeMap, inUVW).rgb;
	
	float exposure = 0.8;
	float gamma = 2.2;

	color = Tonemap(color * exposure);
	color = color * (1.0f / Tonemap(vec3(11.2f)));	
	color = pow(color, vec3(1.0f / gamma));

	outFragColor = vec4(color, 1.0);
}