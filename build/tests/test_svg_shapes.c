#include "test.h"
#include "geometry/svg_shapes.h"

#include <windows.h>
#include <string.h>

static void write_temp_svg(const char *content, wchar_t *out_path)
{
    wchar_t dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    GetTempFileNameW(dir, L"svg", 0, out_path);
    HANDLE h = CreateFileW(out_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD written = 0;
    WriteFile(h, content, (DWORD)strlen(content), &written, NULL);
    CloseHandle(h);
}

void run_svg_shapes_tests(void)
{
    /* preenchimento solido: 1 shape -> 1 peca, cor vermelha */
    {
        const char *svg =
            "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
            "<rect x=\"10\" y=\"10\" width=\"80\" height=\"80\" fill=\"#FF0000\"/>"
            "</svg>";
        wchar_t path[MAX_PATH];
        write_temp_svg(svg, path);

        SvgShapeSet ss;
        int ok = svg_shapes_load(path, 0.01f, &ss);
        DeleteFileW(path);

        EXPECT(ok);
        EXPECT(ss.count == 1);
        if (ss.count == 1) {
            EXPECT(ss.pieces[0].cs.count == 1);
            EXPECT(ss.pieces[0].r > 0.9f);
            EXPECT(ss.pieces[0].g < 0.1f);
            EXPECT(ss.pieces[0].b < 0.1f);
        }
        svg_shapes_free(&ss);
    }

    /* furo via evenodd: 2 subpaths no mesmo <path> -> 2 contornos, fill_rule=1 */
    {
        const char *svg =
            "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
            "<path fill-rule=\"evenodd\" fill=\"#00FF00\" "
            "d=\"M10,10 L90,10 L90,90 L10,90 Z M30,30 L30,70 L70,70 L70,30 Z\"/>"
            "</svg>";
        wchar_t path[MAX_PATH];
        write_temp_svg(svg, path);

        SvgShapeSet ss;
        int ok = svg_shapes_load(path, 0.01f, &ss);
        DeleteFileW(path);

        EXPECT(ok);
        EXPECT(ss.count == 1);
        if (ss.count == 1) {
            EXPECT(ss.pieces[0].cs.count == 2);
            EXPECT(ss.pieces[0].cs.fill_rule == 1);
        }
        svg_shapes_free(&ss);
    }

    /* transform: retangulo deslocado ainda parseia como 1 peca/1 contorno */
    {
        const char *svg =
            "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
            "<rect x=\"0\" y=\"0\" width=\"20\" height=\"20\" fill=\"#0000FF\" transform=\"translate(40,40)\"/>"
            "</svg>";
        wchar_t path[MAX_PATH];
        write_temp_svg(svg, path);

        SvgShapeSet ss;
        int ok = svg_shapes_load(path, 0.01f, &ss);
        DeleteFileW(path);

        EXPECT(ok);
        EXPECT(ss.count == 1);
        if (ss.count == 1) {
            EXPECT(ss.pieces[0].cs.count == 1);
            EXPECT(ss.pieces[0].cs.contours[0].count >= 4);
        }
        svg_shapes_free(&ss);
    }

    /* arquivo inexistente -> falha graciosa, sem crash */
    {
        SvgShapeSet ss;
        int ok = svg_shapes_load(L"C:\\caminho\\que\\nao\\existe.svg", 0.01f, &ss);
        EXPECT(!ok);
    }
}
