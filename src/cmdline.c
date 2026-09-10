#include "cmdline.h"
#include <wctype.h>
#include <stdlib.h>

void m3dt_cmd_parse(int argc, const wchar_t *const *argv, M3dtCmdLine *out)
{
    out->mode = M3DT_MODE_CONFIG;
    out->parent_hwnd = 0;

    if (argc < 2 || !argv || !argv[1] || !argv[1][0])
        return;

    const wchar_t *a = argv[1];
    if (a[0] != L'/' && a[0] != L'-')
        return;                         /* nao e flag -> config */

    wchar_t flag = (wchar_t)towlower((wint_t)a[1]);
    if (!flag)
        return;

    /* numero embutido: "/p:1234" ou "/p1234" */
    const wchar_t *inln = NULL;
    if (a[2] == L':')      inln = a + 3;
    else if (a[2])         inln = a + 2;

    const wchar_t *hs = inln;
    if (!hs && (flag == L'p' || flag == L'c') && argc >= 3 && argv[2] && argv[2][0])
        hs = argv[2];                   /* numero como argumento separado */

    unsigned long long hv = hs ? wcstoull(hs, NULL, 10) : 0ull;

    switch (flag) {
        case L's':
            out->mode = M3DT_MODE_SAVER;
            break;
        case L'p':
            out->parent_hwnd = hv;
            out->mode = hv ? M3DT_MODE_PREVIEW : M3DT_MODE_CONFIG;
            break;
        case L'c':
            out->parent_hwnd = hv;
            out->mode = M3DT_MODE_CONFIG;
            break;
        default:
            out->mode = M3DT_MODE_CONFIG;
            break;
    }
}
