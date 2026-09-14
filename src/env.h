#ifndef M3DT_ENV_H
#define M3DT_ENV_H
#include <wchar.h>

/* Carrega uma imagem (equiretangular) como textura GL 2D com mipmaps.
   Retorna o id da textura, ou 0 se path vazio / falha. Requer contexto GL. */
unsigned env_load_texture(const wchar_t *path);

/* Carrega uma imagem (equiretangular) de um buffer ja em memoria
   (ex.: dado embutido no binario) como textura GL 2D. Retorna o id
   da textura, ou 0 se falha. Requer contexto GL. */
unsigned env_load_texture_from_memory(const unsigned char *data, int len);

void     env_free(unsigned tex);

#endif
