#include "test.h"
#include "util/log.h"

void run_log_tests(void)
{
    char b[128];
    log_format_line(b, (int)sizeof b, "INFO", "hello world",
                    2026, 9, 10, 8, 5, 3);
    EXPECT_STR(b, "2026-09-10 08:05:03 [INFO] hello world\n");

    /* truncamento seguro: n pequeno nao estoura o buffer */
    char small[10];
    log_format_line(small, (int)sizeof small, "ERROR", "xxxxxxxxxxxxxxxx",
                    2026, 1, 2, 3, 4, 5);
    EXPECT(small[9] == '\0' || small[8] == '\0');
}
