#version 450

layout(location = 0) in vec3 fragDir;
layout(location = 1) in vec3 fragSunDir;
layout(location = 2) in float fragDaylight;
layout(location = 3) in vec3 fragSkyTop;
layout(location = 4) in vec3 fragSkyBottom;
layout(location = 5) in vec3 fragSunColor;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 dir = normalize(fragDir);
    float t = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 sky = mix(fragSkyBottom, fragSkyTop, smoothstep(0.0, 0.6, t));

    // Sun disc
    float sunDot = max(dot(dir, normalize(fragSunDir)), 0.0);
    float disc = smoothstep(0.9985, 0.9995, sunDot);
    float glow = pow(sunDot, 32.0) * 0.4;
    sky += fragSunColor * (disc + glow) * fragDaylight;

    // Stars at night (cheap hash-based)
    if (fragDaylight < 0.3) {
        vec3 starDir = floor(dir * 200.0);
        float starHash = fract(sin(dot(starDir, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
        float star = step(0.998, starHash) * (1.0 - fragDaylight * 3.0);
        sky += vec3(star);
    }

    outColor = vec4(sky, 1.0);
}
