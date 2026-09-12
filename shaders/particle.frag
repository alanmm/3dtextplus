#version 330 core
in vec4 vColor;
out vec4 fragColor;

uniform int uRing;   /* 0 disco cheio (dust/sparks/stars), 1 hexagono translucido (bokeh) */

/* "distancia" hexagonal: vale 1.0 na borda de um hexagono regular centrado
   na origem (projecao nos 3 eixos a 60 graus). Barata e suave o bastante
   pra um bokeh decorativo - nao precisa de uma SDF exata. */
float hex_shape(vec2 p)
{
    vec2 a0 = vec2(1.0, 0.0);
    vec2 a1 = vec2(0.5, 0.8660254);
    vec2 a2 = vec2(-0.5, 0.8660254);
    return max(abs(dot(p, a0)), max(abs(dot(p, a1)), abs(dot(p, a2))));
}

void main()
{
    vec2 uv = gl_PointCoord * 2.0 - 1.0;
    float alpha;
    if (uRing != 0) {
        /* queda suave e REDONDA do centro pra borda (tipo desfoque de
           lente) - usar a "distancia" hexagonal direto como expoente cria
           cristas nos 6 eixos que o bloom transforma numa estrela/flor.
           O hexagono entra so' como um recorte na borda externa. */
        float d = length(uv);
        float radial = pow(clamp(1.0 - d, 0.0, 1.0), 1.6);
        float edge = smoothstep(1.0, 0.85, hex_shape(uv));
        alpha = radial * edge;
    } else {
        float d = length(uv);
        alpha = smoothstep(1.0, 0.0, d);
    }
    if (alpha <= 0.001) discard;
    fragColor = vec4(vColor.rgb, vColor.a * alpha);
}
