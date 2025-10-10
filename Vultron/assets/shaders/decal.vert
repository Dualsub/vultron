#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTexCoord;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec4 fragTexRect;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec4 fragColor;
layout(location = 4) out int fragInstanceIndex;

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

struct DecalInstanceData {
    mat4 model;
    mat4 invModel;
    vec4 texOffsetAndSize;
    vec4 color;
};

layout(std140, set = 0, binding = 1) readonly buffer InstanceBufferObject {
    DecalInstanceData decals[];
};

void main()  
{
    vec4 pos = decals[gl_InstanceIndex].model * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * pos;
    fragWorldPos = pos.xyz / pos.w;
    fragTexRect = decals[gl_InstanceIndex].texOffsetAndSize;
    fragNormal = normalize(mat3(transpose(decals[gl_InstanceIndex].invModel)) * inNormal);
    fragColor = decals[gl_InstanceIndex].color;
    fragInstanceIndex = gl_InstanceIndex;
}