#version 330 core
in vec2 vUV;
out vec4 fragColor;

uniform int   uType;        /* 0 solido, 1 gradiente, 2 imagem, 3 nebulosa, 4 grade */
uniform vec3  uColor1;
uniform vec3  uColor2;
uniform float uGradAngle;   /* radianos */
uniform sampler2D uBgTex;
uniform int   uHasBgTex;
uniform int   uBgFit;       /* 0 cobrir, 1 conter, 2 repetir */
uniform vec2  uUvScale;
uniform vec2  uUvOffset;
uniform float uPanSpeed;
uniform vec3  uNebColor1;
uniform vec3  uNebColor2;
uniform float uTime;
uniform vec3  uGridColor1;
uniform vec3  uGridColor2;
uniform float uAspect;     /* fb_w / fb_h - mantem as celulas da grade quadradas */
uniform float uGridDensity;  /* celulas na dimensao menor da tela */
uniform int   uGridDots;     /* 0 linhas, 1 pontos nos cruzamentos */

float hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float value_noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm(vec2 p)
{
    float sum = 0.0, amp = 0.5;
    for (int i = 0; i < 5; ++i) {
        sum += amp * value_noise(p);
        p *= 2.02;
        amp *= 0.5;
    }
    return sum;
}

vec3 gradient_bg(void)
{
    vec2 dir = vec2(cos(uGradAngle), sin(uGradAngle));
    float t = dot(vUV - 0.5, dir) + 0.5;
    /* dither ordenado 4x4 evita banding perceptivel apos o tonemap */
    float bayer[16] = float[16](
        0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0,
        3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0
    );
    ivec2 pc = ivec2(mod(gl_FragCoord.xy, 4.0));
    float dith = (bayer[pc.y * 4 + pc.x] / 16.0 - 0.5) / 255.0;
    return mix(uColor1, uColor2, clamp(t, 0.0, 1.0)) + vec3(dith);
}

vec3 image_bg(void)
{
    vec2 uv = vUV;
    if (uBgFit == 2) {
        uv.x += uPanSpeed * uTime;
        return texture(uBgTex, uv).rgb;
    }
    if (uBgFit == 0) {
        /* cobrir: uUvScale/uUvOffset recortam uma janela <= 1 da textura */
        uv = uv * uUvScale + uUvOffset;
        uv.x += uPanSpeed * uTime;
        return texture(uBgTex, uv).rgb;
    }
    /* conter: uUvScale/uUvOffset descrevem onde a imagem cabe na tela;
       fora dessa janela mostra a cor 1 (faixas) */
    uv = (uv - uUvOffset) / uUvScale;
    uv.x += uPanSpeed * uTime;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
        return uColor1;
    return texture(uBgTex, uv).rgb;
}

vec3 nebula_bg(void)
{
    vec2 p = vUV * 3.0;
    vec2 warp1 = vec2(fbm(p + uTime * 0.01), fbm(p + vec2(5.2, 1.3) + uTime * 0.01));
    vec2 warp2 = vec2(fbm(p + warp1 * 2.0), fbm(p + warp1 * 2.0 + vec2(8.3, 2.8)));
    float n = fbm(p + warp2 * 2.0);
    return mix(uNebColor1, uNebColor2, clamp(n, 0.0, 1.0));
}

/* grade estilo "papel milimetrado" - celulas quadradas (corrigidas pela
   razao de aspecto da tela) com linha fina anti-serrilhada via fwidth,
   independente de resolucao/zoom. Estilo "blueprint" (pedido do
   usuario): cor 1 = fundo, cor 2 = linhas. */
vec3 grid_bg(void)
{
    vec2 p = vec2(vUV.x * uAspect, vUV.y) * uGridDensity;
    vec2 f = fract(p - 0.5) - 0.5;
    float mask;
    if (uGridDots != 0) {
        float d = length(f) - 0.08;
        float pw = max(fwidth(d), 1e-6);
        mask = 1.0 - smoothstep(-pw, pw, d);
    } else {
        vec2 g = abs(f) / max(fwidth(p), vec2(1e-6));
        float line = min(g.x, g.y);
        mask = 1.0 - clamp(line, 0.0, 1.0);
    }
    return mix(uGridColor1, uGridColor2, mask);
}

void main()
{
    vec3 col;
    if (uType == 1) col = gradient_bg();
    else if (uType == 2 && uHasBgTex != 0) col = image_bg();
    else if (uType == 3) col = nebula_bg();
    else if (uType == 4) col = grid_bg();
    else col = uColor1;
    fragColor = vec4(col, 1.0);
}
