#include "test.h"
#include "cmdline.h"
#include <wchar.h>

static M3dtCmdLine P(const wchar_t *a1, const wchar_t *a2)
{
    const wchar_t *argv[3] = { L"3DTextPlus.scr", a1, a2 };
    int argc = 1 + (a1 ? 1 : 0) + (a1 && a2 ? 1 : 0);
    M3dtCmdLine c;
    m3dt_cmd_parse(argc, argv, &c);
    return c;
}

void run_cmdline_tests(void)
{
    EXPECT_EQ_INT(P(NULL, NULL).mode,        M3DT_MODE_CONFIG);
    EXPECT_EQ_INT(P(L"/s", NULL).mode,       M3DT_MODE_SAVER);
    EXPECT_EQ_INT(P(L"/S", NULL).mode,       M3DT_MODE_SAVER);
    EXPECT_EQ_INT(P(L"-s", NULL).mode,       M3DT_MODE_SAVER);
    EXPECT_EQ_INT(P(L"/c", NULL).mode,       M3DT_MODE_CONFIG);
    EXPECT_EQ_INT(P(L"/c", NULL).parent_hwnd, 0);
    EXPECT_EQ_INT(P(L"/c:1234", NULL).mode,        M3DT_MODE_CONFIG);
    EXPECT_EQ_INT(P(L"/c:1234", NULL).parent_hwnd, 1234ull);
    EXPECT_EQ_INT(P(L"/p", L"5678").mode,        M3DT_MODE_PREVIEW);
    EXPECT_EQ_INT(P(L"/p", L"5678").parent_hwnd, 5678ull);
    EXPECT_EQ_INT(P(L"/p:9012", NULL).mode,        M3DT_MODE_PREVIEW);
    EXPECT_EQ_INT(P(L"/p:9012", NULL).parent_hwnd, 9012ull);
    EXPECT_EQ_INT(P(L"/p", NULL).mode,   M3DT_MODE_CONFIG);   /* preview invalido -> config */
    EXPECT_EQ_INT(P(L"/x", NULL).mode,   M3DT_MODE_CONFIG);   /* flag desconhecida -> config */
    EXPECT_EQ_INT(P(L"texto", NULL).mode, M3DT_MODE_CONFIG);  /* sem '/' nem '-' -> config */
}
