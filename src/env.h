#ifndef M3DT_ENV_H
#define M3DT_ENV_H
#include <wchar.h>

/* Carrega uma imagem (equiretangular) como textura GL 2D com mipmaps.
   Retorna o id da textura, ou 0 se path vazio / falha. Requer contexto GL. */
unsigned env_load_texture(const wchar_t *path);
void     env_free(unsigned tex);

#endif
