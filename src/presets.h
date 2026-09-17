#ifndef M3DT_PRESETS_H
#define M3DT_PRESETS_H
#include "config.h"
#include "i18n.h"

#define PRESET_NAME_MAX 64
#define BUILTIN_PRESET_COUNT 6
#define PRESET_BACKUP_MAX 64

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

typedef struct {
    wchar_t name[PRESET_NAME_MAX];
    Config  cfg;
} PresetBackupEntry;

/* backup/restauracao de TODOS os presets salvos (nao inclui os 5
   fixos) num unico arquivo .ini, uma secao [Preset:Nome] por preset -
   reaproveita o saneamento de config_load_from via uma subchave de
   registro temporaria, uma secao por vez. preset_backup_export_file
   retorna 1 em sucesso, 0 em falha ao abrir o arquivo.
   preset_backup_parse_file retorna quantos presets leu (0 se o
   arquivo nao tinha nenhuma secao valida), ou -1 se nao conseguiu
   abrir o arquivo. A variante _from segue a mesma convencao do CRUD
   acima (base alternativa pra testes); a sem sufixo usa a base real. */
int preset_backup_export_file(const wchar_t *path);
int preset_backup_export_file_from(const wchar_t *base, const wchar_t *path);
int preset_backup_parse_file(const wchar_t *path, PresetBackupEntry *out, int max);

#endif
