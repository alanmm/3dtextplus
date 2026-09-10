#version 330 core
in vec2 vUV;
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform float uBloomIntensity;
uniform int   uHasBloom;
out vec4 o;

vec3 aces(vec3 x)
{
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main()
{
    vec3 c = texture(uScene, vUV).rgb;
    if (uHasBloom == 1) c += texture(uBloom, vUV).rgb * uBloomIntensity;
    c = aces(c);
    c = pow(c, vec3(1.0 / 2.2));
    o = vec4(c, 1.0);
}
