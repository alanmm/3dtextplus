#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uTexel;   // 1 / tamanho da textura de origem
out vec3 o;
void main()
{
    vec2 t = uTexel;
    vec3 a = texture(uTex, vUV + t * vec2(-2, -2)).rgb;
    vec3 b = texture(uTex, vUV + t * vec2( 0, -2)).rgb;
    vec3 c = texture(uTex, vUV + t * vec2( 2, -2)).rgb;
    vec3 d = texture(uTex, vUV + t * vec2(-2,  0)).rgb;
    vec3 e = texture(uTex, vUV).rgb;
    vec3 f = texture(uTex, vUV + t * vec2( 2,  0)).rgb;
    vec3 g = texture(uTex, vUV + t * vec2(-2,  2)).rgb;
    vec3 h = texture(uTex, vUV + t * vec2( 0,  2)).rgb;
    vec3 i = texture(uTex, vUV + t * vec2( 2,  2)).rgb;
    vec3 j = texture(uTex, vUV + t * vec2(-1, -1)).rgb;
    vec3 k = texture(uTex, vUV + t * vec2( 1, -1)).rgb;
    vec3 l = texture(uTex, vUV + t * vec2(-1,  1)).rgb;
    vec3 m = texture(uTex, vUV + t * vec2( 1,  1)).rgb;
    o = e * 0.125
      + (a + c + g + i) * 0.03125
      + (b + d + f + h) * 0.0625
      + (j + k + l + m) * 0.125;
}
