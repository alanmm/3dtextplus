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

void material_set_model(const Material *m, m4 model)
{
    glUniformMatrix4fv(m->uModel, 1, GL_FALSE, model.m);
}

void material_destroy(Material *m)
{
    if (m->prog) glDeleteProgram(m->prog);
    m->prog = 0;
}
