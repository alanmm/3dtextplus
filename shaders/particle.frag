#version 330 core
in vec4  vColor;
in float vBlur;
in float vRot;
out vec4 fragColor;

uniform int uRing;   /* 0 disco cheio (dust/sparks/stars), 1 hexagono translucido (bokeh) */

/* "distancia" hexagonal SUAVIZADA: vale 1.0 na borda de um hexagono
   regular. Uma soma de potencias (tipo "superellipse") em vez de um
   max() puro dos 3 eixos - o max() tem uma quina de derivada descontinua
   em cada troca de eixo dominante, que o bloom amplifica numa
   estrela/flor de 6 pontas. A potencia finita (n) suaviza essas quinas
   mantendo o hexagono reconhecivel (n maior = cantos mais definidos). */
float hex_shape(vec2 p)
{
    const float n = 16.0;
    vec2 a0 = vec2(1.0, 0.0);
    vec2 a1 = vec2(0.5, 0.8660254);
    vec2 a2 = vec2(-0.5, 0.8660254);
    float d0 = pow(abs(dot(p, a0)), n);
    float d1 = pow(abs(dot(p, a1)), n);
    float d2 = pow(abs(dot(p, a2)), n);
    return pow(d0 + d1 + d2, 1.0 / n);
}

void main()
{
    vec2 uv = gl_PointCoord * 2.0 - 1.0;
    float alpha;
    if (uRing != 0) {
        /* opacidade UNIFORME dentro do hexagono (nao e' um degrade da
           particula em si) - so' a borda tem uma transicao, pra
           antialiasing e (opcionalmente) um pouco de "desfoque" nas mais
           distantes. A variacao de opacidade entre particulas vem do
           passe de profundidade (vColor.a, calculado no vertex shader) e
           do valor aleatorio proprio de cada uma (particles.c), nao daqui. */
        /* rotaciona a amostra antes de medir a forma - sem isso todo
           hexagono sai com os mesmos 3 eixos, ficando simetrico entre
           particulas diferentes */
        float c = cos(vRot), s = sin(vRot);
        vec2 ruv = vec2(c * uv.x - s * uv.y, s * uv.x + c * uv.y);
        float h = hex_shape(ruv);
        float edgeStart = mix(0.88, 0.45, clamp(vBlur, 0.0, 1.0));
        alpha = 1.0 - smoothstep(edgeStart, 1.0, h);
    } else {
        float d = length(uv);
        alpha = smoothstep(1.0, 0.0, d);
    }
    if (alpha <= 0.001) discard;
    fragColor = vec4(vColor.rgb, vColor.a * alpha);
}
