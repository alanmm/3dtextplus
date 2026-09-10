#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform vec2  uTexel;   // 1 / tamanho da textura de origem
uniform float uRadius;  // 0..1
out vec3 o;
void main()
{
    vec2 r = uTexel * (1.0 + uRadius * 2.0);
    vec3 s  = texture(uTex, vUV + r * vec2(-1, -1)).rgb;
    s += texture(uTex, vUV + r * vec2( 0, -1)).rgb * 2.0;
    s += texture(uTex, vUV + r * vec2( 1, -1)).rgb;
    s += texture(uTex, vUV + r * vec2(-1,  0)).rgb * 2.0;
    s += texture(uTex, vUV).rgb * 4.0;
    s += texture(uTex, vUV + r * vec2( 1,  0)).rgb * 2.0;
    s += texture(uTex, vUV + r * vec2(-1,  1)).rgb;
    s += texture(uTex, vUV + r * vec2( 0,  1)).rgb * 2.0;
    s += texture(uTex, vUV + r * vec2( 1,  1)).rgb;
    o = s * (1.0 / 16.0);
}
