#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform vec2  uTexel;    // 1/tamanho do buffer de streak
uniform vec2  uDir;      // eixo unitario (ex.: (1,0), (0.5,0.866))
uniform float uStep;     // passo desta iteracao (1, 4, 16)
uniform float uLength;   // 0..1 do usuario -> multiplica o espacamento
uniform vec3  uTint;     // branco (starburst) | azulado (anamorfico)
out vec3 o;

const int TAPS = 4;

void main()
{
    vec3  c = vec3(0.0);
    float wsum = 0.0;
    float spacing = uStep * (1.0 + uLength * 3.0);
    for (int i = 0; i < TAPS; ++i) {
        float fi = float(i);
        float w  = pow(0.82, uStep * fi);          // atenuacao exponencial
        vec2  off = uDir * uTexel * spacing * fi;
        c += texture(uTex, vUV + off).rgb * w;
        c += texture(uTex, vUV - off).rgb * w;     // simetrico -> raio nos 2 sentidos
        wsum += 2.0 * w;
    }
    o = (c / max(wsum, 1e-4)) * uTint;
}
