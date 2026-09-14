#include "env.h"
#include "gl_core.h"
#include "util/log.h"

#include <windows.h>
#include <glad/gl.h>
#include "stb_image.h"

unsigned env_load_texture(const wchar_t *path)
{
    if (!path || !path[0]) return 0;

    char u8[1024];
    WideCharToMultiByte(CP_UTF8, 0, path, -1, u8, (int)sizeof u8, NULL, NULL);

    int w = 0, h = 0, ch = 0;
    unsigned char *px = stbi_load(u8, &w, &h, &ch, 3);
    if (!px) {
        log_errorf("env: nao carregou %s (%s)", u8, stbi_failure_reason());
        return 0;
    }

    unsigned t = gl_texture_2d_rgb8(w, h, px, 1);
    stbi_image_free(px);
    log_infof("env: %s (%dx%d) -> tex %u", u8, w, h, t);
    return t;
}

unsigned env_load_texture_from_memory(const unsigned char *data, int len)
{
    if (!data || len <= 0) return 0;

    int w = 0, h = 0, ch = 0;
    unsigned char *px = stbi_load_from_memory(data, len, &w, &h, &ch, 3);
    if (!px) {
        log_errorf("env: nao carregou imagem embutida (%s)", stbi_failure_reason());
        return 0;
    }

    unsigned t = gl_texture_2d_rgb8(w, h, px, 1);
    stbi_image_free(px);
    log_infof("env: imagem embutida (%dx%d) -> tex %u", w, h, t);
    return t;
}

void env_free(unsigned tex)
{
    if (tex) glDeleteTextures(1, &tex);
}
