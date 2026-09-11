#include "post.h"
#include "gl_core.h"
#include "util/log.h"

#include <glad/gl.h>
#include <stdlib.h>
#include <string.h>

#include "embedded.h"

#define BLOOM_MIPS 6

struct Post {
    GlFbo hdr_ms;                 /* alvo da cena quando ms_on; senao nao usado */
    GlFbo hdr;                    /* single-sample: destino do resolve / alvo da cena */
    GlFbo bloom[BLOOM_MIPS];
    GlFbo streak_src;             /* bright-pass em 1/4 res */
    GlFbo streak_a, streak_b;     /* ping-pong do blur, 1/4 res */
    GlFbo streaks_acc;            /* acumulador dos eixos, 1/4 res */
    GlFbo comp;                   /* HDR combinado (cena+bloom+streaks), out_w x out_h */
    GlFbo ldr;                    /* pos-tonemap pre-FXAA, out_w x out_h */
    unsigned prog_bright, prog_down, prog_up, prog_combine;
    unsigned prog_streak;
    unsigned prog_finish, prog_fxaa;
    unsigned fs_vao;               /* VAO do quad fullscreen - por-contexto, ver gl_fullscreen_draw */
    int in_w, in_h, samples, ms_on;
    int out_w, out_h;
};

static unsigned prog(const unsigned char *fs)
{
    return gl_program((const char *)EMBED_fullscreen_vert, (const char *)fs);
}

Post *post_create(void)
{
    Post *p = (Post *)calloc(1, sizeof *p);
    if (!p) return NULL;
    p->prog_bright  = prog(EMBED_post_bright_frag);
    p->prog_down    = prog(EMBED_post_down_frag);
    p->prog_up      = prog(EMBED_post_up_frag);
    p->prog_combine = prog(EMBED_post_combine_frag);
    p->prog_streak  = prog(EMBED_post_streak_frag);
    p->prog_finish  = prog(EMBED_post_finish_frag);
    p->prog_fxaa    = prog(EMBED_post_fxaa_frag);
    if (!p->prog_bright || !p->prog_down || !p->prog_up || !p->prog_combine ||
        !p->prog_streak || !p->prog_finish || !p->prog_fxaa) {
        post_destroy(p);
        return NULL;
    }
    glUseProgram(p->prog_bright); glUniform1i(glGetUniformLocation(p->prog_bright, "uTex"), 0);
    glUseProgram(p->prog_down);   glUniform1i(glGetUniformLocation(p->prog_down, "uTex"), 0);
    glUseProgram(p->prog_up);     glUniform1i(glGetUniformLocation(p->prog_up, "uTex"), 0);
    glUseProgram(p->prog_streak); glUniform1i(glGetUniformLocation(p->prog_streak, "uTex"), 0);
    glUseProgram(p->prog_combine);
    glUniform1i(glGetUniformLocation(p->prog_combine, "uScene"), 0);
    glUniform1i(glGetUniformLocation(p->prog_combine, "uBloom"), 1);
    glUniform1i(glGetUniformLocation(p->prog_combine, "uStreaks"), 2);
    glUseProgram(p->prog_finish);
    glUniform1i(glGetUniformLocation(p->prog_finish, "uTex"), 0);
    glUseProgram(p->prog_fxaa);
    glUniform1i(glGetUniformLocation(p->prog_fxaa, "uTex"), 0);

    return p;
}

static void ensure_size(Post *p, int w, int h, int samples)
{
    int ms_on = samples >= 2;
    if (p->in_w == w && p->in_h == h && p->samples == samples && p->hdr.fbo) return;
    p->in_w = w; p->in_h = h; p->samples = samples; p->ms_on = ms_on;

    gl_fbo_free(&p->hdr_ms);
    gl_fbo_free(&p->hdr);
    for (int i = 0; i < BLOOM_MIPS; ++i) gl_fbo_free(&p->bloom[i]);
    gl_fbo_free(&p->streak_src); gl_fbo_free(&p->streak_a);
    gl_fbo_free(&p->streak_b);   gl_fbo_free(&p->streaks_acc);

    if (ms_on) {
        p->hdr_ms = gl_fbo_hdr_ms(w, h, samples);   /* alvo da cena */
        p->hdr = gl_fbo_color16f(w, h, 0);          /* destino do resolve */
    } else {
        p->hdr = gl_fbo_color16f(w, h, 1);          /* alvo da cena (com depth) */
    }
    int mw = w, mh = h;
    for (int i = 0; i < BLOOM_MIPS; ++i) {
        mw = mw > 1 ? mw / 2 : 1;
        mh = mh > 1 ? mh / 2 : 1;
        p->bloom[i] = gl_fbo_r11f(mw, mh);
    }
    int sw = w / 4 > 1 ? w / 4 : 1;
    int sh = h / 4 > 1 ? h / 4 : 1;
    p->streak_src  = gl_fbo_r11f(sw, sh);
    p->streak_a    = gl_fbo_r11f(sw, sh);
    p->streak_b    = gl_fbo_r11f(sw, sh);
    p->streaks_acc = gl_fbo_r11f(sw, sh);
    if (!p->hdr.fbo || (ms_on && !p->hdr_ms.fbo)) log_errorf("post: FBO HDR incompleto");
}

static void ensure_output_size(Post *p, int w, int h)
{
    if (p->out_w == w && p->out_h == h && p->comp.fbo) return;
    p->out_w = w; p->out_h = h;
    gl_fbo_free(&p->comp);
    gl_fbo_free(&p->ldr);
    p->comp = gl_fbo_color16f(w, h, 0);
    p->ldr  = gl_fbo_color16f(w, h, 0);
    if (!p->comp.fbo || !p->ldr.fbo) log_errorf("post: FBO combine/ldr incompleto");
}

void post_begin(Post *p, int in_w, int in_h, int samples)
{
    if (in_w < 1) in_w = 1;
    if (in_h < 1) in_h = 1;
    if (samples > 8) samples = 8;
    if (samples < 0) samples = 0;
    ensure_size(p, in_w, in_h, samples);
    gl_fbo_bind(p->ms_on ? &p->hdr_ms : &p->hdr);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.02f, 0.03f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

static void set_texel(unsigned program, const GlFbo *src)
{
    glUniform2f(glGetUniformLocation(program, "uTexel"),
                1.0f / (float)src->w, 1.0f / (float)src->h);
}

void post_present(Post *p, int out_w, int out_h, PostParams pr)
{
    if (out_w < 1) out_w = 1;
    if (out_h < 1) out_h = 1;
    ensure_output_size(p, out_w, out_h);

    if (p->ms_on) gl_blit_resolve(&p->hdr_ms, &p->hdr);   /* -> p->hdr (single-sample) */

    glDisable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE0);

    int has_bloom   = (pr.bloom && pr.intensity > 1e-4f) ? 1 : 0;
    int has_streaks = (pr.streaks_mode != 0 && pr.streaks_intensity > 1e-4f) ? 1 : 0;

    if (has_bloom) {
        /* bright-pass -> bloom[0] */
        gl_fbo_bind(&p->bloom[0]);
        glUseProgram(p->prog_bright);
        glUniform1f(glGetUniformLocation(p->prog_bright, "uThreshold"), pr.threshold);
        glUniform1f(glGetUniformLocation(p->prog_bright, "uKnee"), 0.5f * pr.threshold);
        glBindTexture(GL_TEXTURE_2D, p->hdr.color);
        gl_fullscreen_draw(&p->fs_vao);

        /* downsample */
        glUseProgram(p->prog_down);
        for (int i = 1; i < BLOOM_MIPS; ++i) {
            gl_fbo_bind(&p->bloom[i]);
            set_texel(p->prog_down, &p->bloom[i - 1]);
            glBindTexture(GL_TEXTURE_2D, p->bloom[i - 1].color);
            gl_fullscreen_draw(&p->fs_vao);
        }

        /* upsample aditivo */
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glUseProgram(p->prog_up);
        glUniform1f(glGetUniformLocation(p->prog_up, "uRadius"), pr.radius);
        for (int i = BLOOM_MIPS - 2; i >= 0; --i) {
            gl_fbo_bind(&p->bloom[i]);
            set_texel(p->prog_up, &p->bloom[i + 1]);
            glBindTexture(GL_TEXTURE_2D, p->bloom[i + 1].color);
            gl_fullscreen_draw(&p->fs_vao);
        }
        glDisable(GL_BLEND);
    }

    if (has_streaks) {
        /* bright-pass da cena -> streak_src (1/4 res) */
        gl_fbo_bind(&p->streak_src);
        glUseProgram(p->prog_bright);
        glUniform1f(glGetUniformLocation(p->prog_bright, "uThreshold"), pr.threshold);
        glUniform1f(glGetUniformLocation(p->prog_bright, "uKnee"), 0.5f * pr.threshold);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, p->hdr.color);
        gl_fullscreen_draw(&p->fs_vao);

        /* limpa o acumulador */
        gl_fbo_bind(&p->streaks_acc);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        /* eixos: starburst = 3 (0, 60, 120 graus -> estrela de 6 pontas);
           anamorfico = 1 (horizontal) */
        static const float AX3[3][2] = {
            { 1.0f, 0.0f }, { 0.5f, 0.8660254f }, { -0.5f, 0.8660254f }
        };
        static const float TINT_WHITE[3] = { 1.0f, 1.0f, 1.0f };
        static const float TINT_ANAMO[3] = { 0.55f, 0.72f, 1.0f };
        int          naxes     = (pr.streaks_mode == 1) ? 3 : 1;
        float        base_step = (pr.streaks_mode == 2) ? 1.7f : 1.0f;
        /* len: sem reescala por modo — o alcance do anamorfico ja e controlado
           por base_step (acima); um reescalonamento aqui reintroduziria o
           banding que as correcoes desta fase eliminaram (ver post_streak.frag). */
        float        len       = pr.streaks_length;
        const float *tint      = (pr.streaks_mode == 2) ? TINT_ANAMO : TINT_WHITE;

        glUseProgram(p->prog_streak);
        set_texel(p->prog_streak, &p->streak_src);   /* src/a/b/acc tem o mesmo tamanho */
        glUniform1f(glGetUniformLocation(p->prog_streak, "uLength"), len);
        glUniform3fv(glGetUniformLocation(p->prog_streak, "uTint"), 1, tint);

        for (int a = 0; a < naxes; ++a) {
            float dx = (pr.streaks_mode == 2) ? 1.0f : AX3[a][0];
            float dy = (pr.streaks_mode == 2) ? 0.0f : AX3[a][1];
            glUniform2f(glGetUniformLocation(p->prog_streak, "uDir"), dx, dy);

            const GlFbo *in = &p->streak_src;
            const GlFbo *ping[2] = { &p->streak_a, &p->streak_b };
            float steps[3] = { base_step, base_step * 3.0f, base_step * 9.0f };
            for (int it = 0; it < 3; ++it) {
                int last = (it == 2);
                if (last) {
                    gl_fbo_bind(&p->streaks_acc);
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_ONE, GL_ONE);
                } else {
                    gl_fbo_bind(ping[it & 1]);
                    glDisable(GL_BLEND);
                }
                glUniform1f(glGetUniformLocation(p->prog_streak, "uStep"), steps[it]);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, in->color);
                gl_fullscreen_draw(&p->fs_vao);
                in = last ? &p->streaks_acc : ping[it & 1];
            }
        }
        glDisable(GL_BLEND);
    }

    /* combinar -> p->comp (HDR, out_w x out_h; faz upscale se interno < saida) */
    gl_fbo_bind(&p->comp);
    glUseProgram(p->prog_combine);
    glUniform1i(glGetUniformLocation(p->prog_combine, "uHasBloom"), has_bloom);
    glUniform1f(glGetUniformLocation(p->prog_combine, "uBloomIntensity"), pr.intensity);
    glUniform1i(glGetUniformLocation(p->prog_combine, "uHasStreaks"), has_streaks);
    glUniform1f(glGetUniformLocation(p->prog_combine, "uStreaksIntensity"), pr.streaks_intensity);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, p->hdr.color);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, has_bloom ? p->bloom[0].color : p->hdr.color);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, has_streaks ? p->streaks_acc.color : p->hdr.color);
    gl_fullscreen_draw(&p->fs_vao);

    /* finalizar: CA + vinheta + tonemap ACES + gamma. Sem FXAA -> escreve
       direto no framebuffer padrao; com FXAA -> escreve em p->ldr, o FXAA
       resolve pro framebuffer padrao depois. */
    int has_chroma   = (pr.chroma_on   && pr.chroma_strength  > 1e-4f) ? 1 : 0;
    int has_vignette = (pr.vignette_on && pr.vignette_amount  > 1e-4f) ? 1 : 0;
    int has_fxaa     = pr.fxaa_on ? 1 : 0;

    if (has_fxaa) {
        gl_fbo_bind(&p->ldr);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, out_w, out_h);
    }
    glUseProgram(p->prog_finish);
    glUniform1i(glGetUniformLocation(p->prog_finish, "uHasChroma"), has_chroma);
    glUniform1f(glGetUniformLocation(p->prog_finish, "uChromaStrength"), pr.chroma_strength);
    glUniform1i(glGetUniformLocation(p->prog_finish, "uHasVignette"), has_vignette);
    glUniform1f(glGetUniformLocation(p->prog_finish, "uVignetteAmount"), pr.vignette_amount);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, p->comp.color);
    gl_fullscreen_draw(&p->fs_vao);

    /* FXAA opcional: p->ldr -> framebuffer padrao */
    if (has_fxaa) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, out_w, out_h);
        glUseProgram(p->prog_fxaa);
        set_texel(p->prog_fxaa, &p->ldr);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, p->ldr.color);
        gl_fullscreen_draw(&p->fs_vao);
    }

    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_DEPTH_TEST);
}

void post_destroy(Post *p)
{
    if (!p) return;
    gl_fbo_free(&p->hdr_ms);
    gl_fbo_free(&p->hdr);
    for (int i = 0; i < BLOOM_MIPS; ++i) gl_fbo_free(&p->bloom[i]);
    gl_fbo_free(&p->streak_src);
    gl_fbo_free(&p->streak_a);
    gl_fbo_free(&p->streak_b);
    gl_fbo_free(&p->streaks_acc);
    gl_fbo_free(&p->comp);
    gl_fbo_free(&p->ldr);
    if (p->prog_bright)  glDeleteProgram(p->prog_bright);
    if (p->prog_down)    glDeleteProgram(p->prog_down);
    if (p->prog_up)      glDeleteProgram(p->prog_up);
    if (p->prog_combine) glDeleteProgram(p->prog_combine);
    if (p->prog_streak)  glDeleteProgram(p->prog_streak);
    if (p->prog_finish)  glDeleteProgram(p->prog_finish);
    if (p->prog_fxaa)    glDeleteProgram(p->prog_fxaa);
    free(p);
}
