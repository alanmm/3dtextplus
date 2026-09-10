#version 330 core
in vec3 vWorld;
in vec3 vNrm;
in float vSurf;

uniform vec3 uCamPos;
uniform vec3 uBaseColor;

out vec4 fragColor;

const vec3 KEY_DIR  = normalize(vec3(-0.40, -0.75, -0.55));  // luz -> superficie
const vec3 FILL_DIR = normalize(vec3( 0.55,  0.30, -0.45));
const vec3 KEY_COL  = vec3(1.00, 0.96, 0.88);
const vec3 FILL_COL = vec3(0.55, 0.62, 0.80);
const vec3 RIM_COL  = vec3(0.55, 0.68, 0.95);

void main()
{
    vec3 N = normalize(vNrm);
    if (!gl_FrontFacing) N = -N;               // extrusao two-sided
    vec3 V = normalize(uCamPos - vWorld);
    vec3 base = uBaseColor;

    float kd = max(dot(N, -KEY_DIR), 0.0);
    vec3 col = base * (0.12 + 0.88 * kd) * KEY_COL;
    col += base * max(dot(N, -FILL_DIR), 0.0) * 0.30 * FILL_COL;

    vec3 H = normalize(-KEY_DIR + V);
    float spec = pow(max(dot(N, H), 0.0), 96.0);
    col += vec3(1.0) * spec * 0.85;

    float rim = pow(1.0 - max(dot(N, V), 0.0), 3.0);
    col += RIM_COL * rim * 0.35;

    if (vSurf > 1.5 && vSurf < 2.5) col *= 0.92;   // paredes um tom abaixo

    fragColor = vec4(col, 1.0);
}
