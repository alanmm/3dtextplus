#include "post.h"
#include "gl_core.h"
#include "util/log.h"

#include <glad/gl.h>
#include <stdlib.h>
#include <string.h>

#include "embedded.h"

#define BLOOM_MIPS 6

struct Post {
    GlFbo hdr_ms;
    GlFbo hdr;
    GlFbo bloom[BLOOM_MIPS];
    unsigned prog_bright, prog_down, prog_up, prog_comp;
    int w, h, samples;
};

static unsigned prog(const unsigned char *fs)
{
    return gl_program((const char *)EMBED_fullscreen_vert, (const char *)fs);
}

Post *post_create(void)
{
    Post *p = (Post *)calloc(1, sizeof *p);
    if (!p) return NULL;
    p->prog_bright = prog(EMBED_post_bright_frag);
    p->prog_down   = prog(EMBED_post_down_frag);
    p->prog_up     = prog(EMBED_post_up_frag);
    p->prog_comp   = prog(EMBED_post_composite_frag);
    if (!p->prog_bright || !p->prog_down || !p->prog_up || !p->prog_comp) {
        post_destroy(p);
        return NULL;
    }
    glUseProgram(p->prog_bright); glUniform1i(glGetUniformLocation(p->prog_bright, "uTex"), 0);
    glUseProgram(p->prog_down);   glUniform1i(glGetUniformLocation(p->prog_down, "uTex"), 0);
    glUseProgram(p->prog_up);     glUniform1i(glGetUniformLocation(p->prog_up, "uTex"), 0);
    glUseProgram(p->prog_comp);
    glUniform1i(glGetUniformLocation(p->prog_comp, "uScene"), 0);
    glUniform1i(glGetUniformLocation(p->prog_comp, "uBloom"), 1);

    int mx = gl_max_samples();
    p->samples = mx >= 4 ? 4 : (mx >= 2 ? 2 : 1);
    return p;
}

static void ensure_size(Post *p, int w, int h)
{
    if (p->w == w && p->h == h && p->hdr_ms.fbo) return;
    p->w = w; p->h = h;

    gl_fbo_free(&p->hdr_ms);
    gl_fbo_free(&p->hdr);
    for (int i = 0; i < BLOOM_MIPS; ++i) gl_fbo_free(&p->bloom[i]);

    p->hdr_ms = gl_fbo_hdr_ms(w, h, p->samples);
    p->hdr = gl_fbo_color16f(w, h, 0);
    int mw = w, mh = h;
    for (int i = 0; i < BLOOM_MIPS; ++i) {
        mw = mw > 1 ? mw / 2 : 1;
        mh = mh > 1 ? mh / 2 : 1;
        p->bloom[i] = gl_fbo_r11f(mw, mh);
    }
    if (!p->hdr_ms.fbo || !p->hdr.fbo) log_errorf("post: FBO HDR incompleto");
}

void post_begin(Post *p, int w, int h)
{
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    ensure_size(p, w, h);
    gl_fbo_bind(&p->hdr_ms);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.02f, 0.03f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

static void set_texel(unsigned program, const GlFbo *src)
{
    glUniform2f(glGetUniformLocation(program, "uTexel"),
                1.0f / (float)src->w, 1.0f / (float)src->h);
}

void post_present(Post *p, int w, int h, PostParams pr)
{
    if (p->w != w || p->h != h) ensure_size(p, w, h);

    gl_blit_resolve(&p->hdr_ms, &p->hdr);

    glDisable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE0);

    if (pr.bloom && pr.intensity > 1e-4f) {
        /* bright-pass -> bloom[0] */
        gl_fbo_bind(&p->bloom[0]);
        glUseProgram(p->prog_bright);
        glUniform1f(glGetUniformLocation(p->prog_bright, "uThreshold"), pr.threshold);
        glUniform1f(glGetUniformLocation(p->prog_bright, "uKnee"), 0.5f * pr.threshold);
        glBindTexture(GL_TEXTURE_2D, p->hdr.color);
        gl_fullscreen_draw();

        /* downsample */
        glUseProgram(p->prog_down);
        for (int i = 1; i < BLOOM_MIPS; ++i) {
            gl_fbo_bind(&p->bloom[i]);
            set_texel(p->prog_down, &p->bloom[i - 1]);
            glBindTexture(GL_TEXTURE_2D, p->bloom[i - 1].color);
            gl_fullscreen_draw();
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
            gl_fullscreen_draw();
        }
        glDisable(GL_BLEND);
    }

    /* composicao -> framebuffer padrao */
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, w, h);
    glUseProgram(p->prog_comp);
    int has_bloom = (pr.bloom && pr.intensity > 1e-4f) ? 1 : 0;
    glUniform1i(glGetUniformLocation(p->prog_comp, "uHasBloom"), has_bloom);
    glUniform1f(glGetUniformLocation(p->prog_comp, "uBloomIntensity"), pr.intensity);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, p->hdr.color);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, has_bloom ? p->bloom[0].color : p->hdr.color);
    gl_fullscreen_draw();
    glActiveTexture(GL_TEXTURE0);

    glEnable(GL_DEPTH_TEST);
}

void post_destroy(Post *p)
{
    if (!p) return;
    gl_fbo_free(&p->hdr_ms);
    gl_fbo_free(&p->hdr);
    for (int i = 0; i < BLOOM_MIPS; ++i) gl_fbo_free(&p->bloom[i]);
    if (p->prog_bright) glDeleteProgram(p->prog_bright);
    if (p->prog_down)   glDeleteProgram(p->prog_down);
    if (p->prog_up)     glDeleteProgram(p->prog_up);
    if (p->prog_comp)   glDeleteProgram(p->prog_comp);
    free(p);
}
