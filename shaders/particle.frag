#version 330 core
in vec4 vColor;
out vec4 fragColor;

uniform int uRing;   /* 0 disco cheio (dust/sparks/stars), 1 anel suave (bokeh) */

void main()
{
    vec2 uv = gl_PointCoord * 2.0 - 1.0;
    float d = length(uv);
    float alpha;
    if (uRing != 0) {
        alpha = smoothstep(1.0, 0.7, d) - smoothstep(0.55, 0.25, d) * 0.6;
    } else {
        alpha = smoothstep(1.0, 0.0, d);
    }
    if (alpha <= 0.001) discard;
    fragColor = vec4(vColor.rgb, vColor.a * alpha);
}
