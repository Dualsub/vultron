#version 460

layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D tex;

void main() {

    vec4 texColor = texture(tex, fragTexCoord);

    if (texColor.a < 0.1) {
        discard;
    }

    outColor = texColor;
}
