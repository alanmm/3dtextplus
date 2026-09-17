#version 330 core
layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

uniform vec2  uViewportSize;   // pixels
uniform float uThicknessPx;

void main()
{
    vec4 p0 = gl_in[0].gl_Position;
    vec4 p1 = gl_in[1].gl_Position;

    vec2 ndc0 = p0.xy / p0.w;
    vec2 ndc1 = p1.xy / p1.w;

    // trabalha em espaco de pixel de tela pra garantir espessura
    // uniforme mesmo com viewport nao-quadrado (um "perpendicular" em
    // NDC puro sai distorcido pela razao de aspecto).
    vec2 halfViewport = uViewportSize * 0.5;
    vec2 screen0 = ndc0 * halfViewport;
    vec2 screen1 = ndc1 * halfViewport;

    vec2 dir = screen1 - screen0;
    if (length(dir) < 1e-6) return;   // segmento sem extensao em tela
    dir = normalize(dir);
    vec2 normalPx = vec2(-dir.y, dir.x) * (uThicknessPx * 0.5);
    vec2 offsetNdc = normalPx / halfViewport;

    gl_Position = vec4((ndc0 + offsetNdc) * p0.w, p0.z, p0.w); EmitVertex();
    gl_Position = vec4((ndc0 - offsetNdc) * p0.w, p0.z, p0.w); EmitVertex();
    gl_Position = vec4((ndc1 + offsetNdc) * p1.w, p1.z, p1.w); EmitVertex();
    gl_Position = vec4((ndc1 - offsetNdc) * p1.w, p1.z, p1.w); EmitVertex();
    EndPrimitive();
}
