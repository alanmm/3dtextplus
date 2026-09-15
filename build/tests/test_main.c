#include "test.h"

int g_test_failures = 0;

void run_cmdline_tests(void);
void run_log_tests(void);
void run_mathx_tests(void);
void run_font_outline_tests(void);
void run_contour_mesh_tests(void);
void run_config_tests(void);
void run_render_tiers_tests(void);
void run_clockfmt_tests(void);
void run_svg_shapes_tests(void);
void run_mesh_import_tests(void);
void run_i18n_tests(void);
void run_presets_tests(void);

int main(void)
{
    run_cmdline_tests();
    run_log_tests();
    run_mathx_tests();
    run_font_outline_tests();
    run_contour_mesh_tests();
    run_config_tests();
    run_render_tiers_tests();
    run_clockfmt_tests();
    run_svg_shapes_tests();
    run_mesh_import_tests();
    run_i18n_tests();
    run_presets_tests();

    if (g_test_failures) {
        printf("\n%d assertion(s) FAILED\n", g_test_failures);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
