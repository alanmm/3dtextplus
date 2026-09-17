#version 330 core
out vec4 fragColor;

uniform vec3  uLineColor;
uniform vec3  uEmissiveColor;
uniform float uEmissiveAmount;
uniform int   uXray;
uniform sampler2D uSolidDepth;   // so' amostrado quando uXray == 1
uniform vec2  uViewportSize;

void main()
{
    vec3 col = uLineColor + uEmissiveColor * uEmissiveAmount;

    if (uXray == 1) {
        vec2 uv = gl_FragCoord.xy / uViewportSize;
        float solidDepth = texture(uSolidDepth, uv).r;
        // gl_FragCoord.z e' a profundidade desta linha na mesma
        // convencao [0,1] de uma textura GL_DEPTH_COMPONENT - se ela
        // esta' na frente/na superficie da malha solida (ou a malha
        // solida nao escreveu nada ali, profundidade==1 = "infinito"),
        // fica opaca; senao (atras, oculta) fica em 60% - efeito
        // "fantasma" do Blender pedido pelo usuario.
        float alpha = (gl_FragCoord.z <= solidDepth + 1e-5) ? 1.0 : 0.6;
        fragColor = vec4(col, alpha);
    } else {
        fragColor = vec4(col, 1.0);
    }
}
