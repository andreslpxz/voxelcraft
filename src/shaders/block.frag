#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 1) in float fragShade;
layout(location = 2) in float fragTile;
layout(location = 3) in vec3 fragTint;
layout(location = 4) in float fragDaylight;
layout(location = 5) in vec3 fragFogColor;
layout(location = 6) in float fragFogDist;
layout(location = 7) in float fragViewDist;

layout(set = 0, binding = 1) uniform sampler2D blockAtlas;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texColor = texture(blockAtlas, fragUV);

    // Daylight * face shade * tint
    float light = fragDaylight * fragShade;
    vec3 final = texColor.rgb * light * fragTint;

    // Apply fog (exponential)
    float fogFactor = clamp(exp(-fragViewDist / fragFogDist), 0.0, 1.0);
    final = mix(fragFogColor, final, fogFactor);

    // Discard nearly-transparent fragments (for leaves/grass cutouts)
    if (texColor.a < 0.1) discard;

    outColor = vec4(final, texColor.a);
}
