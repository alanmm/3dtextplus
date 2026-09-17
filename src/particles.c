#include "particles.h"
#include "gl_core.h"
#include "util/log.h"
#include "embedded.h"

#include <glad/gl.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <windows.h>

#define PARTICLES_MAX   4000
#define WALL_SAMPLE_MAX 512

typedef struct {
    float x, y, z;
    float vx, vy, vz;
    float age, life;   /* sparks: idade/vida real; stars: age reaproveitado como fase de cintilacao */
    float size;
    float r, g, b;
    float alpha_rand;  /* opacidade base aleatoria por particula (dust/bokeh), 1%..80% */
    float rot;         /* angulo do hexagono (bokeh), aleatorio por particula */
    float blur_seed;   /* 0..1, aleatorio por particula - ver BLUR_RAND_SPREAD em particle.vert */
} Particle;

typedef struct { float x, y, z, size, r, g, b, a, rot, blur_seed; } ParticleVertex;

struct ParticleSystem {
    unsigned prog, vao, vbo;

    Particle       buf[PARTICLES_MAX];
    ParticleVertex gpu_buf[PARTICLES_MAX];
    int            count;

    int   kind;      /* -1 = ainda nao inicializado */
    float density, speed, size_scale, opacity_scale;
    float last_density;  /* densidade usada na ultima semeadura - -1 = nenhuma ainda */
    float box_hx, box_hy, box_hz;
    float fade_dist;   /* distancia de referencia p/ o fade por distancia e o crescimento do bokeh */

    float time;
    float star_angle;
    float emit_accum;
    unsigned seed;

    v3  wall_pos[WALL_SAMPLE_MAX];
    v3  wall_n[WALL_SAMPLE_MAX];
    int wall_count;
};

/* ---------------- ruido em CPU (mesmo estilo do fBm de shaders/background.frag,
   reimplementado aqui porque a simulacao roda em C, nao em GLSL) ---------------- */

static float hashf(unsigned *seed)
{
    *seed = *seed * 1664525u + 1013904223u;
    return (float)(*seed >> 8) / (float)(1u << 24);
}

static float hash21(float x, float y)
{
    float h = sinf(x * 127.1f + y * 311.7f) * 43758.5453f;
    return h - floorf(h);
}

static float value_noise2(float x, float y)
{
    float ix = floorf(x), iy = floorf(y);
    float fx = x - ix, fy = y - iy;
    float a = hash21(ix, iy),          b = hash21(ix + 1.0f, iy);
    float c = hash21(ix, iy + 1.0f),   d = hash21(ix + 1.0f, iy + 1.0f);
    float ux = fx * fx * (3.0f - 2.0f * fx);
    float uy = fy * fy * (3.0f - 2.0f * fy);
    return a + (b - a) * ux + (c - a) * uy + (a - b - c + d) * ux * uy;
}

static void curl2d(float x, float y, float *ox, float *oy)
{
    const float e = 0.06f;
    float n1 = value_noise2(x, y + e), n2 = value_noise2(x, y - e);
    float n3 = value_noise2(x + e, y), n4 = value_noise2(x - e, y);
    *ox =  (n1 - n2) / (2.0f * e);
    *oy = -(n3 - n4) / (2.0f * e);
}

/* h em radianos [0, 2pi); s, l em [0,1]. Conversao HSL->RGB padrao. */
static void hsl_to_rgb(float h, float s, float l, float *r, float *g, float *b)
{
    float hh = h / 6.2831853f;
    hh -= floorf(hh);
    float c = (1.0f - fabsf(2.0f * l - 1.0f)) * s;
    float hp = hh * 6.0f;
    float x = c * (1.0f - fabsf(fmodf(hp, 2.0f) - 1.0f));
    float r1, g1, b1;
    if      (hp < 1.0f) { r1 = c; g1 = x; b1 = 0.0f; }
    else if (hp < 2.0f) { r1 = x; g1 = c; b1 = 0.0f; }
    else if (hp < 3.0f) { r1 = 0.0f; g1 = c; b1 = x; }
    else if (hp < 4.0f) { r1 = 0.0f; g1 = x; b1 = c; }
    else if (hp < 5.0f) { r1 = x; g1 = 0.0f; b1 = c; }
    else                { r1 = c; g1 = 0.0f; b1 = x; }
    float m = l - c * 0.5f;
    *r = r1 + m; *g = g1 + m; *b = b1 + m;
}

/* ---------------- ciclo de vida ---------------- */

ParticleSystem *particles_create(void)
{
    ParticleSystem *p = (ParticleSystem *)calloc(1, sizeof *p);
    if (!p) return NULL;

    p->prog = gl_program((const char *)EMBED_particle_vert, (const char *)EMBED_particle_frag);
    if (!p->prog) { free(p); return NULL; }

    glGenVertexArrays(1, &p->vao);
    glGenBuffers(1, &p->vbo);
    glBindVertexArray(p->vao);
    glBindBuffer(GL_ARRAY_BUFFER, p->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(PARTICLES_MAX * (int)sizeof(ParticleVertex)), NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void *)(4 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void *)(8 * sizeof(float)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void *)(9 * sizeof(float)));
    glBindVertexArray(0);

    p->kind = -1;
    p->last_density = -1.0f;
    /* semente com entropia real - sem isso toda execucao do protetor de
       tela sorteia exatamente a mesma paleta/layout (achado testando esta
       mudanca: duas capturas em execucoes separadas saiam identicas,
       pixel a pixel). QueryPerformanceCounter tem resolucao bem mais fina
       que GetTickCount; o endereco de p adiciona alguma variacao a mais
       entre janelas de monitores diferentes criadas quase juntas. */
    LARGE_INTEGER qpc;
    QueryPerformanceCounter(&qpc);
    p->seed = 0x9E3779B9u ^ (unsigned)qpc.LowPart ^ (unsigned)(size_t)p;
    glEnable(GL_PROGRAM_POINT_SIZE);
    return p;
}

static void spawn_ambient(ParticleSystem *p)
{
    int target = 0;
    float size = 0.05f, r = 1.0f, g = 1.0f, b = 1.0f;
    switch (p->kind) {
        case 0: target = (int)(p->density * 800.0f); size = 0.035f; r = g = b = 1.0f; break;   /* dust */
        case 1: target = (int)(p->density * 60.0f);  size = 0.16f;  break;                     /* bokeh: cor/tamanho abaixo, por particula */
        case 3: target = (int)(p->density * 300.0f); size = 0.03f;  r = 0.85f; g = 0.9f; b = 1.0f; break;   /* stars */
        default: target = 0; break;
    }
    if (target > PARTICLES_MAX) target = PARTICLES_MAX;
    p->count = target;

    /* bokeh: uma faixa de matiz e' sorteada uma vez por semeadura (nao por
       particula) - todas as particulas dessa "sessao" tiram sua cor de
       dentro dessa faixa estreita, dando uma paleta coerente em vez de
       um arco-iris. Reamostrado toda vez que spawn_ambient roda de novo
       (mudanca de densidade/tipo, ou o dialogo reabrindo). */
    const float HUE_BAND_DEG = 40.0f;
    float hue_band_rad = HUE_BAND_DEG * (3.14159265f / 180.0f);
    float palette_hue = hashf(&p->seed) * 6.2831853f;

    for (int i = 0; i < target; ++i) {
        Particle *pt = &p->buf[i];
        pt->x = (hashf(&p->seed) * 2.0f - 1.0f) * p->box_hx;
        pt->y = (hashf(&p->seed) * 2.0f - 1.0f) * p->box_hy;
        pt->z = (hashf(&p->seed) * 2.0f - 1.0f) * p->box_hz;
        pt->vx = pt->vy = pt->vz = 0.0f;
        pt->age = hashf(&p->seed) * 6.2831853f;   /* fase de cintilacao - so' stars usam */
        pt->life = 0.0f;
        pt->alpha_rand = 0.01f + hashf(&p->seed) * 0.79f;   /* 1%..80%, aleatoria por particula */
        if (p->kind == 1) {
            /* bokeh: semente de blur primeiro - ela tambem governa tamanho
               e opacidade abaixo (quanto mais fora de foco, maior e mais
               fraca a particula fica, como um bokeh de verdade), por cima
               da variacao independente que ja existia. */
            pt->blur_seed = hashf(&p->seed);
            pt->size = size * (0.5f + hashf(&p->seed) * 1.0f) * (1.0f + 0.5f * pt->blur_seed);
            pt->alpha_rand *= (1.0f - 0.5f * pt->blur_seed);

            /* cor: HSL bem claro e pouco saturado, dentro da faixa de
               matiz da sessao. */
            float hue = palette_hue + (hashf(&p->seed) - 0.5f) * hue_band_rad;
            /* em HSL a "forca" da cor (chroma) e' (1-|2L-1|)*S - alem disso,
               o bokeh usa blend normal (nao aditivo), entao o resultado
               final na tela e' alpha*corParticula + (1-alpha)*corFundo;
               como o fundo e' escuro-azulado e o alfa costuma ser baixo/
               moderado (teto de 80%, ainda reduzido pelo blur), o azul do
               fundo "dilui" e domina a cor real da particula - confirmado
               amostrando pixels reais: toda particula testada saia com viés
               azul, nao importa o matiz sorteado. L mais baixo da bem mais
               forca de cor pra sobreviver a essa diluicao; ainda fica claro
               (bem acima de 50% = cinza medio), so' nao tao proximo do
               branco quanto antes. */
            float sat = 0.25f + hashf(&p->seed) * 0.25f;    /* 25%..50% */
            float lit = 0.65f + hashf(&p->seed) * 0.17f;    /* 65%..82% */
            hsl_to_rgb(hue, sat, lit, &pt->r, &pt->g, &pt->b);

            pt->rot = hashf(&p->seed) * 6.2831853f;
        } else {
            pt->size = size;
            pt->r = r; pt->g = g; pt->b = b;
            pt->rot = 0.0f;
            pt->blur_seed = 0.0f;
        }
    }
}

void particles_set_config(ParticleSystem *p, const Config *cfg,
                          float hx, float hy, float hz,
                          const v3 *wall_pos, const v3 *wall_n, int wall_count)
{
    int kind_changed = (p->kind != cfg->particles_kind);

    p->density    = cfg->particles_density;
    p->speed      = cfg->particles_speed;
    p->size_scale = cfg->particles_size_scale;
    p->opacity_scale = cfg->particles_opacity;
    p->box_hx = hx * 3.0f + 1.0f;
    p->box_hy = hy * 3.0f + 1.0f;
    p->box_hz = hz * 6.0f + 2.0f;
    p->fade_dist = fmaxf(p->box_hx, fmaxf(p->box_hy, p->box_hz));

    int n = wall_count;
    if (n > WALL_SAMPLE_MAX) n = WALL_SAMPLE_MAX;
    p->wall_count = n;
    if (n > 0) {
        int step = wall_count / n; if (step < 1) step = 1;
        for (int i = 0; i < n; ++i) {
            int src = (i * step) % wall_count;
            p->wall_pos[i] = wall_pos[src];
            p->wall_n[i]   = wall_n[src];
        }
    }

    int density_changed = (p->last_density != cfg->particles_density);

    if (kind_changed) {
        p->kind = cfg->particles_kind;
        p->count = 0;
        p->emit_accum = 0.0f;
        p->star_angle = 0.0f;
        if (p->kind != 2) spawn_ambient(p);   /* sparks (kind 2) comecam vazias, emitem aos poucos */
    } else if ((p->kind == 0 || p->kind == 1 || p->kind == 3) && density_changed) {
        /* so' re-semeia se a densidade realmente mudou - antes rodava em
           TODO sync de config (ou seja, a cada edicao de QUALQUER slider
           do dialogo, ja' que o preview ao vivo resincroniza a config
           inteira a cada tick sujo), teleportando as particulas ambiente
           pra posicoes aleatorias novas o tempo todo e dando uma
           impressao de "particulas aceleradas/erraticas" ao arrastar
           qualquer slider - achado ao investigar reclamacao do usuario de
           que o slider de vies aresta/plano "acelerava particulas". */
        spawn_ambient(p);
    }
    p->last_density = cfg->particles_density;
}

static void update_ambient_drift(ParticleSystem *p, float dt)
{
    for (int i = 0; i < p->count; ++i) {
        Particle *pt = &p->buf[i];
        float cx, cy;
        curl2d(pt->x * 0.25f + p->time * 0.05f, pt->z * 0.25f, &cx, &cy);
        pt->vx = cx * 0.5f;
        pt->vz = cy * 0.5f;
        pt->vy = -0.03f;
        pt->x += pt->vx * dt * p->speed;
        pt->y += pt->vy * dt * p->speed;
        pt->z += pt->vz * dt * p->speed;
        if (pt->x >  p->box_hx) pt->x -= 2.0f * p->box_hx; else if (pt->x < -p->box_hx) pt->x += 2.0f * p->box_hx;
        if (pt->y >  p->box_hy) pt->y -= 2.0f * p->box_hy; else if (pt->y < -p->box_hy) pt->y += 2.0f * p->box_hy;
        if (pt->z >  p->box_hz) pt->z -= 2.0f * p->box_hz; else if (pt->z < -p->box_hz) pt->z += 2.0f * p->box_hz;
    }
}

static void update_stars(ParticleSystem *p, float dt)
{
    const float STAR_ANGULAR_SPEED = 1.5f;   /* graus/seg, em speed=1 */
    p->star_angle += STAR_ANGULAR_SPEED * p->speed * dt;
}

#define SPARK_RATE_MAX  50.0f   /* particulas/seg em density=1 */
#define SPARK_GRAVITY    1.2f
#define SPARK_DRAG       3.0f   /* 1/seg, freia vx/vz - ver comentario em update_sparks */

static void update_sparks(ParticleSystem *p, float dt, m4 model)
{
    for (int i = 0; i < p->count; ) {
        Particle *pt = &p->buf[i];
        pt->age += dt;
        if (pt->age >= pt->life) {
            *pt = p->buf[p->count - 1];
            p->count--;
            continue;
        }
        pt->vy -= SPARK_GRAVITY * dt;
        /* arrasto: sem isso a faisca viaja em linha reta indefinidamente e,
           num objeto pequeno com a camera perto, pode atravessar a camera
           antes de morrer e sumir de vista. Tambem deixa o movimento mais
           parecido com fagulha de solda de verdade (explode e freia, nao
           dispara como projetil). */
        float drag = 1.0f - fminf(SPARK_DRAG * dt, 0.9f);
        pt->vx *= drag;
        pt->vz *= drag;
        pt->x += pt->vx * dt;
        pt->y += pt->vy * dt;
        pt->z += pt->vz * dt;
        ++i;
    }

    if (p->wall_count <= 0) return;
    p->emit_accum += dt * p->density * SPARK_RATE_MAX;
    while (p->emit_accum >= 1.0f && p->count < PARTICLES_MAX) {
        p->emit_accum -= 1.0f;
        int idx = (int)(hashf(&p->seed) * (float)p->wall_count);
        if (idx >= p->wall_count) idx = p->wall_count - 1;

        /* model e' rotacao pura (sem translacao/escala) em scene.c, entao
           tambem serve para transformar a normal - sem precisar de uma
           inversa-transposta separada. */
        v3 wp = m4_mul_point(model, p->wall_pos[idx]);
        v3 wn = m4_mul_point(model, p->wall_n[idx]);

        Particle *pt = &p->buf[p->count++];
        float sp = 1.2f * p->speed;
        pt->x = wp.x; pt->y = wp.y; pt->z = wp.z;
        pt->vx = wn.x * sp;
        pt->vy = wn.y * sp + 0.6f * sp;
        pt->vz = wn.z * sp;
        pt->age = 0.0f;
        pt->life = 0.5f + hashf(&p->seed) * 1.0f;
        pt->size = 0.045f;
        pt->r = 1.0f; pt->g = 0.55f; pt->b = 0.12f;
    }
}

void particles_update(ParticleSystem *p, float dt, m4 model)
{
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.1f) dt = 0.1f;
    p->time += dt;

    switch (p->kind) {
        case 0: case 1: update_ambient_drift(p, dt); break;
        case 2: update_sparks(p, dt, model); break;
        case 3: update_stars(p, dt); break;
        default: break;
    }
}

/* faisca: comeca branco-azulada (metal incandescente) e esfria pra laranja
   na primeira metade da vida; a segunda metade so' encolhe/apaga (alpha e
   tamanho, calculados abaixo). */
static const v3 SPARK_HOT  = { 0.85f, 0.92f, 1.00f };
static const v3 SPARK_COOL = { 1.00f, 0.45f, 0.08f };

void particles_render(ParticleSystem *p, m4 view, m4 proj, int fb_h)
{
    if (p->count <= 0 || !p->prog) return;

    m4 star_rot = m4_rotate_y(m3dt_radians(p->star_angle));
    for (int i = 0; i < p->count; ++i) {
        const Particle *pt = &p->buf[i];
        ParticleVertex *gv = &p->gpu_buf[i];
        v3 pos = { pt->x, pt->y, pt->z };
        if (p->kind == 3) pos = m4_mul_point(star_rot, pos);

        gv->x = pos.x; gv->y = pos.y; gv->z = pos.z;
        gv->size = pt->size * p->size_scale;

        /* dust/bokeh: base 1%..80% aleatoria por particula, escalada pelo
           slider geral de opacidade - aplicado aqui (nao no spawn) pra
           reagir na hora ao mexer no slider, sem esperar re-semear. */
        float alpha = pt->alpha_rand * p->opacity_scale;
        v3 col = { pt->r, pt->g, pt->b };
        if (p->kind == 2) {
            float u = pt->life > 0.0f ? pt->age / pt->life : 1.0f;
            alpha = 1.0f - u;
            gv->size *= (1.0f - 0.5f * u);
            float cool_t = fminf(u * 2.0f, 1.0f);   /* esfria na 1a metade da vida */
            col.x = SPARK_HOT.x + (SPARK_COOL.x - SPARK_HOT.x) * cool_t;
            col.y = SPARK_HOT.y + (SPARK_COOL.y - SPARK_HOT.y) * cool_t;
            col.z = SPARK_HOT.z + (SPARK_COOL.z - SPARK_HOT.z) * cool_t;
        } else if (p->kind == 3) {
            alpha = 0.4f + 0.6f * (0.5f + 0.5f * sinf(p->time * 2.0f + pt->age));
        }
        gv->r = col.x; gv->g = col.y; gv->b = col.z; gv->a = alpha;
        gv->rot = pt->rot;
        gv->blur_seed = pt->blur_seed;
    }

    glBindBuffer(GL_ARRAY_BUFFER, p->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(p->count * (int)sizeof(ParticleVertex)), p->gpu_buf);

    glEnable(GL_BLEND);
    if (p->kind == 1) {
        /* bokeh: blend "normal" (over) - aditivo faz circulos sobrepostos
           saturarem rapido pra um branco solido; alpha normal fica
           translucido de verdade, mais parecido com uma foto de bokeh. */
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);   /* aditivo: poeira/faiscas/estrelas brilham */
    }
    glDepthMask(GL_FALSE);

    glUseProgram(p->prog);
    glUniformMatrix4fv(glGetUniformLocation(p->prog, "uView"), 1, GL_FALSE, view.m);
    glUniformMatrix4fv(glGetUniformLocation(p->prog, "uProj"), 1, GL_FALSE, proj.m);
    /* tamanho do point sprite em pixels precisa escalar com a altura real do
       framebuffer, senao o mesmo tamanho em unidades de mundo fica gigante
       no preview (janela pequena) e minusculo em tela cheia (resolucao
       grande) - proj.m[5] e' 1/tan(fovy/2) (m4_perspective). */
    float pixel_scale = (float)fb_h * proj.m[5] * 0.5f;
    glUniform1f(glGetUniformLocation(p->prog, "uPixelScale"), pixel_scale);
    glUniform1f(glGetUniformLocation(p->prog, "uFadeDist"), p->fade_dist);
    glUniform1i(glGetUniformLocation(p->prog, "uRing"), p->kind == 1 ? 1 : 0);

    glBindVertexArray(p->vao);
    glDrawArrays(GL_POINTS, 0, p->count);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void particles_destroy(ParticleSystem *p)
{
    if (!p) return;
    if (p->vbo) glDeleteBuffers(1, &p->vbo);
    if (p->vao) glDeleteVertexArrays(1, &p->vao);
    if (p->prog) glDeleteProgram(p->prog);
    free(p);
}
