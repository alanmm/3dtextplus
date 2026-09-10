#include "util/mathx.h"
#include <math.h>

v3 v3_add(v3 a, v3 b)      { return (v3){ a.x + b.x, a.y + b.y, a.z + b.z }; }
v3 v3_sub(v3 a, v3 b)      { return (v3){ a.x - b.x, a.y - b.y, a.z - b.z }; }
v3 v3_scale(v3 a, float s) { return (v3){ a.x * s, a.y * s, a.z * s }; }
float v3_dot(v3 a, v3 b)   { return a.x * b.x + a.y * b.y + a.z * b.z; }

v3 v3_cross(v3 a, v3 b)
{
    return (v3){ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

float v3_len(v3 a) { return sqrtf(v3_dot(a, a)); }

v3 v3_norm(v3 a)
{
    float l = v3_len(a);
    return l > 1e-8f ? v3_scale(a, 1.0f / l) : (v3){ 0, 0, 0 };
}

m4 m4_identity(void)
{
    m4 r = {{0}};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

m4 m4_mul(m4 a, m4 b)
{
    m4 r = {{0}};
    for (int c = 0; c < 4; ++c)
        for (int row = 0; row < 4; ++row) {
            float s = 0.0f;
            for (int k = 0; k < 4; ++k) s += a.m[k * 4 + row] * b.m[c * 4 + k];
            r.m[c * 4 + row] = s;
        }
    return r;
}

m4 m4_perspective(float fovy_rad, float aspect, float znear, float zfar)
{
    float f = 1.0f / tanf(fovy_rad * 0.5f);
    m4 r = {{0}};
    r.m[0]  = f / aspect;
    r.m[5]  = f;
    r.m[10] = (zfar + znear) / (znear - zfar);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * zfar * znear) / (znear - zfar);
    return r;
}

m4 m4_look_at(v3 eye, v3 center, v3 up)
{
    v3 f = v3_norm(v3_sub(center, eye));
    v3 s = v3_norm(v3_cross(f, up));
    v3 u = v3_cross(s, f);
    m4 r = m4_identity();
    r.m[0] = s.x;  r.m[4] = s.y;  r.m[8]  = s.z;
    r.m[1] = u.x;  r.m[5] = u.y;  r.m[9]  = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -v3_dot(s, eye);
    r.m[13] = -v3_dot(u, eye);
    r.m[14] = v3_dot(f, eye);
    return r;
}

m4 m4_translate(v3 t)
{
    m4 r = m4_identity();
    r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
    return r;
}

m4 m4_rotate_x(float rad)
{
    float c = cosf(rad), s = sinf(rad);
    m4 r = m4_identity();
    r.m[5] = c;  r.m[6] = s;
    r.m[9] = -s; r.m[10] = c;
    return r;
}

m4 m4_rotate_y(float rad)
{
    float c = cosf(rad), s = sinf(rad);
    m4 r = m4_identity();
    r.m[0] = c;  r.m[2] = -s;
    r.m[8] = s;  r.m[10] = c;
    return r;
}

v3 m4_mul_point(m4 a, v3 p)
{
    float x = a.m[0] * p.x + a.m[4] * p.y + a.m[8]  * p.z + a.m[12];
    float y = a.m[1] * p.x + a.m[5] * p.y + a.m[9]  * p.z + a.m[13];
    float z = a.m[2] * p.x + a.m[6] * p.y + a.m[10] * p.z + a.m[14];
    float w = a.m[3] * p.x + a.m[7] * p.y + a.m[11] * p.z + a.m[15];
    if (fabsf(w) > 1e-8f) { x /= w; y /= w; z /= w; }
    return (v3){ x, y, z };
}

float m3dt_radians(float deg) { return deg * 3.14159265358979323846f / 180.0f; }

float pendulum_angle(float t, float period, float maxdeg)
{
    if (period <= 1e-4f) return 0.0f;
    float u = fmodf(t / period, 1.0f);
    if (u < 0.0f) u += 1.0f;
    /* onda triangular: -1 em u=0, +1 em u=0.5, -1 em u=1 */
    float tri = (u < 0.5f) ? (u * 4.0f - 1.0f) : (3.0f - u * 4.0f);
    float s = (tri + 1.0f) * 0.5f;                       /* 0..1 */
    s = s * s * s * (s * (s * 6.0f - 15.0f) + 10.0f);    /* smootherstep -> C2 nos extremos */
    return (s * 2.0f - 1.0f) * maxdeg;
}
