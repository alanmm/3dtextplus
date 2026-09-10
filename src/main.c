#include <windows.h>
#include "host_win32.h"

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR lpCmdLine, int nShow)
{
    (void)hPrev; (void)lpCmdLine; (void)nShow;
    /* VERIFICACAO TEMPORARIA do Task 4 - substituida no Task 8 */
    extern int m3dt_task4_smoke(HINSTANCE);
    return m3dt_task4_smoke(hInst);
}
