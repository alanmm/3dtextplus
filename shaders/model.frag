#version 330 core
in vec3 vWorld;
in vec3 vNrm;
in float vSurf;

uniform vec3  uCamPos;
uniform vec3  uBaseColor;
uniform int   uMode;         // 0 classico, 1 metalico, 2 vidro, 3 fosco
uniform float uMetalness;    // 0..1
uniform float uRoughness;    // 0..1
uniform sampler2D uEnvTex;
uniform int   uHasEnv;       // 0/1

out vec4 fragColor;

const float PI = 3.14159265;
const vec3 KEY_DIR  = normalize(vec3(-0.40, -0.75, -0.55));
const vec3 FILL_DIR = normalize(vec3( 0.55,  0.30, -0.45));

vec3 proc_env(vec3 d)
{
    vec3 sky    = mix(vec3(0.10, 0.12, 0.17), vec3(0.50, 0.60, 0.82),
                      clamp(d.y * 0.5 + 0.5, 0.0, 1.0));
    vec3 ground = vec3(0.06, 0.055, 0.05);
    vec3 col = mix(ground, sky, smoothstep(-0.15, 0.15, d.y));
    col += vec3(1.00, 0.96, 0.88)
           * pow(max(dot(d, normalize(vec3( 0.45, 0.55, 0.35))), 0.0), 40.0) * 4.0;
    col += vec3(0.35, 0.45, 0.65)
           * pow(max(dot(d, normalize(vec3(-0.55, 0.15, -0.30))), 0.0), 10.0) * 0.8;
    return col;
}

vec3 sample_env(vec3 d, float rough)
{
    vec3 e;
    if (uHasEnv == 1) {
        vec2 uv = vec2(atan(d.z, d.x) / (2.0 * PI) + 0.5,
                       acos(clamp(d.y, -1.0, 1.0)) / PI);
        e = textureLod(uEnvTex, uv, rough * 6.0).rgb;
    } else {
        e = proc_env(d);
    }
    vec3 ambient = proc_env(vec3(0.0, 1.0, 0.0)) * 0.6 + proc_env(vec3(0.0, -1.0, 0.0)) * 0.4;
    return mix(e, ambient, rough * 0.6);
}

void main()
{
    vec3 N = normalize(vNrm);
    if (!gl_FrontFacing) N = -N;
    vec3 V = normalize(uCamPos - vWorld);
    vec3 R = reflect(-V, N);
    vec3 base = uBaseColor;
    float fres = pow(1.0 - max(dot(N, V), 0.0), 5.0);

    if (uMode == 1) {                         // metalico
        vec3 env  = sample_env(R, uRoughness);
        vec3 tint = mix(vec3(1.0), base, uMetalness);
        vec3 col  = env * tint;
        col += vec3(1.0) * fres * (0.15 + 0.85 * (1.0 - uRoughness));
        float kd = max(dot(N, -KEY_DIR), 0.0);
        col = mix(col, base * (0.2 + 0.8 * kd), (1.0 - uMetalness) * 0.5);
        if (vSurf > 1.5 && vSurf < 2.5) col *= 0.9;
        fragColor = vec4(col, 1.0);
        return;
    }
    if (uMode == 2) {                         // vidro (passe transparente)
        vec3 env  = sample_env(R, uRoughness * 0.5);
        vec3 refr = base * 0.6;
        float m   = clamp(fres + 0.15, 0.0, 1.0);
        vec3 col  = mix(refr, env, m);
        vec3 H = normalize(-KEY_DIR + V);
        col += vec3(1.0) * pow(max(dot(N, H), 0.0), 120.0);
        fragColor = vec4(col, mix(0.35, 0.95, m));
        return;
    }
    if (uMode == 3) {                         // fosco
        float w = dot(N, -KEY_DIR) * 0.5 + 0.5;
        vec3 col = base * (0.15 + 0.85 * w * w);
        col += base * max(dot(N, -FILL_DIR), 0.0) * 0.20;
        if (vSurf > 1.5) col *= 0.82;
        fragColor = vec4(col, 1.0);
        return;
    }

    /* uMode == 0: especular classico */
    float kd = max(dot(N, -KEY_DIR), 0.0);
    vec3 col = base * (0.12 + 0.88 * kd) * vec3(1.00, 0.96, 0.88);
    col += base * max(dot(N, -FILL_DIR), 0.0) * 0.30 * vec3(0.55, 0.62, 0.80);
    vec3 H = normalize(-KEY_DIR + V);
    col += vec3(1.0) * pow(max(dot(N, H), 0.0), 96.0) * 0.85;
    col += vec3(0.55, 0.68, 0.95) * pow(1.0 - max(dot(N, V), 0.0), 3.0) * 0.35;
    if (vSurf > 1.5 && vSurf < 2.5) col *= 0.92;
    fragColor = vec4(col, 1.0);
}
