#version 460

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec4 fragClipCoord;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D tex;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

float screenPxRange() {
    vec2 unitRange = vec2(2.0) / vec2(textureSize(tex, 0));
    vec2 screenTexSize = vec2(1.0) / fwidth(fragTexCoord);
    return max(0.5 * dot(unitRange, screenTexSize), 1.0);
}

float GetOpacity(vec2 fragTexCoord) {
    vec3 msd = textureLod(tex, fragTexCoord, 0).rgb;
    float sd = median(msd.r, msd.g, msd.b);
    float screenPxDistance = screenPxRange() * (sd - 0.5);
    return clamp(screenPxDistance + 0.5, 0.0, 1.0);
}

void main() {
    float opacity = GetOpacity(fragTexCoord);
    vec4 text = vec4(fragColor.rgb, fragColor.a * opacity);

    vec2 shadowOffset = vec2(-0.001, -0.001);
    vec4 shadowColor = vec4(0.0, 0.0, 0.0, 0.0);
    float shadowOpacity = GetOpacity(clamp(fragTexCoord + shadowOffset, fragClipCoord.xy, fragClipCoord.zw));
    vec4 shadow = vec4(shadowColor.rgb, shadowColor.a * shadowOpacity);

    outColor = text;
}
