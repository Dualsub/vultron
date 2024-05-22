#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec4 fragLightSpacePos;
layout(location = 4) out vec4 fragColor;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 viewPos;
    vec3 lightDir;
    vec3 lightColor;
    mat4 lightSpaceMatrix;
} ubo;

struct ParticleInstanceData {
    vec4 positionAndLifeTime;
    vec2 size;
    vec4 velocityAndGravityFactor;
    vec4 texCoordAndSize;
    vec4 color;
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
    ParticleInstanceData instance = instances[gl_InstanceIndex];

    // Calculate model matrix with translation and scaling
    mat4 translation = mat4(1.0);
    translation[3] = vec4(instance.positionAndLifeTime.xyz, 1.0);

    mat4 scale = mat4(1.0);
    scale[0][0] = instance.size.x;
    scale[1][1] = instance.size.y;
    scale[2][2] = 1.0;


    mat3 viewRotation = mat3(ubo.view);
    mat3 viewRotationInverse = viewRotation;

    mat4 rotation = mat4(1.0);
    rotation[0][0] = viewRotationInverse[0][0];
    rotation[0][1] = viewRotationInverse[1][0];
    rotation[0][2] = viewRotationInverse[2][0];

    rotation[1][0] = viewRotationInverse[0][1];
    rotation[1][1] = viewRotationInverse[1][1];
    rotation[1][2] = viewRotationInverse[2][1];

    rotation[2][0] = viewRotationInverse[0][2];
    rotation[2][1] = viewRotationInverse[1][2];
    rotation[2][2] = viewRotationInverse[2][2];

    mat4 model = translation * rotation * scale;

    // Calculate the position in world space
    vec4 worldPosition = model * vec4(inPosition, 1.0);

    // Apply the modified view matrix (without rotation)
    vec4 viewPosition = ubo.view * worldPosition;

    // Final position
    gl_Position = ubo.proj * viewPosition;

    fragWorldPos = worldPosition.xyz;
    fragTexCoord = inTexCoord * instance.texCoordAndSize.zw + instance.texCoordAndSize.xy;
    fragNormal = normalize(mat3(transpose(inverse(model))) * inNormal);
    fragLightSpacePos = biasMat * ubo.lightSpaceMatrix * worldPosition;
    fragColor = instance.color;
}