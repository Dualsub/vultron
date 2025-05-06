#version 460

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragDecalPos;
layout(location = 2) in vec4 fragTexRect;
layout(location = 3) in vec3 fragNormal;
layout(location = 4) in vec4 fragColor;
layout(location = 5) in flat int fragInstanceIndex;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outARM;

layout(set = 1, binding = 0) uniform sampler2DArray albedoMap;
layout(set = 1, binding = 1) uniform sampler2DArray normalMap;
layout(set = 1, binding = 2) uniform sampler2DArray armMap;
layout(set = 1, binding = 3) uniform sampler2DArray emissiveMap;

struct DecalInstanceData {
    mat4 model;
    mat4 worldToDecal;
    vec4 texOffsetAndSize;
    vec4 color;
};

layout(std140, set = 0, binding = 1) readonly buffer InstanceBufferObject {
    DecalInstanceData decals[];
};

void main() 
{
    // vec3 decalPos = (decals[fragInstanceIndex].worldToDecal * vec4(fragWorldPos.xyz, 1.0)).xyz + 0.5;
    vec3 decalPos = fragDecalPos;

    if (decalPos.x < 0.0 || decalPos.y < 0.0 || decalPos.z < 0.0 || decalPos.x > 1.0 || decalPos.y > 1.0 || decalPos.z > 1.0) {
        discard;
    }

    vec3 texCoord = vec3(decalPos.xy * fragTexRect.zw + fragTexRect.xy, 0.0);
    vec4 albedo = texture(albedoMap, texCoord) * fragColor;
    vec3 normal = texture(normalMap, texCoord).xyz;
    vec3 arm = texture(armMap, texCoord).rgb;

    // outAlbedo = albedo;
    float depth = fragWorldPos.z;
    float near = 0.1;
    float far = 3200.0;
    float linearDepth = (2.0 * near) / (far + near - depth * (far - near));
    vec3 depthColor = vec3(linearDepth);
    outAlbedo = vec4(fragWorldPos.zzz, 1.0);
    outNormal = vec4(normal, albedo.a);
    outARM = vec4(arm, albedo.a);
}
