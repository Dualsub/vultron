#version 460

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D tex;
layout(set = 0, binding = 1) uniform sampler2D bloomTex;
layout(set = 0, binding = 2) uniform sampler2D depthTex;

// Push Constants
layout(push_constant) uniform PushConstants {
    float exposure;
    float gamma;
    float bloomIntensity;
    float bloomThreshold;
    float nearPlane;
    float farPlane;
} pc;

vec3 Reinhard(vec3 x) 
{
    const float L_white = 4.0;
    return (x * (1.0 + x / (L_white * L_white))) / (1.0 + x);
}

vec3 Aces(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 Tonemap(vec3 x)
{
    return Reinhard(x);
}

float LinearizeDepth(float depth, float near, float far) {
    return (2.0 * near) / (far + near - depth * (far - near));
}

float SobelEdgeDetection(sampler2D depthTex, vec2 uv, vec2 texelSize)
{
    mat3 kernelX = mat3(
        -1.0,  0.0,  1.0,
        -2.0,  0.0,  2.0,
        -1.0,  0.0,  1.0
    );

    mat3 kernelY = mat3(
        -1.0, -2.0, -1.0,
         0.0,  0.0,  0.0,
         1.0,  2.0,  1.0
    );

    float gradientX = 0.0;
    float gradientY = 0.0;

    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            vec2 offset = vec2(float(i), float(j)) * texelSize;
            float depth = texture(depthTex, uv + offset).r;
            // depth = LinearizeDepth(depth, pc.nearPlane, pc.farPlane);

            gradientX += depth * kernelX[i + 1][j + 1];
            gradientY += depth * kernelY[i + 1][j + 1];
        }
    }

    float edgeStrength = length(vec2(gradientX, gradientY));
    return smoothstep(0.1, 0.2, edgeStrength);
}

void main() {

    vec4 texColor = texture(tex, fragTexCoord);
    vec4 bloomColor = texture(bloomTex, fragTexCoord) * pc.bloomIntensity;
    vec3 color = mix(texColor.rgb, bloomColor.rgb, pc.bloomThreshold);

    color = Tonemap(color * pc.exposure);
    color = color * (1.0f / Tonemap(vec3(11.2f)));    
    color = pow(color, vec3(1.0f / pc.gamma));

    vec2 texelSize = 1.0 / vec2(textureSize(depthTex, 0)) * 0.25;
    float outline = SobelEdgeDetection(depthTex, fragTexCoord, texelSize);

    vec3 outlineColor = vec3(0.0);
    color = mix(color, outlineColor, outline);

    outColor = vec4(color, 1.0);
}
