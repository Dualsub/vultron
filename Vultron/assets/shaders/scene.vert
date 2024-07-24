#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec4 fragLightSpacePos;
layout(location = 4) out vec4 fragColor;

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
    mat4 lightSpaceMatrix;
	PointLight pointLights[4];
} ubo;

struct InstanceData {
    mat4 model;
    vec4 texOffsetAndSize;
    vec4 color;
};

layout(std140, set = 0, binding = 1) readonly buffer InstanceBufferObject {
    InstanceData instances[];
};

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );

void main()  
{
    vec4 pos = instances[gl_InstanceIndex].model * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * pos;
    fragWorldPos = pos.xyz;
    fragTexCoord = vec3(inTexCoord * instances[gl_InstanceIndex].texOffsetAndSize.zw + instances[gl_InstanceIndex].texOffsetAndSize.xy, 0.0);
    fragNormal = normalize(mat3(transpose(inverse(instances[gl_InstanceIndex].model))) * inNormal);
    fragLightSpacePos = biasMat * ubo.lightSpaceMatrix * pos;
    fragColor = instances[gl_InstanceIndex].color;
}