#include <windows.h>
#include <shellapi.h>
#include "cmdline.h"
#include "host_win32.h"
#include "config_dialog.h"
#include "util/log.h"

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR lpCmdLine, int nShow)
{
    (void)hPrev; (void)lpCmdLine; (void)nShow;

    log_init();

    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    M3dtCmdLine cmd;
    m3dt_cmd_parse(argc, (const wchar_t *const *)argv, &cmd);
    LocalFree(argv);

    log_infof("start mode=%d parent=%llu", (int)cmd.mode, cmd.parent_hwnd);

    int rc = 0;
    switch (cmd.mode) {
        case M3DT_MODE_SAVER:   rc = host_run_saver(hInst); break;
        case M3DT_MODE_PREVIEW: rc = host_run_preview(hInst, (HWND)(UINT_PTR)cmd.parent_hwnd); break;
        case M3DT_MODE_CONFIG:  rc = config_dialog_run(hInst, (HWND)(UINT_PTR)cmd.parent_hwnd); break;
    }

    log_infof("exit rc=%d", rc);
    log_shutdown();
    return rc;
}
