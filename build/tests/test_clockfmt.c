#include "test.h"
#include "util/clockfmt.h"
#include <string.h>

void run_clockfmt_tests(void)
{
    SYSTEMTIME st;
    memset(&st, 0, sizeof st);
    st.wYear = 2026; st.wMonth = 9; st.wDay = 12;
    st.wHour = 14; st.wMinute = 35; st.wSecond = 22;

    char buf_no_date[512], buf_date[512];
    clock_format(st, 0, 1, buf_no_date, sizeof buf_no_date);
    EXPECT(buf_no_date[0] != 0);
    EXPECT(strchr(buf_no_date, '\n') == NULL);   /* sem data = 1 linha */

    clock_format(st, 1, 1, buf_date, sizeof buf_date);
    EXPECT(buf_date[0] != 0);
    EXPECT(strchr(buf_date, '\n') != NULL);       /* com data = 2 linhas */

    char buf_sec[512], buf_nosec[512];
    clock_format(st, 0, 1, buf_sec, sizeof buf_sec);
    clock_format(st, 0, 0, buf_nosec, sizeof buf_nosec);
    EXPECT(strcmp(buf_sec, buf_nosec) != 0);      /* toggle de segundos muda a string */
}
