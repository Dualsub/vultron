#version 460

layout(push_constant) uniform PushConstants { uvec2 screenSize; };

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec4 fragClipCoord;
layout(location = 3) out vec2 quadSize;
layout(location = 4) out vec4 borderRadius;

struct SpriteInstanceData {
    vec4 positionAndSize;   // xy: center in screen space, zw: size
    vec4 texCoordAndSize;
    vec4 color;
    vec4 borderRadius;
    float rotation;
};

layout(std140, set = 0, binding = 0) readonly buffer InstanceBufferObject {
    SpriteInstanceData instances[];
};

void main()
{
    SpriteInstanceData d = instances[gl_InstanceIndex];

    vec2 pos  = d.positionAndSize.xy;
    vec2 size = d.positionAndSize.zw;

    vec2 tc = inTexCoord * d.texCoordAndSize.zw + d.texCoordAndSize.xy;

    float c = cos(d.rotation);
    float s = sin(d.rotation);
    mat2  R = mat2(c, s, -s, c); // column-major

    float aspect = float(screenSize.x) / float(screenSize.y);

    vec2 local = inPosition * size;

    local.x *= aspect;
    local    = R * local;
    local.x /= aspect;

    vec2 world = pos + local;

    gl_Position   = vec4(world, 0.0, 1.0);
    fragClipCoord = vec4(d.texCoordAndSize.xy, d.texCoordAndSize.xy + d.texCoordAndSize.zw);
    fragTexCoord  = tc;
    fragColor     = d.color;
    quadSize      = size;
    borderRadius  = d.borderRadius;
}
