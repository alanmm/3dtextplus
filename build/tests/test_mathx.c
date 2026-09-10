#include "test.h"
#include "util/mathx.h"
#include <math.h>

static int near(float a, float b) { return fabsf(a - b) < 1e-4f; }

void run_mathx_tests(void)
{
    /* vetores */
    EXPECT(near(v3_len((v3){3, 4, 0}), 5.0f));
    v3 n = v3_norm((v3){0, 5, 0});
    EXPECT(near(n.y, 1.0f));
    v3 c = v3_cross((v3){1, 0, 0}, (v3){0, 1, 0});
    EXPECT(near(c.z, 1.0f));
    EXPECT(near(v3_dot((v3){1, 2, 3}, (v3){4, 5, 6}), 32.0f));

    /* identidade */
    m4 I = m4_identity();
    m4 II = m4_mul(I, I);
    for (int i = 0; i < 16; ++i) EXPECT(near(II.m[i], I.m[i]));

    /* rotate_y(90 graus): (1,0,0) -> (0,0,-1) */
    v3 r = m4_mul_point(m4_rotate_y(m3dt_radians(90.0f)), (v3){1, 0, 0});
    EXPECT(near(r.x, 0.0f) && near(r.y, 0.0f) && near(r.z, -1.0f));

    /* rotate_x(90 graus): (0,1,0) -> (0,0,1) */
    v3 rx = m4_mul_point(m4_rotate_x(m3dt_radians(90.0f)), (v3){0, 1, 0});
    EXPECT(near(rx.x, 0.0f) && near(rx.y, 0.0f) && near(rx.z, 1.0f));

    /* translate */
    v3 t = m4_mul_point(m4_translate((v3){2, -3, 4}), (v3){1, 1, 1});
    EXPECT(near(t.x, 3.0f) && near(t.y, -2.0f) && near(t.z, 5.0f));

    /* look_at: o olho vai para a origem no espaco de view */
    v3 e = m4_mul_point(m4_look_at((v3){0, 0, 10}, (v3){0, 0, 0}, (v3){0, 1, 0}), (v3){0, 0, 10});
    EXPECT(near(e.x, 0.0f) && near(e.y, 0.0f) && near(e.z, 0.0f));

    /* perspective: ponto no plano near mapeia para z_ndc ~ -1 */
    m4 P = m4_perspective(m3dt_radians(60.0f), 1.0f, 1.0f, 100.0f);
    v3 pn = m4_mul_point(P, (v3){0, 0, -1.0f});
    EXPECT(near(pn.z, -1.0f));

    /* pendulo */
    EXPECT(near(pendulum_angle(0.0f, 8.0f, 45.0f), -45.0f));
    EXPECT(near(pendulum_angle(4.0f, 8.0f, 45.0f), 45.0f));
    EXPECT(fabsf(pendulum_angle(2.0f, 8.0f, 45.0f)) < 0.5f);
    for (float tt = 0; tt < 40; tt += 0.13f)
        EXPECT(fabsf(pendulum_angle(tt, 8.0f, 45.0f)) <= 45.0f + 1e-3f);
    float d = (pendulum_angle(0.02f, 8.0f, 45.0f) - pendulum_angle(0.0f, 8.0f, 45.0f)) / 0.02f;
    EXPECT(fabsf(d) < 2.0f);
    EXPECT(near(pendulum_angle(1.0f, 8.0f, 45.0f), -pendulum_angle(5.0f, 8.0f, 45.0f)));
}
