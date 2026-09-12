#include "util/clockfmt.h"
#include <wchar.h>

void clock_format(SYSTEMTIME st, int show_date, int show_seconds, char *out, int outsz)
{
    wchar_t time_buf[128], date_buf[128], full[300];

    DWORD flags = show_seconds ? 0 : TIME_NOSECONDS;
    if (GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, flags, &st, NULL, time_buf, 128) == 0)
        wcscpy(time_buf, L"--:--");

    if (show_date) {
        if (GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &st, NULL, date_buf, 128, NULL) == 0)
            wcscpy(date_buf, L"----");
        swprintf(full, 300, L"%ls\n%ls", date_buf, time_buf);
    } else {
        swprintf(full, 300, L"%ls", time_buf);
    }

    WideCharToMultiByte(CP_UTF8, 0, full, -1, out, outsz, NULL, NULL);
    out[outsz - 1] = 0;
}
