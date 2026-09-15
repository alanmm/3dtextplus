#version 330 core
in vec2 vUV;
uniform sampler2D uAccum;
uniform sampler2D uRevealLog;
out vec4 o;

void main()
{
    vec4  accum     = texture(uAccum, vUV);
    float revealage = exp(texture(uRevealLog, vUV).r);
    float alpha     = clamp(1.0 - revealage, 0.0, 1.0);
    vec3  avgColor  = accum.rgb / max(accum.a, 1e-5);
    o = vec4(avgColor, alpha);
}
