#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float time;
    vec2 resolution;
    float timeScale;
    float amplitude;
    float yScale;
    float exposure;
} ubuf;

#define iTime (ubuf.time * ubuf.timeScale)
#define iResolution vec3(ubuf.resolution, 1.0)

void mainImage(out vec4 fragColorLocal, in vec2 fragCoord)
{
    vec2 uv = fragCoord / iResolution.xy * 2.0 - 1.0;
    uv.x *= iResolution.x / iResolution.y;

    uv.x += sin(cos(uv.x * sin(uv.x / 2.0) - iTime) + iTime) * 5.0;

    vec3 uvRGB = vec3(uv.y, uv.y, uv.y);

    uvRGB.x += (cos(uv.x) * sin(iTime) + cos(uv.x + iTime * 2.0)) * 0.3;
    uvRGB.y += (cos(uv.x + 2.0) * sin(iTime - 2.0) + cos(uv.x + iTime * 2.0)) * 0.3;
    uvRGB.z += (cos(uv.x + 4.0) * sin(iTime - 4.0) + cos(uv.x + iTime * 2.0)) * 0.3;

    float a = smoothstep(0.0, abs(uvRGB.x), 0.08);
    float b = smoothstep(0.0, abs(uvRGB.y), 0.08);
    float c = smoothstep(0.0, abs(uvRGB.z), 0.08);

    vec3 aRGB = vec3(a) * vec3(0.973, 0.188, 0.384);
    vec3 bRGB = vec3(b) * vec3(0.110, 0.949, 0.475);
    vec3 cRGB = vec3(c) * vec3(0.067, 0.475, 0.910);

    fragColorLocal = vec4(aRGB + bRGB + cRGB, 1.0);
}

void main()
{
    vec2 safeResolution = max(ubuf.resolution, vec2(1.0, 1.0));
    vec2 centeredUv = qt_TexCoord0 - vec2(0.5);
    centeredUv.y *= ubuf.yScale;
    centeredUv.y += sin(centeredUv.x * 14.0 + ubuf.time * (2.4 + ubuf.timeScale)) * 0.018 * ubuf.amplitude;
    centeredUv.x += sin(centeredUv.y * 7.0 + ubuf.time * 1.5) * 0.012 * ubuf.amplitude;
    vec2 fragCoord = (centeredUv + vec2(0.5)) * safeResolution;

    vec4 color = vec4(0.0);
    mainImage(color, fragCoord);
    color.rgb = max(color.rgb * ubuf.exposure, vec3(0.0));

    float signal = max(max(color.r, color.g), color.b);
    float alpha = smoothstep(0.015, 0.24, signal);
    alpha *= 0.6 + ubuf.amplitude * 0.5;

    fragColor = vec4(color.rgb, clamp(alpha, 0.0, 1.0)) * ubuf.qt_Opacity;
}
