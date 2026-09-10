#include "test.h"
#include "config.h"

#include <windows.h>
#include <string.h>
#include <math.h>

#define TESTKEY L"Software\\Modern3DText_test"

static int nearf(float a, float b) { return fabsf(a - b) < 1e-3f; }

void run_config_tests(void)
{
    RegDeleteKeyW(HKEY_CURRENT_USER, TESTKEY);

    /* defaults */
    Config d;
    config_defaults(&d);
    EXPECT(strcmp(d.text, "Modern 3D Text") == 0);
    EXPECT(wcscmp(d.font_family, L"Segoe UI") == 0);
    EXPECT(nearf(d.depth, 0.30f));
    EXPECT(d.version == 1);

    /* round-trip */
    Config a;
    config_defaults(&a);
    strcpy(a.text, "Ola Mundo");
    wcscpy(a.font_family, L"Arial");
    a.font_bold = 0;
    a.font_italic = 1;
    a.depth = 0.55f;
    a.max_angle_y = 30.0f;
    a.tilt_x = 12.0f;
    a.period = 6.5f;
    a.base_r = 0.10f; a.base_g = 0.20f; a.base_b = 0.90f;
    a.material_mode = 2;
    a.metalness = 0.4f;
    a.roughness = 0.7f;
    wcscpy(a.env_path, L"C:\\img\\studio.jpg");
    config_save_to(&a, TESTKEY);

    Config b;
    config_load_from(&b, TESTKEY);
    EXPECT(strcmp(b.text, "Ola Mundo") == 0);
    EXPECT(wcscmp(b.font_family, L"Arial") == 0);
    EXPECT(b.font_bold == 0 && b.font_italic == 1);
    EXPECT(nearf(b.depth, 0.55f));
    EXPECT(nearf(b.max_angle_y, 30.0f));
    EXPECT(nearf(b.period, 6.5f));
    EXPECT(fabsf(b.base_b - 0.90f) < 0.01f);   /* cor tem erro de quantizacao 8-bit */
    EXPECT(fabsf(b.base_r - 0.10f) < 0.01f);
    EXPECT(b.material_mode == 2);
    EXPECT(nearf(b.metalness, 0.4f));
    EXPECT(nearf(b.roughness, 0.7f));
    EXPECT(wcscmp(b.env_path, L"C:\\img\\studio.jpg") == 0);

    /* valor ausente -> default; fora de faixa -> clamp; lixo -> default */
    RegDeleteKeyW(HKEY_CURRENT_USER, TESTKEY);
    HKEY k;
    RegCreateKeyExW(HKEY_CURRENT_USER, TESTKEY, 0, NULL, 0, KEY_WRITE, NULL, &k, NULL);
    RegSetValueExW(k, L"depth", 0, REG_SZ, (const BYTE *)L"999", 4 * sizeof(wchar_t));
    RegSetValueExW(k, L"period", 0, REG_SZ, (const BYTE *)L"lixo", 5 * sizeof(wchar_t));
    RegSetValueExW(k, L"material_mode", 0, REG_SZ, (const BYTE *)L"7", 2 * sizeof(wchar_t));
    RegSetValueExW(k, L"metalness", 0, REG_SZ, (const BYTE *)L"5", 2 * sizeof(wchar_t));
    RegCloseKey(k);

    Config c;
    config_load_from(&c, TESTKEY);
    EXPECT(c.depth >= 0.02f && c.depth <= 2.0f);
    EXPECT(nearf(c.period, 9.0f));
    EXPECT(strcmp(c.text, "Modern 3D Text") == 0);
    EXPECT(c.material_mode == 0);              /* 7 fora de 0..3 -> 0 */
    EXPECT(c.metalness >= 0.0f && c.metalness <= 1.0f);   /* 5 -> clamp */

    RegDeleteKeyW(HKEY_CURRENT_USER, TESTKEY);
}
