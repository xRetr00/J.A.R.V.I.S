// Subtle diagonal shimmer for text rendered through Qt Quick layer.effect.

#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float time;
    float shimmerStrength;
    float shimmerSpeed;
    float shimmerWidth;
    float shimmerSkew;
};

layout(binding = 1) uniform sampler2D source;

void main()
{
    vec2 uv = qt_TexCoord0;
    vec4 base = texture(source, uv);

    float phase = fract(time * shimmerSpeed);
    float sweep = mix(-shimmerWidth, 1.0 + shimmerWidth, phase);
    float diagonalCoord = uv.x + (1.0 - uv.y) * shimmerSkew;
    float dist = abs(diagonalCoord - sweep);

    float band = 1.0 - smoothstep(0.0, shimmerWidth, dist);
    float highlight = pow(max(band, 0.0), 1.8) * shimmerStrength;

    vec3 shimmerTint = vec3(0.88, 0.95, 1.0) * highlight;
    vec3 finalRgb = min(base.rgb + shimmerTint * base.a, vec3(base.a));

    fragColor = vec4(finalRgb, base.a) * qt_Opacity;
}
