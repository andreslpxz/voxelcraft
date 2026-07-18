#version 450

layout(location = 0) in vec2 inPosition;  // pixel coords (0..W, 0..H)
layout(location = 1) in vec2 inUV;

layout(push_constant) uniform Push {
    vec2 screenSize;
} push;

layout(location = 0) out vec2 fragUV;

void main() {
    // Convert pixel coords to NDC
    vec2 ndc = (inPosition / push.screenSize) * 2.0 - 1.0;
    ndc.y = -ndc.y;
    gl_Position = vec4(ndc, 0.0, 1.0);
    fragUV = inUV;
}
