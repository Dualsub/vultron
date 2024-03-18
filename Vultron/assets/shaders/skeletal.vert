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

vec4 QMul(vec4 q1, vec4 q2) {
    return vec4(
        q1.w * q2.x + q1.x * q2.w + q1.y * q2.z - q1.z * q2.y,
        q1.w * q2.y - q1.x * q2.z + q1.y * q2.w + q1.z * q2.x,
        q1.w * q2.z + q1.x * q2.y - q1.y * q2.x + q1.z * q2.w,
        q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z
    );
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

vec4 slerp(vec4 q1, vec4 q2, float t) {
    // Compute the cosine of the angle between the two vectors.
    float cosTheta = dot(q1, q2);

    // If cosTheta < 0, the interpolation will take the long way around the sphere. 
    // To fix this, one quaternion is negated.
    if (cosTheta < 0.0) {
        q1 = -q1;
        cosTheta = -cosTheta;
    }

    // Perform a linear interpolation when cosTheta is close to 1 to avoid side effect of sin(angle) becoming a zero denominator
    if (cosTheta > 1.0 - 1E-6) {
        // Linear interpolation
        return mix(q1, q2, t);
    } else {
        // Essential Mathematics for SLERP
        float angle = acos(cosTheta);
        return (sin((1.0 - t) * angle) * q1 + sin(t * angle) * q2) / sin(angle);
    }
}

struct BonePose {
    vec3 position;
    vec4 rotation;
    vec3 scale;
};

// BonePose GetPose(int frame1, int frame2, float frameBlend, int boneIndex, int boneCount)
// {
//     mat4 boneMatrix = mat4(1.0);
//     int currBoneIndex = boneIndex;

//     // Accumulate the position, rotation, and scale of the bone
//     vec3 accPosition = vec3(0.0); // Initial position
//     vec4 accRotation = vec4(0.0, 0.0, 0.0, 1.0); // Initial rotation
//     vec3 accScale = vec3(1.0); // Initial scale

//     for (int i = 0; i < boneCount; i++)
//     {
//         int frame1Index = currBoneIndex + frame1;
//         int frame2Index = currBoneIndex + frame2;

//         vec4 rotation1 = normalize(frames[frame1Index].rotation);
//         vec4 rotation2 = normalize(frames[frame2Index].rotation);

//         if (dot(rotation1, rotation2) < 0.0) { rotation1 *= -1.0; }

//         vec3 position = mix(frames[frame1Index].position, frames[frame2Index].position, frameBlend);
//         vec4 rotation = normalize(slerp(rotation1, rotation2, frameBlend));
//         vec3 scale = mix(frames[frame1Index].scale, frames[frame2Index].scale, frameBlend);

//         // boneMatrix = ToMatrix(position, rotation, scale) * boneMatrix;
        
//         accRotation = normalize(QMul(rotation, accRotation));
//         accPosition = QMulV(rotation, accScale * accPosition) + position;
//         accScale = scale * accScale;

//         currBoneIndex = bones[currBoneIndex].parent;

//         if (currBoneIndex == -1)
//         {
//             break;
//         }
//     }

//     // return boneMatrix;
//     return BonePose(accPosition, accRotation, accScale);
// }

mat4 GetPose(int animationInstanceOffset, int animationInstanceCount, int boneIndex, int boneCount)
{
    mat4 boneMatrix = mat4(1.0);
    int currBoneIndex = boneIndex;

    for (int b = 0; b < boneCount; b++)
    { 
        float totalBlendFactor = 0.0;
        BonePose accPose = BonePose(vec3(0.0), vec4(0.0, 0.0, 0.0, 1.0), vec3(1.0));
        
        for (int i = 0; i < animationInstanceCount; i++) {
            int frameOffset = animationInstances[animationInstanceOffset + i].frameOffsetAnd1And2.x;
            int frame1 = animationInstances[animationInstanceOffset + i].frameOffsetAnd1And2.y;
            int frame2 = animationInstances[animationInstanceOffset + i].frameOffsetAnd1And2.z;
            float frameBlend = animationInstances[animationInstanceOffset + i].timeAndBlendFactor.x;
            float blendFactor = animationInstances[animationInstanceOffset + i].timeAndBlendFactor.y;

            // Blending between two frames
            int frame1Index = frameOffset + frame1 * boneCount + currBoneIndex;
            int frame2Index = frameOffset + frame2 * boneCount + currBoneIndex;

            vec4 rotation1 = normalize(frames[frame1Index].rotation);
            vec4 rotation2 = normalize(frames[frame2Index].rotation);

            if (dot(rotation1, rotation2) < 0.0) { rotation1 *= -1.0; }

            vec3 position = mix(frames[frame1Index].position, frames[frame2Index].position, frameBlend);
            vec4 rotation = normalize(slerp(rotation1, rotation2, frameBlend));
            vec3 scale = mix(frames[frame1Index].scale, frames[frame2Index].scale, frameBlend);

            // Blending between animations
            totalBlendFactor += blendFactor;
            float blend = blendFactor / totalBlendFactor;

            accPose.position = mix(accPose.position, position, blend);
            accPose.rotation = normalize(slerp(accPose.rotation, rotation, blend));
            accPose.scale = mix(accPose.scale, scale, blend);
        }

        boneMatrix = ToMatrix(accPose.position, accPose.rotation, accPose.scale) * boneMatrix;

        currBoneIndex = bones[currBoneIndex].parent;

        if (currBoneIndex == -1)
        {
            break;
        }
    }

    return boneMatrix;
}

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


    for (int j = 0; j < 4; j++) {
        if (inBoneIDs[j] == -1) {
            break;
        }

        mat4 offset = bones[boneOffset + inBoneIDs[j]].offset;
        mat4 boneTransform = GetPose(animationInstanceOffset, animationInstanceCount, inBoneIDs[j], boneCount);
        boneMatrix += (boneTransform * offset) * inWeights[j];
    }

    vec4 fragPos = instances[gl_InstanceIndex].model * boneMatrix * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * fragPos;
    fragWorldPos = vec3(fragPos);
    fragTexCoord = inTexCoord;
    fragNormal = inNormal;
    fragLightSpacePos = biasMat * ubo.lightSpaceMatrix * fragPos;
}