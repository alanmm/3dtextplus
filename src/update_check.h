#ifndef M3DT_UPDATE_CHECK_H
#define M3DT_UPDATE_CHECK_H
#include <wchar.h>

#define UPDATE_INTERVAL_DAYS 30
#define UPDATE_TAG_MAX 48

/* "v1.2.3", "1.2" ou "1.2.3.4" -> out[3] = {maior, menor, correcao}; sufixos tipo
   "-beta" sao ignorados. Devolve 0 se nao comeca com um numero. */
int update_parse_version(const char *s, int out[3]);
int update_is_newer(const int latest[3], const int current[3]);

/* extrai "tag_name" do JSON da API de releases. So' aceita [0-9A-Za-z._-]: o valor
   vem da rede e vira parte de uma URL aberta no navegador. */
int update_extract_tag(const char *json, char *tag, int cap);

/* 1 se ja passaram `interval_days` desde `last` (ou se nunca checou / o relogio voltou) */
int update_due(long long last, long long now, int interval_days);

/* data da ultima checagem (segundos Unix) - valor cru na chave do produto, fora
   do Config de proposito: o dialogo regrava o Config inteiro ao salvar */
long long update_last_check_load(void);
void      update_last_check_save(long long t);
long long update_last_check_load_from(const wchar_t *key);
void      update_last_check_save_to(const wchar_t *key, long long t);

/* consulta a ultima release publicada (update_net.c). Bloqueante, com timeout:
   chamar de uma thread. 1 = `tag` preenchida. */
int update_fetch_latest_tag(char *tag, int cap);

#endif
