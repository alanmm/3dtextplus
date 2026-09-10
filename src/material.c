#include "material.h"
#include "gl_core.h"
#include "util/log.h"

#include <glad/gl.h>
#include "embedded.h"

int material_init(Material *m)
{
    m->prog = gl_program((const char *)EMBED_model_vert, (const char *)EMBED_model_frag);
    if (!m->prog) return 0;
    m->uModel     = glGetUniformLocation(m->prog, "uModel");
    m->uView      = glGetUniformLocation(m->prog, "uView");
    m->uProj      = glGetUniformLocation(m->prog, "uProj");
    m->uCamPos    = glGetUniformLocation(m->prog, "uCamPos");
    m->uBaseColor = glGetUniformLocation(m->prog, "uBaseColor");
    m->uMode      = glGetUniformLocation(m->prog, "uMode");
    m->uMetalness = glGetUniformLocation(m->prog, "uMetalness");
    m->uRoughness = glGetUniformLocation(m->prog, "uRoughness");
    m->uEnvTex    = glGetUniformLocation(m->prog, "uEnvTex");
    m->uHasEnv    = glGetUniformLocation(m->prog, "uHasEnv");
    m->uSdf       = glGetUniformLocation(m->prog, "uSdf");
    m->uSdfMin    = glGetUniformLocation(m->prog, "uSdfMin");
    m->uSdfSize   = glGetUniformLocation(m->prog, "uSdfSize");
    m->uBevelMode = glGetUniformLocation(m->prog, "uBevelMode");
    m->uBevelSize = glGetUniformLocation(m->prog, "uBevelSize");
    m->uHalfDepth = glGetUniformLocation(m->prog, "uHalfDepth");
    glUseProgram(m->prog);
    glUniform1i(m->uEnvTex, 0);   /* unidade de textura 0 */
    glUniform1i(m->uSdf, 1);      /* unidade de textura 1 */
    return 1;
}

void material_begin(const Material *m, m4 view, m4 proj, v3 campos, v3 base)
{
    glUseProgram(m->prog);
    glUniformMatrix4fv(m->uView, 1, GL_FALSE, view.m);
    glUniformMatrix4fv(m->uProj, 1, GL_FALSE, proj.m);
    glUniform3f(m->uCamPos, campos.x, campos.y, campos.z);
    glUniform3f(m->uBaseColor, base.x, base.y, base.z);
}

void material_set_style(const Material *m, int mode, float metalness, float roughness,
                        unsigned env_tex)
{
    glUseProgram(m->prog);
    glUniform1i(m->uMode, mode);
    glUniform1f(m->uMetalness, metalness);
    glUniform1f(m->uRoughness, roughness);
    glUniform1i(m->uHasEnv, env_tex ? 1 : 0);
    if (env_tex) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, env_tex);
    }
}

void material_set_bevel(const Material *m, int bevel_mode, float bevel_size, float half_depth,
                        v2 sdf_min, v2 sdf_size, unsigned sdf_tex)
{
    glUseProgram(m->prog);
    glUniform1i(m->uBevelMode, sdf_tex ? bevel_mode : 2);   /* sem SDF -> trata como off */
    glUniform1f(m->uBevelSize, bevel_size);
    glUniform1f(m->uHalfDepth, half_depth);
    glUniform2f(m->uSdfMin, sdf_min.x, sdf_min.y);
    glUniform2f(m->uSdfSize, sdf_size.x, sdf_size.y);
    if (sdf_tex) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, sdf_tex);
    }
}

void material_set_model(const Material *m, m4 model)
{
    glUniformMatrix4fv(m->uModel, 1, GL_FALSE, model.m);
}

void material_destroy(Material *m)
{
    if (m->prog) glDeleteProgram(m->prog);
    m->prog = 0;
}
