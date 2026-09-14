#ifndef M3DT_PRESETS_H
#define M3DT_PRESETS_H
#include "config.h"
#include "i18n.h"

#define PRESET_NAME_MAX 64
#define BUILTIN_PRESET_COUNT 5

typedef struct {
    StrId name;
    void (*build)(Config *out);
} BuiltinPreset;

extern const BuiltinPreset g_builtin_presets[BUILTIN_PRESET_COUNT];

/* copia so os campos "visuais" (material/geometria-bevel/fundo/
   efeitos/particulas/cor do texto/qualidade) de src pra dst - usada
   tanto pra aplicar um preset em g_work quanto pra extrair de g_work
   na hora de salvar. Ao copiar particles_kind/density/speed/size/
   opacity, tambem sincroniza o par de memoria por tipo correspondente
   (particles_<kind>_density/size/opacity) com os mesmos valores. */
void preset_scope_copy(Config *dst, const Config *src);

/* presets salvos pelo usuario - subchaves de <base>\<nome>. As
   variantes sem sufixo _from usam a base real do produto; as com
   _from aceitam uma base alternativa (testes). */
void preset_user_save(const wchar_t *name, const Config *from);
int  preset_user_load(const wchar_t *name, Config *out);
void preset_user_delete(const wchar_t *name);
int  preset_user_list(wchar_t names[][PRESET_NAME_MAX], int max);

void preset_user_save_to(const wchar_t *base, const wchar_t *name, const Config *from);
int  preset_user_load_from(const wchar_t *base, const wchar_t *name, Config *out);
void preset_user_delete_from(const wchar_t *base, const wchar_t *name);
int  preset_user_list_from(const wchar_t *base, wchar_t names[][PRESET_NAME_MAX], int max);

/* importa/exporta um preset como arquivo .ini (chave=valor, cabecalho
   [Preset]) - reaproveita o saneamento de config_load_from via uma
   subchave temporaria. Retornam 1 em sucesso, 0 em falha. */
int preset_export_file(const wchar_t *path, const Config *from);
int preset_import_file(const wchar_t *path, Config *out);

#endif
