#version 460

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;


struct SpriteInstanceData {
    vec4 positionAndSize;
    vec4 texCoordAndFlip;
    vec4 color;
};

layout(std140, set = 0, binding = 0) readonly buffer InstanceBufferObject {
    SpriteInstanceData instances[];
};

void main()
{
    vec4 position = instances[gl_InstanceIndex].positionAndSize;
    vec4 texCoordAndFlip = instances[gl_InstanceIndex].texCoordAndFlip;

    vec2 texCoord = inTexCoord * texCoordAndFlip.zw + texCoordAndFlip.xy;

    gl_Position = vec4(inPosition * position.zw + position.xy, 0.0, 1.0);
    fragTexCoord = texCoord;
    fragColor = instances[gl_InstanceIndex].color;
}
