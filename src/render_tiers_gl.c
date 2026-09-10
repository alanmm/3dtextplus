/* AutoQuality: mede o tempo de GPU por frame (GL_TIME_ELAPSED) e caminha o
   ladder de qualidade com histerese. A decisao pura fica em aq_decide()
   (render_tiers.c); aqui e a coleta de tempos + o estado. */
#include "render_tiers.h"
#include "util/log.h"

#include <glad/gl.h>
#include <windows.h>
#include <stdlib.h>
#include <string.h>

#define AQ_RING 32

struct AutoQuality {
    Config        cfg;
    M3dtTier      tier;
    int           step;
    int           enabled;           /* 0 se timer query indisponivel */
    int           forced_ms;         /* M3DT_AQ_FORCE_MS: > 0 injeta esse tempo */
    int           fast;              /* M3DT_AQ_FAST: janelas curtas p/ teste */
    unsigned      q[2];              /* timer queries double-buffered */
    int           qi;
    int           primed;
    double        ring[AQ_RING];
    int           ring_n, ring_head;
    LARGE_INTEGER last_change, freq;
};

static void ring_push(AutoQuality *aq, double ms)
{
    aq->ring[aq->ring_head] = ms;
    aq->ring_head = (aq->ring_head + 1) % AQ_RING;
    if (aq->ring_n < AQ_RING) aq->ring_n++;
}

static int cmp_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static int env_int(const char *name, int dflt)
{
    char b[16];
    DWORD n = GetEnvironmentVariableA(name, b, sizeof b);
    if (n == 0 || n >= sizeof b) return dflt;
    return atoi(b);
}

static int env_flag(const char *name)
{
    char b[8];
    DWORD n = GetEnvironmentVariableA(name, b, sizeof b);
    return n > 0 && n < sizeof b && b[0] != '0';
}

AutoQuality *aq_create(const Config *cfg, M3dtTier tier)
{
    AutoQuality *aq = (AutoQuality *)calloc(1, sizeof *aq);
    if (!aq) return NULL;
    aq->cfg = *cfg;
    aq->tier = tier;
    aq->step = 0;
    aq->enabled = (tier == M3DT_TIER_FULL && render_quality_ladder_len(tier) > 1);
    aq->forced_ms = env_int("M3DT_AQ_FORCE_MS", 0);
    aq->fast = env_flag("M3DT_AQ_FAST");
    QueryPerformanceFrequency(&aq->freq);
    QueryPerformanceCounter(&aq->last_change);

    if (aq->enabled && aq->forced_ms <= 0) {
        while (glGetError() != GL_NO_ERROR) { /* limpa erros pendentes */ }
        glGenQueries(2, aq->q);
        if (glGetError() != GL_NO_ERROR || !aq->q[0]) {
            aq->enabled = 0;
            aq->q[0] = aq->q[1] = 0;
            log_infof("autoQuality: timer query indisponivel, desabilitado");
        }
    }
    return aq;
}

void aq_destroy(AutoQuality *aq)
{
    if (!aq) return;
    if (aq->q[0]) glDeleteQueries(2, aq->q);
    free(aq);
}

void aq_frame_begin(AutoQuality *aq)
{
    if (!aq || !aq->enabled || aq->forced_ms > 0) return;
    glBeginQuery(GL_TIME_ELAPSED, aq->q[aq->qi]);
}

void aq_frame_end(AutoQuality *aq)
{
    if (!aq) return;
    if (aq->forced_ms > 0) { ring_push(aq, (double)aq->forced_ms); return; }
    if (!aq->enabled) return;

    glEndQuery(GL_TIME_ELAPSED);
    if (aq->primed) {
        unsigned avail = 0;
        glGetQueryObjectuiv(aq->q[aq->qi ^ 1], GL_QUERY_RESULT_AVAILABLE, &avail);
        if (avail) {
            GLuint64 ns = 0;
            glGetQueryObjectui64v(aq->q[aq->qi ^ 1], GL_QUERY_RESULT, &ns);
            ring_push(aq, (double)ns / 1.0e6);
        }
    }
    aq->primed = 1;
    aq->qi ^= 1;
}

int aq_update(AutoQuality *aq, RenderQuality *io)
{
    if (!aq || !aq->enabled) return 0;

    int min_samples = aq->fast ? 8 : 20;
    if (aq->ring_n < min_samples) return 0;

    double tmp[AQ_RING];
    for (int i = 0; i < aq->ring_n; ++i) tmp[i] = aq->ring[i];
    qsort(tmp, (size_t)aq->ring_n, sizeof tmp[0], cmp_double);
    int idx = (int)(0.90 * (aq->ring_n - 1) + 0.5);
    double p90 = tmp[idx];

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    double since = (double)(now.QuadPart - aq->last_change.QuadPart) / (double)aq->freq.QuadPart;
    if (aq->fast) since *= (5.0 / 0.4);   /* comprime a janela de 5 s p/ ~0.4 s no teste */

    AqInput in;
    in.step = aq->step;
    in.ladder_len = render_quality_ladder_len(aq->tier);
    in.p90_ms = p90;
    in.since_change_s = since;

    int ns = aq_decide(in);
    if (ns == aq->step) return 0;

    aq->step = ns;
    QueryPerformanceCounter(&aq->last_change);
    aq->ring_n = 0;
    aq->ring_head = 0;
    *io = render_quality_for_step(&aq->cfg, aq->tier, aq->step);
    return 1;
}
