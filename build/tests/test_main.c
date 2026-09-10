#include "test.h"

int g_test_failures = 0;

void run_cmdline_tests(void);
void run_log_tests(void);
void run_mathx_tests(void);
void run_font_outline_tests(void);

int main(void)
{
    run_cmdline_tests();
    run_log_tests();
    run_mathx_tests();
    run_font_outline_tests();

    if (g_test_failures) {
        printf("\n%d assertion(s) FAILED\n", g_test_failures);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
