#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTexCoord;
layout(location = 3) in ivec4 inBoneIDs;
layout(location = 4) in vec4 inWeights;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec4 fragLightSpacePos;
layout(location = 4) out vec4 fragColor;
layout(location = 5) out vec4 fragEmissiveColor;
layout(location = 6) out ivec4 fragClosestProbes;
layout(location = 7) out vec4 fragProbeWeights;

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
    ivec4 boneAndInstanceOffsetAndCount;
    int boneOutputOffset;
    vec4 color;
    vec4 emissiveColor;
};

layout(std140, set = 0, binding = 1) readonly buffer InstanceBufferObject {
    InstanceData instances[];
};

layout(std140, set = 0, binding = 6) readonly buffer BoneOutputBufferObject {
    mat4 boneOutput[];
}; 

struct IrradianceVolume {
	vec3 volumeMin;
	vec3 volumeMax;
	uvec3 numCells;
};

layout(std430, set = 1, binding = 2) readonly buffer ProbeBuffer {
    IrradianceVolume irradianceVolume;
    vec3 probePositions[];
};

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );

void find4NearestProbes(in vec3 worldPos, 
                        out ivec4 outIndices, 
                        out vec4 outWeights)
{
    uint probeCount = probePositions.length();

    // We allocate some space for the distances and indices
    float distArr[64];
    int   idxArr[64];

    for (int i = 0; i < probeCount; i++) {
        distArr[i] = distance(worldPos, probePositions[i]);
        idxArr[i]  = i;
    }

    for (int i = 0; i < probeCount; i++) {
        for (int j = i + 1; j < probeCount; j++) {
            if (distArr[i] > distArr[j]) {
                // Swap distances
                float tmpDist = distArr[i];
                distArr[i] = distArr[j];
                distArr[j] = tmpDist;
                // Swap indices
                int tmpIdx = idxArr[i];
                idxArr[i] = idxArr[j];
                idxArr[j] = tmpIdx;
            }
        }
    }

    float d0 = distArr[0];
    float d1 = distArr[1];
    float d2 = distArr[2];
    float d3 = distArr[3];
    int i0 = idxArr[0];
    int i1 = idxArr[1];
    int i2 = idxArr[2];
    int i3 = idxArr[3];

    float w0 = 1.0 / (d0 + 0.0001);
    float w1 = 1.0 / (d1 + 0.0001);
    float w2 = 1.0 / (d2 + 0.0001);
    float w3 = 1.0 / (d3 + 0.0001);

    float sumW = w0 + w1 + w2 + w3 + 1e-8;
    w0 /= sumW;
    w1 /= sumW;
    w2 /= sumW;
    w3 /= sumW;

    outIndices = ivec4(i0, i1, i2, i3);
    outWeights = vec4(w0, w1, w2, w3);
}

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
    fragColor = instances[gl_InstanceIndex].color;
    fragEmissiveColor = instances[gl_InstanceIndex].emissiveColor;

    ivec4 closestProbes;
    vec4 probeWeights;

    find4NearestProbes(fragWorldPos, closestProbes, probeWeights);

    fragClosestProbes = closestProbes;
    fragProbeWeights = probeWeights;
}