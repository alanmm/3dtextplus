#version 330 core
layout(location = 0) in vec3  aPos;
layout(location = 1) in float aSize;
layout(location = 2) in vec4  aColor;

uniform mat4  uView;
uniform mat4  uProj;
uniform float uPixelScale;   /* fbHeight * uProj[1][1] * 0.5 - ver particles_render */

out vec4 vColor;

void main()
{
    vec4 viewPos = uView * vec4(aPos, 1.0);
    float dist = max(-viewPos.z, 0.1);
    gl_PointSize = clamp(aSize * uPixelScale / dist, 1.0, 256.0);
    gl_Position = uProj * viewPos;
    vColor = aColor;
}
