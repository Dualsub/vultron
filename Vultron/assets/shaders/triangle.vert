#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec4 fragLightSpacePos;

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
    gl_Position = ubo.proj * ubo.view * instances[gl_InstanceIndex].model * vec4(inPosition, 1.0);
    fragWorldPos = vec3(instances[gl_InstanceIndex].model * vec4(inPosition, 1.0));
    fragTexCoord = inTexCoord;
    fragNormal = inNormal;
    fragLightSpacePos = biasMat * ubo.lightSpaceMatrix * instances[gl_InstanceIndex].model * vec4(inPosition, 1.0);
}