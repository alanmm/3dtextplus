#version 330 core
layout(location = 0) in vec3  aPos;
layout(location = 1) in float aSize;
layout(location = 2) in vec4  aColor;
layout(location = 3) in float aRot;
layout(location = 4) in float aBlurSeed;

uniform mat4  uView;
uniform mat4  uProj;
uniform float uPixelScale;   /* fbHeight * uProj[1][1] * 0.5 - ver particles_render */
uniform float uFadeDist;     /* distancia de referencia p/ o passe de profundidade e o bokeh */
uniform int   uRing;         /* 1 = bokeh: cresce (em vez de encolher) com a distancia */

/* quanto o blur varia entre particulas vizinhas na MESMA profundidade, em
   vez de todo blur vir so' da distancia (o que fazia todas ficarem com a
   mesma nitidez). Maior = mais variedade de blur; menor = mais uniforme/
   dependente so' da distancia. Ajustar aqui se o valor nao ficar bom. */
const float BLUR_RAND_SPREAD = 0.7;

out vec4  vColor;
out float vBlur;
out float vRot;

void main()
{
    vec4 viewPos = uView * vec4(aPos, 1.0);
    float dist = max(-viewPos.z, 0.1);
    float distNorm = clamp(dist / uFadeDist, 0.0, 1.5);

    if (uRing != 0) {
        /* bokeh: quanto mais longe, maior - efeito de circulo de confusao
           de fundo desfocado, o oposto da projecao em perspectiva normal */
        gl_PointSize = clamp(aSize * uPixelScale * max(distNorm, 0.05) * 0.5, 1.0, 256.0);
        vBlur = clamp(distNorm + (aBlurSeed - 0.5) * BLUR_RAND_SPREAD, 0.0, 1.5);
    } else {
        gl_PointSize = clamp(aSize * uPixelScale / dist, 1.0, 256.0);
        vBlur = 0.0;
    }
    vRot = aRot;

    /* passe de profundidade: quanto mais longe da camera, mais transparente -
       e' uma mascara multiplicada em cima do alfa PROPRIO de cada particula
       (aColor.a, ja aleatorio por particula - ver particles.c), nao troca a
       forma da particula em si. */
    float depthFade = clamp(1.0 - dist / (uFadeDist * 2.0), 0.0, 1.0);

    gl_Position = uProj * viewPos;
    vColor = vec4(aColor.rgb, aColor.a * depthFade);
}
