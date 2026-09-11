#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform vec2  uTexel;
out vec4 o;

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

void main()
{
    vec3 rgbM  = texture(uTex, vUV).rgb;
    vec3 rgbNW = texture(uTex, vUV + vec2(-1.0, -1.0) * uTexel).rgb;
    vec3 rgbNE = texture(uTex, vUV + vec2( 1.0, -1.0) * uTexel).rgb;
    vec3 rgbSW = texture(uTex, vUV + vec2(-1.0,  1.0) * uTexel).rgb;
    vec3 rgbSE = texture(uTex, vUV + vec2( 1.0,  1.0) * uTexel).rgb;

    float lM  = luma(rgbM);
    float lNW = luma(rgbNW), lNE = luma(rgbNE);
    float lSW = luma(rgbSW), lSE = luma(rgbSE);

    vec2 dir;
    dir.x = -((lNW + lNE) - (lSW + lSE));
    dir.y =   (lNW + lSW) - (lNE + lSE);

    float dirReduce = max((lNW + lNE + lSW + lSE) * 0.03125, 1.0 / 128.0);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, -8.0, 8.0) * uTexel;

    vec3 rgbA = 0.5 * (
        texture(uTex, vUV + dir * (1.0 / 3.0 - 0.5)).rgb +
        texture(uTex, vUV + dir * (2.0 / 3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(uTex, vUV + dir * -0.5).rgb +
        texture(uTex, vUV + dir *  0.5).rgb);

    float lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE)));
    float lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));
    float lB   = luma(rgbB);

    o = vec4((lB < lMin || lB > lMax) ? rgbA : rgbB, 1.0);
}
