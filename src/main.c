#include <windows.h>
#include "host_win32.h"

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR lpCmdLine, int nShow)
{
    (void)hPrev; (void)lpCmdLine; (void)nShow;
    return host_run_saver(hInst);   /* provisorio: so /s ate o Task 8 */
}
