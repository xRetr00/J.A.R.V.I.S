#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float time;
    vec2 resolution;
    float timeScale;
    float zoom;
    float exposure;
} ubuf;

#define TAU 6.28318530718
#define iTime (ubuf.time * ubuf.timeScale)
#define iResolution vec3(ubuf.resolution, 1.0)

float rand(vec2 n)
{
    return fract(sin(dot(n, vec2(12.9898, 4.1414))) * 43758.5453);
}

float noise(vec2 p)
{
    vec2 ip = floor(p);
    vec2 u = fract(p);
    u = u * u * (3.0 - 2.0 * u);
    float res = mix(
        mix(rand(ip), rand(ip + vec2(1.0, 0.0)), u.x),
        mix(rand(ip + vec2(0.0, 1.0)), rand(ip + vec2(1.0, 1.0)), u.x),
        u.y
    );
    return res * res;
}

float fbm(vec2 p, int octaves)
{
    float s = 0.0;
    float m = 0.0;
    float a = 0.5;

    if (octaves >= 1) {
        s += a * noise(p);
        m += a;
        a *= 0.5;
        p *= 2.0;
    }

    if (octaves >= 2) {
        s += a * noise(p);
        m += a;
        a *= 0.5;
        p *= 2.0;
    }

    if (octaves >= 3) {
        s += a * noise(p);
        m += a;
        a *= 0.5;
        p *= 2.0;
    }

    if (octaves >= 4) {
        s += a * noise(p);
        m += a;
        a *= 0.5;
        p *= 2.0;
    }

    return s / max(m, 1e-5);
}

vec3 pal(float t, vec3 a, vec3 b, vec3 c, vec3 d)
{
    return a + b * cos(TAU * (c * t + d));
}

float luma(vec3 color)
{
    return dot(color, vec3(0.299, 0.587, 0.114));
}

void mainImage(out vec4 color, in vec2 fragCoord)
{
    float minRes = min(iResolution.x, iResolution.y);
    vec2 uv = (fragCoord * 2.0 - iResolution.xy) / minRes * 1.5;
    float t = iTime;

    float l = dot(uv, uv);
    color = vec4(0.0);
    if (l > 2.5) {
        return;
    }

    float sm = smoothstep(1.02, 0.98, l);
    float d = sm * l * l * l * 2.0;
    vec3 norm = normalize(vec3(uv.x, uv.y, 0.7 - d));
    float nx = fbm(uv * 2.0 + t * 0.4 + 25.69, 4);
    float ny = fbm(uv * 2.0 + t * 0.4 + 86.31, 4);
    float n = fbm(uv * 3.0 + 2.0 * vec2(nx, ny), 3);
    vec3 col = vec3(n * 0.5 + 0.25);
    float angle = atan(uv.y, uv.x) / TAU + t * 0.1;
    col *= pal(angle, vec3(0.3), vec3(0.5), vec3(1.0), vec3(0.0, 0.8, 0.8));
    col *= 2.0;

    vec3 chroma = abs(col);
    vec3 lit = col * d;
    lit += (lit * 0.5 + vec3(1.0) - luma(lit))
        * vec3(max(0.0, pow(dot(norm, vec3(0.0, 0.0, -1.0)), 5.0) * 3.0));

    float glowField = 1.5 * smoothstep(0.6, 1.0, fbm(norm.xy * 3.0 / (1.0 + norm.z), 2)) * d;
    lit += glowField;
    col = lit + col
        * pow(
            (1.0 - smoothstep(1.0, 0.98, l) - pow(max(0.0, length(uv) - 1.0), 0.2)) * 2.0,
            4.0
        );

    float f = fbm(normalize(uv) * 2.0 + t, 2) + 0.1;
    uv *= f + 0.1;
    uv *= 0.5;
    l = dot(uv, uv);

    vec3 ins = normalize(chroma + vec3(1e-4)) + 0.1;
    float ind = 0.2 + pow(smoothstep(0.0, 1.5, sqrt(l)) * 48.0, 0.25);
    ind *= ind * ind * ind;
    ind = 1.0 / max(ind, 1e-5);
    ins *= ind;
    col += ins * ins * sm * smoothstep(0.7, 1.0, ind);
    col += abs(norm) * (1.0 - d) * sm * 0.25;

    float alpha = clamp(max(max(col.r, col.g), col.b), 0.0, 1.0);
    alpha = max(alpha, sm * 0.4);
    color = vec4(max(col, vec3(0.0)), alpha);
}

void main()
{
    vec2 safeResolution = max(ubuf.resolution, vec2(1.0, 1.0));
    vec2 centeredUv = (qt_TexCoord0 - vec2(0.5)) * ubuf.zoom + vec2(0.5);
    vec2 fragCoord = centeredUv * safeResolution;

    vec4 color = vec4(0.0);
    mainImage(color, fragCoord);
    color.rgb = max(color.rgb * ubuf.exposure, vec3(0.0));
    color.a = clamp(color.a, 0.0, 1.0);
    fragColor = color * ubuf.qt_Opacity;
}
