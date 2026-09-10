/* embed <saida.h> <arquivo> [arquivo...]
   Gera um header com o conteudo de cada arquivo como array de bytes (com \0 final). */
#include <stdio.h>
#include <ctype.h>

static void sym_from_path(const char *path, char *out, int n)
{
    const char *base = path;
    for (const char *p = path; *p; ++p)
        if (*p == '/' || *p == '\\') base = p + 1;
    int j = 0;
    for (const char *p = base; *p && j < n - 1; ++p)
        out[j++] = isalnum((unsigned char)*p) ? *p : '_';
    out[j] = 0;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "uso: embed saida.h arquivo...\n"); return 2; }

    FILE *o = fopen(argv[1], "wb");
    if (!o) { fprintf(stderr, "nao abriu %s\n", argv[1]); return 1; }

    fprintf(o, "/* AUTO-GERADO por build/tools/embed.c - nao editar */\n");
    fprintf(o, "#ifndef M3DT_EMBEDDED_H\n#define M3DT_EMBEDDED_H\n\n");

    for (int a = 2; a < argc; ++a) {
        FILE *f = fopen(argv[a], "rb");
        if (!f) { fprintf(stderr, "nao abriu %s\n", argv[a]); fclose(o); return 1; }

        char sym[128];
        sym_from_path(argv[a], sym, (int)sizeof sym);

        fprintf(o, "static const unsigned char EMBED_%s[] = {\n", sym);
        int c, n = 0, col = 0;
        while ((c = fgetc(f)) != EOF) {
            fprintf(o, "%d,", c);
            n++;
            if (++col == 20) { fputc('\n', o); col = 0; }
        }
        fprintf(o, "0 };\n");
        fprintf(o, "static const int EMBED_%s_len = %d;\n\n", sym, n);
        fclose(f);
    }

    fprintf(o, "#endif\n");
    fclose(o);
    return 0;
}
