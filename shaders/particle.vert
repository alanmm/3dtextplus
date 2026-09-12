#version 330 core
layout(location = 0) in vec3  aPos;
layout(location = 1) in float aSize;
layout(location = 2) in vec4  aColor;

uniform mat4  uView;
uniform mat4  uProj;
uniform float uPixelScale;   /* fbHeight * uProj[1][1] * 0.5 - ver particles_render */
uniform float uFadeDist;     /* distancia de referencia p/ o fade por distancia e o bokeh */
uniform int   uRing;         /* 1 = bokeh: cresce (em vez de encolher) com a distancia */

out vec4 vColor;

void main()
{
    vec4 viewPos = uView * vec4(aPos, 1.0);
    float dist = max(-viewPos.z, 0.1);

    if (uRing != 0) {
        /* bokeh: quanto mais longe, maior - efeito de circulo de confusao
           de fundo desfocado, o oposto da projecao em perspectiva normal */
        float distNorm = clamp(dist / uFadeDist, 0.05, 1.5);
        gl_PointSize = clamp(aSize * uPixelScale * distNorm * 0.5, 1.0, 256.0);
    } else {
        gl_PointSize = clamp(aSize * uPixelScale / dist, 1.0, 256.0);
    }

    /* mascara de distancia: quanto mais longe do centro da cena, mais
       transparente - aplicada a todos os tipos. */
    float distFade = clamp(1.0 - dist / (uFadeDist * 2.0), 0.0, 1.0);

    gl_Position = uProj * viewPos;
    vColor = vec4(aColor.rgb, aColor.a * distFade);
}
