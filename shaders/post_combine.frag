#version 330 core
in vec2 vUV;
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform sampler2D uStreaks;
uniform float uBloomIntensity;
uniform float uStreaksIntensity;
uniform int   uHasBloom;
uniform int   uHasStreaks;
out vec4 o;

void main()
{
    vec3 c = texture(uScene, vUV).rgb;
    if (uHasBloom == 1)   c += texture(uBloom, vUV).rgb   * uBloomIntensity;
    if (uHasStreaks == 1) c += texture(uStreaks, vUV).rgb * uStreaksIntensity;
    o = vec4(c, 1.0);
}
