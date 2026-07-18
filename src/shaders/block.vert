#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in float inTile;
layout(location = 3) in float inShade;

layout(push_constant) uniform Push {
    mat4 mvp;
    mat4 model;
    vec4 tintAndLight; // xyz = tint, w = daylight
    vec4 fogColorAndDist; // xyz = fog color, w = fog distance
} push;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out float fragShade;
layout(location = 2) out float fragTile;
layout(location = 3) out vec3  fragTint;
layout(location = 4) out float fragDaylight;
layout(location = 5) out vec3  fragFogColor;
layout(location = 6) out float fragFogDist;
layout(location = 7) out float fragViewDist;

void main() {
    gl_Position = push.mvp * vec4(inPosition, 1.0);

    fragUV = inUV;
    fragShade = inShade;
    fragTile = inTile;
    fragTint = push.tintAndLight.xyz;
    fragDaylight = push.tintAndLight.w;
    fragFogColor = push.fogColorAndDist.xyz;
    fragFogDist  = push.fogColorAndDist.w;
    // After perspective projection, gl_Position.w == -viewSpaceZ (positive in front of camera).
    // Use that as the fog distance.
    fragViewDist = gl_Position.w;
}
