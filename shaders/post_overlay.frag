#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform float uAlpha;
out vec4 o;

// Devolve o texel como esta (ja codificado em sRGB, vindo do GDI) com opacidade
// uAlpha; o blend do GL faz o crossfade sobre o quadro ja desenhado.
void main()
{
    o = vec4(texture(uTex, vUV).rgb, uAlpha);
}
