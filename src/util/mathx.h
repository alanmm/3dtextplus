#ifndef M3DT_MATHX_H
#define M3DT_MATHX_H

typedef struct { float x, y; } v2;
typedef struct { float x, y, z; } v3;
typedef struct { float m[16]; } m4;   /* coluna-major: m[col*4 + row] */

v3    v3_add(v3 a, v3 b);
v3    v3_sub(v3 a, v3 b);
v3    v3_scale(v3 a, float s);
float v3_dot(v3 a, v3 b);
v3    v3_cross(v3 a, v3 b);
float v3_len(v3 a);
v3    v3_norm(v3 a);

m4 m4_identity(void);
m4 m4_mul(m4 a, m4 b);
m4 m4_perspective(float fovy_rad, float aspect, float znear, float zfar);
m4 m4_look_at(v3 eye, v3 center, v3 up);
m4 m4_translate(v3 t);
m4 m4_rotate_x(float rad);
m4 m4_rotate_y(float rad);
v3 m4_mul_point(m4 a, v3 p);

float m3dt_radians(float deg);
float pendulum_angle(float t, float period, float maxdeg);

#endif
