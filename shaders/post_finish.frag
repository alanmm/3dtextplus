#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform int   uHasChroma;
uniform float uChromaStrength;   // 0..1 do usuario
uniform int   uHasVignette;
uniform float uVignetteAmount;   // 0..1
uniform int   uDebugBypass;      // 1 = visualizacao de debug: pula o tonemap ACES
out vec4 o;

vec3 aces(vec3 x)
{
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main()
{
    vec3 c;
    if (uHasChroma == 1) {
        // deslocamento radial em R/G/B: off = dir_do_centro * strength * r^2
        // (spec 8.1 passo 7). strength do usuario (0..1) escalado por 0.02
        // pra manter o deslocamento em fracao pequena de tela.
        vec2  d  = vUV - vec2(0.5);
        float r2 = dot(d, d);
        vec2  off = d * (uChromaStrength * 0.02) * r2;
        c.r = texture(uTex, vUV - off).r;
        c.g = texture(uTex, vUV).g;
        c.b = texture(uTex, vUV + off).b;
    } else {
        c = texture(uTex, vUV).rgb;
    }

    if (uHasVignette == 1) {
        vec2  d = vUV - vec2(0.5);
        float r = length(d) * 1.4142136;             // 0 no centro, ~1 no canto
        float v = 1.0 - uVignetteAmount * smoothstep(0.25, 1.0, r);
        c *= clamp(v, 0.0, 1.0);
    }

    if (uDebugBypass == 1) {
        // a curva ACES e' bem agressiva em tons medios (um valor linear
        // de 0.3 sai dela + gama proximo de 0.69) - inutiliza a leitura
        // de um valor escalar cru. So' gama 2.2, sem tonemap filmico.
        c = clamp(c, 0.0, 1.0);
    } else {
        c = aces(c);
    }
    c = pow(c, vec3(1.0 / 2.2));
    o = vec4(c, 1.0);
}
