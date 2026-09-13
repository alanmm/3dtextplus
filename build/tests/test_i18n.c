#include "test.h"
#include "i18n.h"
#include <string.h>
#include <wchar.h>

void run_i18n_tests(void)
{
    /* toda chave do enum tem valor real em pt e em em - nunca cai no
       fallback "[chave]" nem fica vazia, nos dois idiomas */
    i18n_init(1);   /* pt */
    for (int i = 0; i < STR_COUNT; ++i) {
        const wchar_t *s = i18n_str((StrId)i);
        EXPECT(s[0] != 0);
        EXPECT(s[0] != L'[');
    }

    i18n_init(2);   /* en */
    for (int i = 0; i < STR_COUNT; ++i) {
        const wchar_t *s = i18n_str((StrId)i);
        EXPECT(s[0] != 0);
        EXPECT(s[0] != L'[');
    }

    /* strings conhecidas batem com o conteudo real dos arquivos */
    i18n_init(1);
    EXPECT(wcscmp(i18n_str(STR_BTN_CANCEL), L"Cancelar") == 0);
    i18n_init(2);
    EXPECT(wcscmp(i18n_str(STR_BTN_CANCEL), L"Cancel") == 0);

    /* auto: se o Windows de teste estiver em pt-BR isso bate com pt,
       senao com en - so' confirma que auto nao trava nem cai no
       fallback, sem assumir o idioma do sistema */
    i18n_init(0);
    EXPECT(i18n_str(STR_BTN_CANCEL)[0] != 0);
    EXPECT(i18n_str(STR_BTN_CANCEL)[0] != L'[');

    /* mensagem multi-linha (\n) decodificada corretamente */
    i18n_init(1);
    const wchar_t *err = i18n_str(STR_ERROR_MESH_INVALID);
    EXPECT(wcschr(err, L'\n') != NULL);
    EXPECT(wcsstr(err, L"\\n") == NULL);   /* nao sobrou \n literal (2 chars) */
}
