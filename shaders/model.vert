#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in float aSurf;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3  vWorld;
out vec3  vNrm;       // normal em espaco de mundo
out vec3  vNrmLocal;  // normal em espaco local (para o bevel via SDF)
out float vSurf;
out vec2  vLocalXY;   // posicao local no plano do contorno
out float vLocalZ;    // z local (para a juncao bevel<->parede)

void main()
{
    vec4 w = uModel * vec4(aPos, 1.0);
    vWorld = w.xyz;
    vNrm = mat3(uModel) * aNrm;
    vNrmLocal = aNrm;
    vSurf = aSurf;
    vLocalXY = aPos.xy;
    vLocalZ = aPos.z;
    gl_Position = uProj * uView * w;
}
