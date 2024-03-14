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
    mat4 lightSpaceMatrix;
} ubo;

struct InstanceData {
    mat4 model;
    ivec4 boneAndInstanceOffsetAndCount;
};

layout(std140, set = 0, binding = 1) readonly buffer InstanceBufferObject {
    InstanceData instances[];
};

struct SkeletonBone {
    mat4 offset;
    int parent;
};

layout(set = 0, binding = 3) uniform SkeletonBoneObject {
    SkeletonBone bones[128];
};

struct AnimationFrame {
    vec3 position;
    vec4 rotation;
    vec3 scale;
};

layout(std140, set = 0, binding = 4) readonly buffer AnimationBufferObject {
    AnimationFrame frames[];
};

struct AnimationInstance {
    ivec3 frameOffsetAnd1And2;
    vec2 timeAndBlendFactor;
};

layout(set = 0, binding = 5) uniform AnimationInstanceObject {
    AnimationInstance animationInstances[128];
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

mat4 GetPose(int frame1, int frame2, float frameBlend, int boneIndex, int boneCount)
{
    mat4 boneMatrix = mat4(1.0);
    int currBoneIndex = boneIndex;

    for (int i = 0; i < boneCount; i++)
    {
        int frame1Index = currBoneIndex + frame1;
        int frame2Index = currBoneIndex + frame2;

        vec4 rotation1 = normalize(frames[frame1Index].rotation);
        vec4 rotation2 = normalize(frames[frame2Index].rotation);

        if (dot(rotation1, rotation2) < 0.0) { rotation1 *= -1.0; }

        vec3 position = mix(frames[frame1Index].position, frames[frame2Index].position, frameBlend);
        vec4 rotation = normalize(mix(rotation1, rotation2, frameBlend));
        vec3 scale = mix(frames[frame1Index].scale, frames[frame2Index].scale, frameBlend);

        boneMatrix = ToMatrix(position, rotation, scale) * boneMatrix;
        
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

    int boneOffset = instances[gl_InstanceIndex].boneAndInstanceOffsetAndCount.x;
    int boneCount = instances[gl_InstanceIndex].boneAndInstanceOffsetAndCount.y;
    int animationInstanceOffset = instances[gl_InstanceIndex].boneAndInstanceOffsetAndCount.z;
    int animationInstanceCount = instances[gl_InstanceIndex].boneAndInstanceOffsetAndCount.w;

    for (int i = 0; i < animationInstanceCount; i++) {
        int frameOffset = animationInstances[animationInstanceOffset + i].frameOffsetAnd1And2.x;
        int frame1 = animationInstances[animationInstanceOffset + i].frameOffsetAnd1And2.y;
        int frame2 = animationInstances[animationInstanceOffset + i].frameOffsetAnd1And2.z;
        float frameBlend = animationInstances[animationInstanceOffset + i].timeAndBlendFactor.x;
        float blendFactor = animationInstances[animationInstanceOffset + i].timeAndBlendFactor.y;

        for (int j = 0; j < 4; j++) {
            if (inBoneIDs[j] == -1) {
                break;
            }

            int frame1Index = frameOffset + frame1 * boneCount;
            int frame2Index = frameOffset + frame2 * boneCount;

            mat4 offset = bones[boneOffset + inBoneIDs[j]].offset;
            mat4 boneTransform = GetPose(frame1Index, frame2Index, frameBlend, inBoneIDs[j], boneCount);
            boneMatrix += (boneTransform * offset) * inWeights[j] * blendFactor;
        }
    }

    vec4 fragPos = instances[gl_InstanceIndex].model * boneMatrix * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * fragPos;
    fragWorldPos = vec3(fragPos);
    fragTexCoord = inTexCoord;
    fragNormal = inNormal;
}