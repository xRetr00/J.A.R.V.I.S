#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float time;
    float level;
    float speaking;
    float mode;
    float distortion;
    vec2 resolution;
    vec4 colorA;
    vec4 colorB;
    vec4 colorC;
} ubuf;

const float PI = 3.14159265359;

mat2 rot(float a)
{
    float s = sin(a);
    float c = cos(a);
    return mat2(c, -s, s, c);
}

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);

    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

float fbm(vec2 p)
{
    float v = 0.0;
    float a = 0.55;
    for (int i = 0; i < 5; ++i) {
        v += a * noise(p);
        p = rot(0.85) * p * 1.92 + vec2(1.7, -1.3);
        a *= 0.52;
    }
    return v;
}

float plasmaBand(vec2 p, float t, float scale, float speed, float warp)
{
    vec2 q = p * scale;
    q += vec2(sin(q.y * 1.7 + t * speed), cos(q.x * 1.4 - t * speed * 1.13)) * warp;
    q += vec2(fbm(q + vec2(t * 0.13, -t * 0.11)), fbm(q - vec2(t * 0.09, t * 0.15)) - 0.5) * warp;

    float v = 0.0;
    v += 0.55 + 0.45 * sin(q.x * 2.2 + t * speed * 1.7);
    v += 0.55 + 0.45 * sin(q.y * 2.9 - t * speed * 1.2);
    v += 0.5 + 0.5 * sin((q.x + q.y) * 2.3 + t * speed * 0.9);
    v += 0.5 + 0.5 * sin(length(q) * 4.8 - t * speed * 1.4);
    v *= 0.25;

    return smoothstep(0.25, 0.92, v);
}

void main()
{
    vec2 uv = qt_TexCoord0 * 2.0 - 1.0;
    uv.x *= ubuf.resolution.x / max(ubuf.resolution.y, 1.0);

    float listening = clamp(1.0 - abs(ubuf.mode - 1.0), 0.0, 1.0);
    float processing = clamp(1.0 - abs(ubuf.mode - 2.0), 0.0, 1.0);
    float speakingMode = clamp(1.0 - abs(ubuf.mode - 3.0), 0.0, 1.0);

    float audioReactive = clamp(ubuf.level * 1.15 + ubuf.speaking * 0.95, 0.0, 1.0);
    float pulse = 1.0
        + 0.035 * sin(ubuf.time * (0.9 + speakingMode * 1.7))
        + 0.05 * audioReactive
        + 0.03 * speakingMode;

    vec2 p = uv / pulse;
    float r = length(p);
    float a = atan(p.y, p.x);

    float warp = 0.12 + ubuf.distortion * 0.42 + listening * 0.08 + audioReactive * 0.12;
    float edgeVibration = listening * (0.018 + ubuf.level * 0.05) * sin(a * 11.0 - ubuf.time * 5.0);
    float processWarp = processing * 0.028 * sin(a * 7.0 + ubuf.time * 3.8);
    float speakWarp = speakingMode * (0.02 + ubuf.speaking * 0.04) * cos(a * 6.0 - ubuf.time * 6.2);
    float softNoise = (fbm(p * 3.0 + vec2(ubuf.time * 0.15, -ubuf.time * 0.1)) - 0.5) * 0.08;

    float boundary = 0.72 + edgeVibration + processWarp + speakWarp + softNoise;

    vec2 p1 = rot(ubuf.time * (0.12 + processing * 0.28)) * p;
    vec2 p2 = rot(-ubuf.time * (0.19 + listening * 0.15)) * p;
    vec2 p3 = rot(ubuf.time * (0.24 + speakingMode * 0.24)) * p;

    float layerCore = plasmaBand(p1, ubuf.time, 3.4, 0.8 + processing * 0.7, warp);
    float layerMid = plasmaBand(p2, ubuf.time + 11.0, 4.9, 1.05 + listening * 0.65, warp * 1.2);
    float layerOuter = plasmaBand(p3, ubuf.time + 27.0, 2.6, 0.52 + speakingMode * 0.95, warp * 0.8);

    float density = layerCore * 0.5 + layerMid * 0.33 + layerOuter * 0.17;
    float innerField = smoothstep(0.0, 0.82, 1.0 - r / max(boundary, 0.001));
    float shellMask = 1.0 - smoothstep(boundary - 0.085, boundary + 0.018, r);
    float fresnel = pow(clamp(r / max(boundary, 0.001), 0.0, 1.0), 2.6);
    float rim = smoothstep(boundary - 0.11, boundary - 0.008, r);
    float glow = exp(-10.0 * max(r - boundary + 0.02, 0.0));

    vec3 coreTint = mix(ubuf.colorC.rgb, ubuf.colorB.rgb, clamp(layerOuter + innerField * 0.25, 0.0, 1.0));
    vec3 plasmaTint = mix(coreTint, ubuf.colorA.rgb, clamp(layerCore * 0.65 + layerMid * 0.35, 0.0, 1.0));
    vec3 edgeTint = mix(ubuf.colorB.rgb, ubuf.colorA.rgb, 0.55 + 0.45 * sin(a * 2.0 + ubuf.time * 0.8));

    vec3 color = plasmaTint;
    color += ubuf.colorA.rgb * innerField * (0.26 + audioReactive * 0.3);
    color += edgeTint * rim * (0.22 + processing * 0.14 + speakingMode * 0.12);
    color += mix(ubuf.colorA.rgb, ubuf.colorB.rgb, 0.5) * fresnel * 0.18;
    color += ubuf.colorA.rgb * glow * (0.14 + audioReactive * 0.16);
    color *= 0.72 + density * 0.62 + audioReactive * 0.18;

    float alphaCore = shellMask * (0.6 + innerField * 0.36 + density * 0.24);
    float alphaGlow = glow * (0.18 + ubuf.level * 0.16 + ubuf.speaking * 0.2);
    float alpha = (alphaCore + alphaGlow) * ubuf.qt_Opacity;

    fragColor = vec4(color, alpha);
}
