#version 450

layout(push_constant) uniform Push {
    mat4 mvp;
    vec4 sunDirAndDaylight; // xyz = sun dir, w = daylight
    vec4 skyColorTop;
    vec4 skyColorBottom;
    vec4 sunColor;
} push;

layout(location = 0) out vec3 fragDir;
layout(location = 1) out vec3 fragSunDir;
layout(location = 2) out float fragDaylight;
layout(location = 3) out vec3 fragSkyTop;
layout(location = 4) out vec3 fragSkyBottom;
layout(location = 5) out vec3 fragSunColor;

// Full-screen triangle
vec2 positions[3] = vec2[](
    vec2(-1.0, -1.0),
    vec2( 3.0, -1.0),
    vec2(-1.0,  3.0)
);

void main() {
    vec2 p = positions[gl_VertexIndex];
    gl_Position = vec4(p, 0.0, 1.0);
    // Direction from camera through this screen point (assume inverse VP applied CPU-side is overkill;
    // we just use screen xy as direction approximation with z=1)
    fragDir = normalize(vec3(p.x, -p.y, 1.0));
    fragSunDir = push.sunDirAndDaylight.xyz;
    fragDaylight = push.sunDirAndDaylight.w;
    fragSkyTop = push.skyColorTop.xyz;
    fragSkyBottom = push.skyColorBottom.xyz;
    fragSunColor = push.sunColor.xyz;
}
