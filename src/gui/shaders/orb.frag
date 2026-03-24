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
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < 5; ++i) {
        value += amplitude * noise(p);
        p *= 2.02;
        amplitude *= 0.5;
    }
    return value;
}

void main()
{
    vec2 uv = qt_TexCoord0 * 2.0 - 1.0;
    float radius = length(uv);
    float angle = atan(uv.y, uv.x);

    float listening = clamp(1.0 - abs(ubuf.mode - 1.0), 0.0, 1.0);
    float processing = clamp(1.0 - abs(ubuf.mode - 2.0), 0.0, 1.0);
    float speakingMode = clamp(1.0 - abs(ubuf.mode - 3.0), 0.0, 1.0);

    float swirl = sin(angle * (4.0 + processing * 2.0) + ubuf.time * (1.1 + processing * 1.8)) * 0.05;
    float listenWave = sin(angle * 7.0 - ubuf.time * 3.6) * (0.03 + listening * (0.08 + ubuf.level * 0.1));
    float speakWave = cos(angle * 5.0 + ubuf.time * 4.7) * (0.025 + speakingMode * (0.05 + ubuf.speaking * 0.08));
    float detail = (fbm(uv * (3.0 + ubuf.distortion * 4.0) + vec2(ubuf.time * 0.22, -ubuf.time * 0.18)) - 0.5)
        * (0.11 + ubuf.distortion * 0.16);

    float boundary = 0.72 + swirl + listenWave + speakWave + detail;
    float mask = 1.0 - smoothstep(boundary - 0.06, boundary + 0.01, radius);

    float innerGlow = 1.0 - smoothstep(0.02, 0.68, radius);
    float caustic = fbm(uv * 4.8 + vec2(ubuf.time * 0.35, ubuf.time * -0.28));
    float rim = smoothstep(boundary - 0.02, boundary - 0.16, radius);

    vec3 gradient = mix(ubuf.colorC.rgb, ubuf.colorB.rgb, clamp((uv.y + 1.0) * 0.5 + caustic * 0.12, 0.0, 1.0));
    gradient = mix(gradient, ubuf.colorA.rgb, innerGlow * (0.46 + ubuf.speaking * 0.24));

    float holographicBand = 0.5 + 0.5 * sin((uv.y - uv.x) * 7.0 + ubuf.time * (1.2 + processing * 1.8));
    gradient += ubuf.colorA.rgb * holographicBand * 0.07;
    gradient += ubuf.colorB.rgb * rim * (0.14 + processing * 0.1 + speakingMode * 0.06);

    float alpha = mask * (0.82 + innerGlow * 0.18 + rim * 0.12) * ubuf.qt_Opacity;
    fragColor = vec4(gradient, alpha);
}
