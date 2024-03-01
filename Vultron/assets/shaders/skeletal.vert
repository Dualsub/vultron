#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in ivec4 inBoneIDs;
layout(location = 4) in vec4 inWeights;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 viewPos;
    vec3 lightDir;
    vec3 lightColor;
} ubo;

struct InstanceData {
    mat4 model;
};

layout(std140, set = 0, binding = 1) readonly buffer InstanceBufferObject {
    InstanceData instances[];
};

struct SkeletonBone {
    mat4 offset;
    int parent;
};

layout(set = 0, binding = 2) uniform SkeletonBoneObject {
    SkeletonBone bones[1000];
};

struct AnimationFrame {
    vec3 position;
    vec4 rotation;
    vec3 scale;
};

layout(std140, set = 0, binding = 3) readonly buffer AnimationBufferObject {
    AnimationFrame frames[];
};

vec3 QMulV(vec4 q, vec3 v) {
    return q.xyz * 2.0f * dot(q.xyz, v) +
           v * (q.w * q.w - dot(q.xyz, q.xyz)) +
           cross(q.xyz, v) * 2.0f * q.w;
}

mat4 ToMatrix(vec3 position, vec4 rotation, vec3 scale) {
    vec3 xBasis = QMulV(rotation, vec3(scale.x, 0, 0));
    vec3 yBasis = QMulV(rotation, vec3(0, scale.y, 0));
    vec3 zBasis = QMulV(rotation, vec3(0, 0, scale.z));

    return mat4(
        xBasis.x, xBasis.y, xBasis.z, 0.0,
        yBasis.x, yBasis.y, yBasis.z, 0.0,
        zBasis.x, zBasis.y, zBasis.z, 0.0,
        position.x, position.y, position.z, 1.0
    );
}

mat4 GetPose(int frameOffset, int boneIndex)
{
    mat4 boneMatrix = mat4(1.0);
    int currBoneIndex = boneIndex;

    for (int i = 0; i < 100; i++)
    {
        int frameIndex = currBoneIndex + frameOffset;
        boneMatrix = ToMatrix(frames[frameIndex].position, frames[frameIndex].rotation.yzwx, frames[frameIndex].scale) * boneMatrix;
        currBoneIndex = bones[currBoneIndex].parent;

        if (currBoneIndex == -1)
        {
            break;
        }
    }

    return boneMatrix;
}

void main()  {

    mat4 boneMatrix = mat4(0.0);

    int frameOffset = 0;
    int skeletonOffset = 0;

    for (int i = 0; i < 4; i++) {
        if (inBoneIDs[i] == -1) {
            break;
        }

        int frameIndex = inBoneIDs[i] + frameOffset;

        mat4 offset = bones[inBoneIDs[i] + skeletonOffset].offset;
        mat4 boneTransform = GetPose(frameOffset, inBoneIDs[i]);
        boneMatrix += (boneTransform * offset) * inWeights[i];
    }

    vec4 fragPos = instances[gl_InstanceIndex].model * boneMatrix * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * fragPos;
    fragWorldPos = vec3(fragPos);
    fragTexCoord = inTexCoord;
    fragNormal = inNormal;
}