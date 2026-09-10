#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform float uThreshold;
uniform float uKnee;       // = 0.5 * threshold
out vec3 o;
void main()
{
    vec3 c = texture(uTex, vUV).rgb;
    float br = max(c.r, max(c.g, c.b));
    float k = max(uKnee, 1e-4);
    float soft = clamp(br - uThreshold + k, 0.0, 2.0 * k);
    soft = soft * soft / (4.0 * k);
    float w = max(soft, br - uThreshold) / max(br, 1e-4);
    o = c * w;
}
