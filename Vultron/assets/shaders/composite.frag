#version 460

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D tex;
layout(set = 0, binding = 1) uniform sampler2D bloomTex;

// Push Constants
layout(push_constant) uniform PushConstants {
    float exposure;
    float gamma;
    float bloomIntensity;
    float bloomThreshold;
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

void main() {

    vec4 texColor = texture(tex, fragTexCoord);
    vec4 bloomColor = texture(bloomTex, fragTexCoord) * pc.bloomIntensity;
    vec3 color = mix(texColor.rgb, bloomColor.rgb, pc.bloomThreshold);

    color = Tonemap(color * pc.exposure);
	color = color * (1.0f / Tonemap(vec3(11.2f)));	
	color = pow(color, vec3(1.0f / pc.gamma));

    outColor = vec4(color, 1.0);
}