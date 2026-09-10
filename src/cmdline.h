#ifndef M3DT_CMDLINE_H
#define M3DT_CMDLINE_H

#include <wchar.h>

typedef enum {
    M3DT_MODE_CONFIG  = 0,
    M3DT_MODE_SAVER   = 1,
    M3DT_MODE_PREVIEW = 2
} M3dtRunMode;

typedef struct {
    M3dtRunMode        mode;
    unsigned long long parent_hwnd;   /* valor cru; convertido para HWND no host */
} M3dtCmdLine;

/* argc/argv no estilo CommandLineToArgvW: argv[0] = caminho do exe. */
void m3dt_cmd_parse(int argc, const wchar_t *const *argv, M3dtCmdLine *out);

#endif
