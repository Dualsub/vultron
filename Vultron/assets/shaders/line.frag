#version 460

layout(location = 0) in vec4 fragColor;

layout(location = 0) out vec4 outColor;
layout(location = 1) out float outDepth;

void main() 
{
    outColor = fragColor;
    outDepth = 0.0;
}
