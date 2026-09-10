#include "config_dialog.h"
#include "resource.h"
#include "util/log.h"

#include <commctrl.h>
#include <stdbool.h>

#define M3DT_SELFTEST_TIMER 1

static bool selftest_flag(void)
{
    char buf[8];
    DWORD k = GetEnvironmentVariableA("M3DT_SELFTEST", buf, sizeof buf);
    return k > 0 && k < sizeof buf && buf[0] != '0';
}

static INT_PTR CALLBACK dlg_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    (void)l;
    switch (m) {
        case WM_INITDIALOG:
            if (selftest_flag())
                SetTimer(h, M3DT_SELFTEST_TIMER, 800, NULL);
            return TRUE;
        case WM_TIMER:
            if (w == M3DT_SELFTEST_TIMER) {
                KillTimer(h, M3DT_SELFTEST_TIMER);
                EndDialog(h, IDCANCEL);
                return TRUE;
            }
            break;
        case WM_COMMAND:
            if (LOWORD(w) == IDOK || LOWORD(w) == IDCANCEL) {
                EndDialog(h, (INT_PTR)LOWORD(w));
                return TRUE;
            }
            break;
        case WM_CLOSE:
            EndDialog(h, IDCANCEL);
            return TRUE;
    }
    return FALSE;
}

int config_dialog_run(HINSTANCE hInst, HWND parent)
{
    INITCOMMONCONTROLSEX icc = { sizeof icc, ICC_STANDARD_CLASSES | ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    HWND owner = IsWindow(parent) ? parent : NULL;
    log_infof("config: abrindo dialogo (owner=%p)", (void *)owner);

    INT_PTR r = DialogBoxParamW(hInst, MAKEINTRESOURCEW(IDD_CONFIG), owner, dlg_proc, 0);
    log_infof("config: dialogo fechou (r=%lld)", (long long)r);
    return 0;
}
