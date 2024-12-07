#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTexCoord;
layout(location = 3) in vec4 inColor;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec4 fragLightSpacePos;
layout(location = 4) out vec4 fragColor;
layout(location = 5) out vec4 fragEmissiveColor;

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

struct ParticleInstanceData {
    vec4 positionAndLifeTime;
    vec3 lifeDurationAndNumFramesAndFrameRate;
    vec3 sizeAndRotation;
    vec3 velocity;
    vec3 acceleration;
    vec4 texCoordAndSize;
    vec4 startColor;
    vec4 endColor;
    vec4 scaleFadeInOutAndOpacityFadeInOut;
};

layout(std140, set = 0, binding = 1) readonly buffer ParticleInstanceInputBuffer  {
    uint instanceCount;
    ParticleInstanceData instances[];
};

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );

void main()
{
    vec4 worldPosition = vec4(inPosition, 1.0);
    vec4 viewPosition = ubo.view * worldPosition;
    
    vec4 position = ubo.proj * viewPosition;
    gl_Position = position / position.w;

    fragWorldPos = worldPosition.xyz;
    fragTexCoord = inTexCoord;
    fragNormal = inNormal;
    fragLightSpacePos = biasMat * ubo.lightSpaceMatrix * worldPosition;
    fragColor = vec4(0.0, 0.0, 0.0, 1.0);
    fragEmissiveColor = inColor;
}
