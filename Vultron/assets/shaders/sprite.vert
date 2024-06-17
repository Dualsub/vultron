#version 460

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec4 fragClipCoord;


struct SpriteInstanceData {
    vec4 positionAndSize;
    vec4 texCoordAndSize;
    vec4 color;
    float rotation;
};

layout(std140, set = 0, binding = 0) readonly buffer InstanceBufferObject {
    SpriteInstanceData instances[];
};

void main()
{
    vec2 position = instances[gl_InstanceIndex].positionAndSize.xy;
    vec2 size = instances[gl_InstanceIndex].positionAndSize.zw;
    vec4 texCoordAndSize = instances[gl_InstanceIndex].texCoordAndSize;

    vec2 texCoord = inTexCoord * texCoordAndSize.zw + texCoordAndSize.xy;

    float c = cos(instances[gl_InstanceIndex].rotation);
    float s = sin(instances[gl_InstanceIndex].rotation);
    mat2 rotationMatrix = mat2(c, -s, s, c);

    gl_Position = vec4(inPosition * rotationMatrix * size + position, 0.0, 1.0);
    fragClipCoord = vec4(texCoordAndSize.xy, texCoordAndSize.xy + texCoordAndSize.zw);
    fragTexCoord = texCoord;
    fragColor = instances[gl_InstanceIndex].color;
}
