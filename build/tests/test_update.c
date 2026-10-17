#include "test.h"
#include "update_check.h"
#include <windows.h>
#include <string.h>

static int ver_is(const char *s, int a, int b, int c)
{
    int v[3];
    if (!update_parse_version(s, v)) return 0;
    return v[0] == a && v[1] == b && v[2] == c;
}

static int newer(const char *latest, const char *current)
{
    int l[3], c[3];
    if (!update_parse_version(latest, l) || !update_parse_version(current, c)) return -1;
    return update_is_newer(l, c);
}

void run_update_tests(void)
{
    EXPECT(ver_is("v1.2.3", 1, 2, 3));
    EXPECT(ver_is("1.2", 1, 2, 0));
    EXPECT(ver_is("2", 2, 0, 0));
    EXPECT(ver_is("1.0.0.0", 1, 0, 0));          /* FILEVERSION de 4 partes */
    EXPECT(ver_is("V10.20.30", 10, 20, 30));
    EXPECT(ver_is("v1.2.3-beta.1", 1, 2, 3));    /* sufixo ignorado */
    EXPECT(ver_is("  v1.0.1", 1, 0, 1));
    EXPECT(ver_is("1.", 1, 0, 0));
    {
        int v[3];
        EXPECT(!update_parse_version("", v));
        EXPECT(!update_parse_version("abc", v));
        EXPECT(!update_parse_version("v", v));
        EXPECT(!update_parse_version(NULL, v));
    }

    EXPECT(newer("v1.0.1", "1.0.0.0") == 1);
    EXPECT(newer("v1.1.0", "1.0.9") == 1);
    EXPECT(newer("v2.0.0", "1.9.9") == 1);
    EXPECT(newer("v1.0.0", "1.0.0.0") == 0);     /* igual */
    EXPECT(newer("v0.9.9", "1.0.0") == 0);       /* mais antiga */
    EXPECT(newer("v1.10.0", "1.9.0") == 1);      /* comparacao numerica, nao textual */

    char tag[UPDATE_TAG_MAX];
    EXPECT(update_extract_tag("{\"url\":\"x\",\"tag_name\":\"v1.2.3\",\"name\":\"y\"}", tag, sizeof tag));
    EXPECT(strcmp(tag, "v1.2.3") == 0);
    EXPECT(update_extract_tag("{ \"tag_name\" :\n  \"1.0.1\" }", tag, sizeof tag));
    EXPECT(strcmp(tag, "1.0.1") == 0);
    EXPECT(!update_extract_tag("{\"name\":\"y\"}", tag, sizeof tag));            /* sem tag_name */
    EXPECT(!update_extract_tag("{\"tag_name\":\"\"}", tag, sizeof tag));         /* vazia */
    EXPECT(!update_extract_tag("{\"tag_name\":123}", tag, sizeof tag));          /* nao e' string */
    EXPECT(!update_extract_tag("{\"tag_name\":\"v1.0;calc\"}", tag, sizeof tag)); /* char fora da lista */
    EXPECT(!update_extract_tag("{\"tag_name\":\"v1/../x\"}", tag, sizeof tag));
    EXPECT(!update_extract_tag("{\"tag_name\":\"v1\\u0041\"}", tag, sizeof tag)); /* escape */
    EXPECT(!update_extract_tag("{\"tag_name\":\"v1.0.0", tag, sizeof tag));       /* string aberta */
    EXPECT(!update_extract_tag("{\"tag_name\":\"v1.0.0.0.0.0.0\"}", tag, 8));     /* nao cabe */
    EXPECT(!update_extract_tag(NULL, tag, sizeof tag));

    const long long DAY = 86400LL;
    EXPECT(update_due(0, 1000000, 30));                    /* nunca checou */
    EXPECT(!update_due(1000000, 1000000 + 29 * DAY, 30));
    EXPECT(update_due(1000000, 1000000 + 30 * DAY, 30));
    EXPECT(update_due(1000000, 1000000 + 400 * DAY, 30));
    EXPECT(update_due(2000000, 1000000, 30));              /* relogio voltou */

    const wchar_t *TKEY = L"Software\\3DTextPlus_test_update";
    RegDeleteKeyW(HKEY_CURRENT_USER, TKEY);
    EXPECT(update_last_check_load_from(TKEY) == 0);
    update_last_check_save_to(TKEY, 1760000000LL);
    EXPECT(update_last_check_load_from(TKEY) == 1760000000LL);
    update_last_check_save_to(TKEY, 1770000000LL);          /* sobrescreve */
    EXPECT(update_last_check_load_from(TKEY) == 1770000000LL);
    RegDeleteKeyW(HKEY_CURRENT_USER, TKEY);
}
