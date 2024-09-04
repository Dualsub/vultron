#version 460

layout(push_constant) uniform PushConstants {
    uvec2 screenSize;
};


layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 3) in vec2 quadSize;
layout(location = 4) in vec4 borderRadius;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D tex;

const float edgeSoftness = 1.0;

float roundedBoxSDF(vec2 CenterPosition, vec2 Size, float Radius) {
    return length(max(abs(CenterPosition)-Size+Radius,0.0))-Radius;
}

float GetOpacity(vec2 fragTexCoord) {    
    vec2 quadScreenSize = quadSize * screenSize;
    float pixelSize = 1.0 / min(quadScreenSize.x, quadScreenSize.y);

    // Select border radius based on the corner
    float borderRadiusValue = 
    fragTexCoord.x < 0.5 ? 
    (fragTexCoord.y > 0.5 ? borderRadius.w : borderRadius.x) : 
    (fragTexCoord.y > 0.5 ? borderRadius.z : borderRadius.y);

    vec2 aspectRatio = (vec2(screenSize) / min(screenSize.x, screenSize.y)) * (quadSize / min(quadSize.x, quadSize.y));
    float sd = roundedBoxSDF((fragTexCoord - 0.5) * aspectRatio, vec2(0.5) * aspectRatio, borderRadiusValue);
    
    return 1.0 - smoothstep(0.0, edgeSoftness * 2.0 * pixelSize, sd);
}

void main() {
    float opacity = GetOpacity(fragTexCoord);

    vec4 texColor = texture(tex, fragTexCoord) * fragColor;
    texColor.a *= opacity;

    outColor = texColor;
}
