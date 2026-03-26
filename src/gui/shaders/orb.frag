#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float time;
    float audioLevel;
    float speaking;
    float uiState;
    float quality;
    vec2 resolution;
} ubuf;

float hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float noise21(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);

    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));

    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float layeredNoise(vec2 p, float t)
{
    float n0 = noise21(p * 2.1 + vec2(t * 0.30, -t * 0.18));
    float n1 = noise21(p * 4.8 + vec2(-t * 0.55, t * 0.43));
    float n2 = noise21(p * 9.4 + vec2(t * 0.92, t * 0.67));
    return n0 * 0.52 + n1 * 0.31 + n2 * 0.17;
}

void main()
{
    vec2 safeResolution = max(ubuf.resolution, vec2(1.0));
    vec2 uv = qt_TexCoord0 * 2.0 - 1.0;
    uv.x *= safeResolution.x / safeResolution.y;

    float audioReactive = clamp(ubuf.audioLevel, 0.0, 1.0);
    float speakingReactive = clamp(ubuf.speaking, 0.0, 1.0);
    float stateReactive = clamp(ubuf.uiState / 3.0, 0.0, 1.0);

    float speed = mix(0.9, 2.8, stateReactive + speakingReactive * 0.4);
    float pulse = sin(ubuf.time * speed + audioReactive * 3.0);
    float pulseStrength = 0.04 + 0.05 * speakingReactive + 0.04 * stateReactive;

    vec2 distortedUv = uv;
    distortedUv += sin(distortedUv.yx * 4.0 + ubuf.time) * (0.02 + speakingReactive * 0.01) * (0.3 + audioReactive);
    distortedUv *= 1.0 + pulse * pulseStrength;

    float radius = length(distortedUv);
    float orbRadius = 0.78 + audioReactive * 0.03;
    float radialDensity = clamp(1.0 - radius / max(orbRadius, 0.001), 0.0, 1.0);
    float depthFalloff = radialDensity * radialDensity;

    int steps = 40;
    if (ubuf.quality < 0.5) {
        steps = 20;
    } else if (ubuf.quality > 1.5) {
        steps = 60;
    }

    vec3 marchColor = vec3(0.0);
    float marchAlpha = 0.0;
    float t = 0.0;
    vec3 rayOrigin = vec3(0.0, 0.0, 2.3);
    vec3 rayDir = normalize(vec3(distortedUv * 1.08, -1.9));

    for (int i = 0; i < 60; ++i) {
        if (i >= steps) {
            break;
        }

        vec3 pos = rayOrigin + rayDir * t;
        float shell = length(pos.xy);
        float sphereMask = smoothstep(orbRadius + 0.14, orbRadius - 0.20, shell);

        float n = layeredNoise(pos.xy + pos.z * 0.7, ubuf.time * (0.7 + stateReactive * 0.8));
        float n2 = layeredNoise(pos.yx * 1.4 + pos.z * 0.35, ubuf.time * (1.3 + speakingReactive * 0.9));
        float turbulence = mix(n, n2, 0.45);

        float density = sphereMask * (0.26 + turbulence * 0.95) * (0.4 + depthFalloff * 0.95);
        density *= 0.65 + 0.35 * (0.5 + 0.5 * pulse);

        vec3 baseColor = mix(vec3(0.10, 0.24, 0.55), vec3(0.35, 0.70, 1.0), turbulence);
        baseColor += vec3(0.08, 0.12, 0.22) * speakingReactive;

        float alphaStep = density * 0.042;
        marchColor += baseColor * alphaStep * (1.0 - marchAlpha);
        marchAlpha += alphaStep * (1.0 - marchAlpha);

        t += 0.055;
    }

    vec2 normalUv = distortedUv / max(orbRadius, 0.001);
    float z = sqrt(max(0.0, 1.0 - dot(normalUv, normalUv)));
    vec3 normal = normalize(vec3(normalUv, z));
    vec3 lightDir = normalize(vec3(-0.34, -0.42, 0.84));
    float light = clamp(dot(normal, lightDir), 0.0, 1.0);
    float fresnel = pow(1.0 - clamp(normal.z, 0.0, 1.0), 2.2);

    float core = pow(radialDensity, 2.6);
    float aura = smoothstep(orbRadius + 0.30, orbRadius - 0.02, radius);
    float softMask = 1.0 - smoothstep(orbRadius - 0.05, orbRadius + 0.12, radius);

    float flicker = 0.93 + 0.07 * sin(ubuf.time * 17.0 + audioReactive * 8.0);
    vec3 colorShift = vec3(
        0.93 + 0.07 * sin(ubuf.time * 0.17),
        0.92 + 0.08 * sin(ubuf.time * 0.23 + 1.3),
        0.90 + 0.10 * sin(ubuf.time * 0.19 + 2.2)
    );

    vec3 coreColor = vec3(0.44, 0.80, 1.0) * core * (0.8 + 0.55 * speakingReactive);
    vec3 auraColor = vec3(0.18, 0.37, 0.85) * aura * (0.35 + 0.25 * stateReactive);
    vec3 lit = marchColor * (0.72 + light * 0.58) + coreColor + auraColor + vec3(0.50, 0.78, 1.0) * fresnel * 0.34;
    lit *= flicker * colorShift;

    float alpha = clamp(marchAlpha * 1.28 + core * 0.55 + aura * 0.44 + fresnel * 0.15, 0.0, 1.0);
    alpha *= softMask;

    fragColor = vec4(max(lit, vec3(0.0)), alpha * ubuf.qt_Opacity);
}
