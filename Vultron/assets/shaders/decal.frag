#version 460

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec4 fragTexRect;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec4 fragColor;
layout(location = 4) in flat int fragInstanceIndex;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outARM;

layout(set = 1, binding = 0) uniform sampler2DArray albedoMap;
layout(set = 1, binding = 1) uniform sampler2DArray normalMap;
layout(set = 1, binding = 2) uniform sampler2DArray armMap;
layout(set = 1, binding = 3) uniform sampler2DArray emissiveMap;

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
	vec4 lightCascadeSplits;
	PointLight pointLights[4];
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

layout(set = 0, binding = 2) uniform sampler2D depthMap;

vec4 reconstruct_pos(float z, vec2 uv_f)
{
    mat4 invProjView = inverse(ubo.proj * ubo.view);
    vec4 sPos = vec4(uv_f * 2.0 - 1.0, z, 1.0);
    sPos = invProjView * sPos;
    return vec4((sPos.xyz / sPos.w ), sPos.w);
}

void main() 
{
    vec2 res = vec2(textureSize(depthMap, 0));
    float aspect = res.x / res.y;
    vec2 screenPos = (gl_FragCoord.xy + 0.5) / res;

    float depth = texture(depthMap, screenPos).r;

    vec4 worldPos = reconstruct_pos(depth, screenPos);
    worldPos.w = 1.0;
    vec4 localPos = decals[fragInstanceIndex].invModel * worldPos;

    float dist = 0.5 - abs(localPos.y);
    float dist2 = 0.5 - abs(localPos.x);
    float dist3 = 0.5 - abs(localPos.z);

    if (dist < 0 || dist2 < 0 || dist3 < 0)
    {
        discard;
    }
    else
    {
        vec2 decalTexCoord = (localPos.xy + 0.5) * decals[fragInstanceIndex].texOffsetAndSize.zw + decals[fragInstanceIndex].texOffsetAndSize.xy;
        vec4 albedo = texture(albedoMap, vec3(decalTexCoord, 0));
        albedo *= fragColor;
        vec3 normal = texture(normalMap, vec3(decalTexCoord, 0)).xyz;
        vec3 arm = texture(armMap, vec3(decalTexCoord, 0)).rgb;

        outAlbedo = albedo;
        outNormal = vec4(normal, albedo.a);
        outARM = vec4(arm, albedo.a);
    }
}
