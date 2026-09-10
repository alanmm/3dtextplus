#include "test.h"
#include "geometry/sdf.h"
#include "geometry/geom_types.h"
#include <math.h>
#include <stdlib.h>

static ContourSet circle_cs(float R, int n)
{
    ContourSet cs;
    cs.count = 1;
    cs.contours = (Contour *)calloc(1, sizeof(Contour));
    cs.contours[0].count = n;
    cs.contours[0].pts = (v2 *)malloc((size_t)n * sizeof(v2));
    for (int i = 0; i < n; ++i) {
        float a = (float)i / (float)n * 6.2831853f;
        cs.contours[0].pts[i] = (v2){ R * cosf(a), R * sinf(a) };
    }
    cs.minx = -R; cs.maxx = R; cs.miny = -R; cs.maxy = R;
    return cs;
}
static void cs_free(ContourSet *cs)
{
    for (int i = 0; i < cs->count; ++i) free(cs->contours[i].pts);
    free(cs->contours);
}

void run_sdf_tests(void)
{
    const float R = 1.0f;
    ContourSet cs = circle_cs(R, 64);
    Sdf s;
    EXPECT(sdf_build(&cs, 256, &s) == 1);

    float tol = 4.0f * s.size_x / (float)s.res;   /* ~4 texels */

    EXPECT(fabsf(sdf_sample(&s, 0.0f, 0.0f) - (-R)) < tol);      /* centro ~ -R */
    EXPECT(fabsf(sdf_sample(&s, R, 0.0f)) < tol);                /* borda ~ 0 */
    EXPECT(fabsf(sdf_sample(&s, R * 1.2f, 0.0f) - 0.2f * R) < tol); /* fora ~ +0.2R */

    /* gradiente perto da borda aponta radialmente para fora */
    /* amostra numerica do gradiente via sdf_sample */
    float e = s.size_x / (float)s.res;
    float gx = (sdf_sample(&s, R * 0.9f + e, 0.0f) - sdf_sample(&s, R * 0.9f - e, 0.0f)) / (2 * e);
    float gy = (sdf_sample(&s, R * 0.9f, e) - sdf_sample(&s, R * 0.9f, -e)) / (2 * e);
    EXPECT(gx > 0.5f && fabsf(gy) < 0.3f);

    sdf_free(&s);
    cs_free(&cs);

    /* quadrado com furo: centro do furo -> fora (dist > 0) */
    {
        ContourSet q;
        q.count = 2;
        q.contours = (Contour *)calloc(2, sizeof(Contour));
        float outer[] = { -2,-2,  2,-2,  2,2,  -2,2 };
        float inner[] = { -1,1,  1,1,  1,-1,  -1,-1 };   /* winding oposto */
        for (int c = 0; c < 2; ++c) {
            const float *src = c ? inner : outer;
            q.contours[c].count = 4;
            q.contours[c].pts = (v2 *)malloc(4 * sizeof(v2));
            for (int i = 0; i < 4; ++i) q.contours[c].pts[i] = (v2){ src[i*2], src[i*2+1] };
        }
        q.minx = -2; q.maxx = 2; q.miny = -2; q.maxy = 2;
        Sdf sh;
        EXPECT(sdf_build(&q, 192, &sh) == 1);
        EXPECT(sdf_sample(&sh, 0.0f, 0.0f) > 0.0f);   /* meio do furo eh fora */
        EXPECT(sdf_sample(&sh, 1.5f, 0.0f) < 0.0f);   /* na parede eh dentro */
        sdf_free(&sh);
        cs_free(&q);
    }

    /* degenerado -> 0, sem crash */
    {
        ContourSet d;
        d.count = 1;
        d.contours = (Contour *)calloc(1, sizeof(Contour));
        d.contours[0].count = 2;
        d.contours[0].pts = (v2 *)malloc(2 * sizeof(v2));
        d.contours[0].pts[0] = (v2){ 0, 0 };
        d.contours[0].pts[1] = (v2){ 1, 1 };
        d.minx = 0; d.maxx = 1; d.miny = 0; d.maxy = 1;
        Sdf sd;
        int r = sdf_build(&d, 64, &sd);
        EXPECT(r == 0);
        cs_free(&d);
    }
}
