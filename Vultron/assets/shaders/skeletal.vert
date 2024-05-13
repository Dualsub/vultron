#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in ivec4 inBoneIDs;
layout(location = 4) in vec4 inWeights;

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

struct InstanceData {
    mat4 model;
    ivec4 boneAndInstanceOffsetAndCount;
    int boneOutputOffset;
};

layout(std140, set = 0, binding = 1) readonly buffer InstanceBufferObject {
    InstanceData instances[];
};

layout(std140, set = 0, binding = 6) readonly buffer BoneOutputBufferObject {
    mat4 boneOutput[];
}; 

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );


void main()  {

    mat4 boneMatrix = mat4(0.0);

    int boneOffset = instances[gl_InstanceIndex].boneAndInstanceOffsetAndCount.x;
    int boneCount = instances[gl_InstanceIndex].boneAndInstanceOffsetAndCount.y;
    int animationInstanceOffset = instances[gl_InstanceIndex].boneAndInstanceOffsetAndCount.z;
    int animationInstanceCount = instances[gl_InstanceIndex].boneAndInstanceOffsetAndCount.w;
    int boneOutputOffset = instances[gl_InstanceIndex].boneOutputOffset;

    for (int j = 0; j < 4; j++) {
        if (inBoneIDs[j] == -1) {
            break;
        }

        mat4 boneTransform = boneOutput[boneOutputOffset + inBoneIDs[j]];
        boneMatrix += boneTransform * inWeights[j];
    }

    vec4 fragPos = instances[gl_InstanceIndex].model * boneMatrix * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * fragPos;
    fragWorldPos = vec3(fragPos);
    fragTexCoord = inTexCoord;
    fragNormal = normalize(mat3(instances[gl_InstanceIndex].model * boneMatrix) * inNormal);
    fragLightSpacePos = biasMat * ubo.lightSpaceMatrix * fragPos;
    fragColor = vec4(1.0);
}