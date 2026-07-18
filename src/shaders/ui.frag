#version 450

layout(location = 0) in vec2 fragUV;
layout(set = 0, binding = 1) uniform sampler2D uiTex;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 c = texture(uiTex, fragUV);
    // Tint white pixels red for selected slot (handled by vertex color in real impl)
    outColor = c;
}
