#include "particles.h"
#include "gl_core.h"
#include "util/log.h"
#include "embedded.h"

#include <glad/gl.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define PARTICLES_MAX   4000
#define WALL_SAMPLE_MAX 512

typedef struct {
    float x, y, z;
    float vx, vy, vz;
    float age, life;   /* sparks: idade/vida real; stars: age reaproveitado como fase de cintilacao */
    float size;
    float r, g, b;
} Particle;

typedef struct { float x, y, z, size, r, g, b, a; } ParticleVertex;

struct ParticleSystem {
    unsigned prog, vao, vbo;

    Particle       buf[PARTICLES_MAX];
    ParticleVertex gpu_buf[PARTICLES_MAX];
    int            count;

    int   kind;      /* -1 = ainda nao inicializado */
    float density, speed, size_scale;
    float box_hx, box_hy, box_hz;

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
    glBindVertexArray(0);

    p->kind = -1;
    p->seed = 0x9E3779B9u;
    glEnable(GL_PROGRAM_POINT_SIZE);
    return p;
}

static void spawn_ambient(ParticleSystem *p)
{
    int target = 0;
    float size = 0.05f, r = 1.0f, g = 1.0f, b = 1.0f;
    switch (p->kind) {
        case 0: target = (int)(p->density * 800.0f); size = 0.035f; r = g = b = 1.0f; break;               /* dust */
        case 1: target = (int)(p->density * 60.0f);  size = 0.16f;  r = 1.0f; g = 0.95f; b = 0.85f; break;  /* bokeh */
        case 3: target = (int)(p->density * 300.0f); size = 0.03f;  r = 0.85f; g = 0.9f; b = 1.0f; break;   /* stars */
        default: target = 0; break;
    }
    if (target > PARTICLES_MAX) target = PARTICLES_MAX;
    p->count = target;
    for (int i = 0; i < target; ++i) {
        Particle *pt = &p->buf[i];
        pt->x = (hashf(&p->seed) * 2.0f - 1.0f) * p->box_hx;
        pt->y = (hashf(&p->seed) * 2.0f - 1.0f) * p->box_hy;
        pt->z = (hashf(&p->seed) * 2.0f - 1.0f) * p->box_hz;
        pt->vx = pt->vy = pt->vz = 0.0f;
        pt->age = hashf(&p->seed) * 6.2831853f;   /* fase de cintilacao - so' stars usam */
        pt->life = 0.0f;
        pt->size = size;
        pt->r = r; pt->g = g; pt->b = b;
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
    p->box_hx = hx * 3.0f + 1.0f;
    p->box_hy = hy * 3.0f + 1.0f;
    p->box_hz = hz * 6.0f + 2.0f;

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

    if (kind_changed) {
        p->kind = cfg->particles_kind;
        p->count = 0;
        p->emit_accum = 0.0f;
        p->star_angle = 0.0f;
        if (p->kind != 2) spawn_ambient(p);   /* sparks (kind 2) comecam vazias, emitem aos poucos */
    } else if (p->kind == 0 || p->kind == 1 || p->kind == 3) {
        spawn_ambient(p);   /* densidade pode ter mudado sem trocar de tipo - re-semeia */
    }
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
        pt->size = 0.02f;
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

void particles_render(ParticleSystem *p, m4 view, m4 proj)
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

        float alpha = 1.0f;
        if (p->kind == 2) {
            float u = pt->life > 0.0f ? pt->age / pt->life : 1.0f;
            alpha = 1.0f - u;
            gv->size *= (1.0f - 0.5f * u);
        } else if (p->kind == 3) {
            alpha = 0.4f + 0.6f * (0.5f + 0.5f * sinf(p->time * 2.0f + pt->age));
        }
        gv->r = pt->r; gv->g = pt->g; gv->b = pt->b; gv->a = alpha;
    }

    glBindBuffer(GL_ARRAY_BUFFER, p->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(p->count * (int)sizeof(ParticleVertex)), p->gpu_buf);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);

    glUseProgram(p->prog);
    glUniformMatrix4fv(glGetUniformLocation(p->prog, "uView"), 1, GL_FALSE, view.m);
    glUniformMatrix4fv(glGetUniformLocation(p->prog, "uProj"), 1, GL_FALSE, proj.m);
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
