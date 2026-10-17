#include "test.h"
/* Inclui o .c pra testar a simulacao de CPU da poeira (agitacao browniana, caixa que da'
   a volta, fade nas bordas). Nada de GL e' chamado aqui; so' o stub abaixo, pra linkar. */
#include "particles.c"

unsigned gl_program(const char *vs_src, const char *fs_src) { (void)vs_src; (void)fs_src; return 0; }

/* maior ocupacao relativa (vs. uniforme) de x, z e da celula (x,z) */
static void clump_stats(const ParticleSystem *p, double *max_x, double *max_z, double *max_cell)
{
    int hxb[40] = {0}, hzb[40] = {0}, cell[20][20];
    memset(cell, 0, sizeof cell);
    for (int i = 0; i < p->count; ++i) {
        int bx = (int)((p->buf[i].x + p->box_hx) / (2.0f * p->box_hx) * 40.0f);
        int bz = (int)((p->buf[i].z + p->box_hz) / (2.0f * p->box_hz) * 40.0f);
        if (bx < 0) bx = 0;
        if (bx > 39) bx = 39;
        if (bz < 0) bz = 0;
        if (bz > 39) bz = 39;
        hxb[bx]++; hzb[bz]++; cell[bx / 2][bz / 2]++;
    }
    int mx = 0, mz = 0, mc = 0;
    for (int b = 0; b < 40; ++b) { if (hxb[b] > mx) mx = hxb[b]; if (hzb[b] > mz) mz = hzb[b]; }
    for (int a = 0; a < 20; ++a) for (int b = 0; b < 20; ++b) if (cell[a][b] > mc) mc = cell[a][b];
    *max_x = mx / ((double)p->count / 40.0);
    *max_z = mz / ((double)p->count / 40.0);
    *max_cell = mc / ((double)p->count / 400.0);
}

static ParticleSystem *make_dust(unsigned seed, float density, float speed)
{
    ParticleSystem *p = (ParticleSystem *)calloc(1, sizeof *p);
    if (!p) return NULL;
    p->kind = -1; p->last_density = -1.0f; p->seed = seed;
    Config cfg;
    config_defaults(&cfg);
    cfg.particles_on = 1; cfg.particles_kind = 0;
    cfg.particles_density = density;
    cfg.particles_speed = speed; cfg.particles_size_scale = 1.0f; cfg.particles_opacity = 1.0f;
    particles_set_config(p, &cfg, 1.2f, 0.4f, 0.1f, NULL, NULL, 0);
    return p;
}

void run_particles_tests(void)
{
    /* normal padrao do Box-Muller: media 0, variancia 1 */
    {
        unsigned s = 4242u;
        double sum = 0, sum2 = 0;
        const int N = 40000;
        for (int i = 0; i < N; ++i) { double g = gaussf(&s); sum += g; sum2 += g * g; }
        double mean = sum / N, var = sum2 / N - mean * mean;
        EXPECT(fabs(mean) < 0.03);
        EXPECT(fabs(var - 1.0) < 0.05);
    }

    /* fade nas faces da caixa: 1 no miolo, 0 na face (a volta da caixa fica invisivel) */
    {
        const float H = 5.0f;
        EXPECT(edge_weight(0.0f, H) == 1.0f);
        EXPECT(edge_weight(H * (1.0f - EDGE_FADE_FRAC), H) == 1.0f);
        EXPECT(edge_weight(H, H) == 0.0f);
        EXPECT(edge_weight(-H, H) == 0.0f);
        EXPECT(edge_weight(H * 0.9f, H) > 0.0f && edge_weight(H * 0.9f, H) < 1.0f);
        EXPECT(edge_weight(H * 0.85f, H) > edge_weight(H * 0.95f, H));   /* decresce rumo a face */
        EXPECT(edge_weight(-H * 0.9f, H) == edge_weight(H * 0.9f, H));   /* simetrico */
    }

    /* movimento: independente por particula, uniforme no tempo e preso na caixa */
    {
        ParticleSystem *p = make_dust(777u, 1.0f, 1.0f);   /* 800 particulas, velocidade 1 */
        EXPECT(p != NULL);
        if (!p) return;
        EXPECT(p->count == 800);

        m4 model;
        memset(&model, 0, sizeof model);
        double worst_x = 0, worst_z = 0, worst_cell = 0;
        for (int step = 1; step <= 30 * 60 * 5; ++step) {          /* 5 min a 30 passos/s */
            particles_update(p, 1.0f / 30.0f, model);
            if (step % (30 * 60) == 0 && step >= 30 * 60 * 2) {      /* minutos 2..5 */
                double mx, mz, mc;
                clump_stats(p, &mx, &mz, &mc);
                if (mx > worst_x) worst_x = mx;
                if (mz > worst_z) worst_z = mz;
                if (mc > worst_cell) worst_cell = mc;
            }
        }
        /* campo de fluxo antigo: faixa vertical, densidade local dezenas de vezes a media */
        EXPECT(worst_x < 2.2);
        EXPECT(worst_z < 2.2);
        EXPECT(worst_cell < 7.0);

        int inside = 1;
        double mvx = 0, mvy = 0, mvz = 0, speed2 = 0;
        for (int i = 0; i < p->count; ++i) {
            const Particle *q = &p->buf[i];
            if (fabsf(q->x) > p->box_hx + 1e-3f || fabsf(q->y) > p->box_hy + 1e-3f || fabsf(q->z) > p->box_hz + 1e-3f)
                inside = 0;
            mvx += q->vx; mvy += q->vy; mvz += q->vz;
            speed2 += (double)q->vx * q->vx + (double)q->vy * q->vy + (double)q->vz * q->vz;
        }
        mvx /= p->count; mvy /= p->count; mvz /= p->count;
        double rms = sqrt(speed2 / p->count);
        EXPECT(inside);
        EXPECT(p->count == 800);
        EXPECT(fabs(mvx) < 0.02 && fabs(mvz) < 0.02);         /* nenhum vento / deriva coletiva */
        EXPECT(fabs(mvy - DRIFT_SETTLE) < 0.01);              /* so' o assentamento lento em y */
        EXPECT(rms > 0.08 && rms < 0.30);                     /* agitacao da ordem de DRIFT_SIGMA_DUST */

        /* vizinhas nao se movem juntas: correlacao entre direcoes de particulas proximas ~ 0
           (no campo antigo era alta - a nuvem inteira girava como um globo) */
        double sum_cos = 0; long pairs = 0;
        for (int i = 0; i < p->count; ++i) {
            for (int j = i + 1; j < p->count; ++j) {
                const Particle *a = &p->buf[i], *b = &p->buf[j];
                float dx = a->x - b->x, dy = a->y - b->y, dz = a->z - b->z;
                if (dx * dx + dy * dy + dz * dz > 1.0f) continue;
                double na = sqrt((double)a->vx * a->vx + (double)a->vy * a->vy + (double)a->vz * a->vz);
                double nb = sqrt((double)b->vx * b->vx + (double)b->vy * b->vy + (double)b->vz * b->vz);
                if (na < 1e-6 || nb < 1e-6) continue;
                sum_cos += ((double)a->vx * b->vx + (double)a->vy * b->vy + (double)a->vz * b->vz) / (na * nb);
                ++pairs;
            }
        }
        EXPECT(pairs > 1000);
        EXPECT(fabs(sum_cos / (double)pairs) < 0.1);

        free(p);
    }

    /* velocidade 0 congela tudo */
    {
        ParticleSystem *p = make_dust(31u, 0.5f, 0.0f);
        EXPECT(p != NULL);
        if (!p) return;
        float x0 = p->buf[0].x, y0 = p->buf[0].y, z0 = p->buf[0].z;
        m4 model;
        memset(&model, 0, sizeof model);
        for (int i = 0; i < 300; ++i) particles_update(p, 1.0f / 30.0f, model);
        EXPECT(p->buf[0].x == x0 && p->buf[0].y == y0 && p->buf[0].z == z0);
        free(p);
    }
}
