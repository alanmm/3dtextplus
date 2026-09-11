#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform vec2  uTexel;    // 1/tamanho do buffer de streak
uniform vec2  uDir;      // eixo unitario (ex.: (1,0), (0.5,0.866))
uniform float uStep;     // passo desta iteracao (1, 3, 9)
uniform float uLength;   // 0..1 do usuario -> multiplica o espacamento
uniform vec3  uTint;     // branco (starburst) | azulado (anamorfico)
out vec3 o;

const int TAPS = 10;

/* Invariante anti-banding: a razao entre passos consecutivos de uStep (3x,
   ver post.c) deve ficar <= a faixa de t (0..3 acima), senao o passe N+1
   amostra pontos que o passe N nao deixou suaves -> raios com degraus
   visiveis. O primeiro passe (o unico cuja entrada nao e borrada) tambem
   precisa que o espacamento entre amostras fique perto de 1 texel:
   spacing/TAPS ~= uStep*(1+3*uLength)/TAPS. Se aumentar o alcance do
   anamorfico (base_step ou uLength), aumente TAPS na mesma proporcao ou
   volte a aparecer o banding que 3 commits desta fase corrigiram. */

void main()
{
    vec3  c = vec3(0.0);
    float wsum = 0.0;
    float spacing = uStep * (1.0 + uLength * 3.0);
    for (int i = 0; i < TAPS; ++i) {
        float fi = float(i);
        float t  = fi * 3.0 / float(TAPS - 1);
        float w  = pow(0.82, uStep * t);           // atenuacao exponencial
        vec2  off = uDir * uTexel * spacing * t;
        c += texture(uTex, vUV + off).rgb * w;
        c += texture(uTex, vUV - off).rgb * w;     // simetrico -> raio nos 2 sentidos
        wsum += 2.0 * w;
    }
    o = (c / max(wsum, 1e-4)) * uTint;
}
