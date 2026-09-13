#include "test.h"
#include "geometry/mesh_import.h"

#include <windows.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void write_text_file(const char *content, const wchar_t *ext, wchar_t *out_path)
{
    wchar_t dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    GetTempFileNameW(dir, L"tmp", 0, out_path);
    /* GetTempFileNameW sempre cria com extensao .tmp - troca pra extensao pedida,
       ja que mesh_import_load decide o parser pela extensao do caminho */
    size_t n = wcslen(out_path);
    wcsncpy(out_path + n - 3, ext, 3);

    HANDLE h = CreateFileW(out_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD written = 0;
    WriteFile(h, content, (DWORD)strlen(content), &written, NULL);
    CloseHandle(h);
}

static void write_binary_stl_triangle(wchar_t *out_path)
{
    wchar_t dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    GetTempFileNameW(dir, L"tmp", 0, out_path);
    size_t n = wcslen(out_path);
    wcsncpy(out_path + n - 3, L"stl", 3);

    unsigned char buf[84 + 50];
    memset(buf, 0, 80);
    unsigned int ntris = 1;
    memcpy(buf + 80, &ntris, 4);

    float rec[12] = {
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f
    };
    memcpy(buf + 84, rec, 48);
    buf[84 + 48] = 0; buf[84 + 49] = 0;

    HANDLE h = CreateFileW(out_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD written = 0;
    WriteFile(h, buf, sizeof buf, &written, NULL);
    CloseHandle(h);
}

static int nearf(float a, float b) { return fabsf(a - b) < 1e-3f; }

static void write_obj_with_mtl(const char *obj_body_fmt, const char *mtl_content,
                               wchar_t *out_obj_path, wchar_t *out_mtl_path)
{
    wchar_t dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    GetTempFileNameW(dir, L"tmp", 0, out_obj_path);
    size_t n = wcslen(out_obj_path);
    wcsncpy(out_obj_path + n - 3, L"obj", 3);

    wcscpy(out_mtl_path, out_obj_path);
    wcsncpy(out_mtl_path + n - 3, L"mtl", 3);

    wchar_t *base = wcsrchr(out_mtl_path, L'\\');
    char mtl_name[MAX_PATH];
    WideCharToMultiByte(CP_UTF8, 0, base ? base + 1 : out_mtl_path, -1, mtl_name, MAX_PATH, NULL, NULL);

    char obj_content[2048];
    snprintf(obj_content, sizeof obj_content, obj_body_fmt, mtl_name);

    HANDLE h1 = CreateFileW(out_mtl_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD written = 0;
    WriteFile(h1, mtl_content, (DWORD)strlen(mtl_content), &written, NULL);
    CloseHandle(h1);

    HANDLE h2 = CreateFileW(out_obj_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h2, obj_content, (DWORD)strlen(obj_content), &written, NULL);
    CloseHandle(h2);
}

static void write_glb(const char *json, const void *bin, size_t bin_len, wchar_t *out_path)
{
    wchar_t dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    GetTempFileNameW(dir, L"tmp", 0, out_path);
    size_t n = wcslen(out_path);
    wcsncpy(out_path + n - 3, L"glb", 3);

    size_t json_len = strlen(json);
    size_t json_pad = (4 - (json_len % 4)) % 4;
    size_t bin_pad = bin_len > 0 ? (4 - (bin_len % 4)) % 4 : 0;

    unsigned int magic = 0x46546C67, version = 2;
    unsigned int total_len = (unsigned int)(12 + 8 + json_len + json_pad
                                            + (bin_len > 0 ? 8 + bin_len + bin_pad : 0));
    unsigned char header[12];
    memcpy(header + 0, &magic, 4);
    memcpy(header + 4, &version, 4);
    memcpy(header + 8, &total_len, 4);

    unsigned int json_chunk_len = (unsigned int)(json_len + json_pad);
    unsigned int json_chunk_type = 0x4E4F534A;
    unsigned char json_chunk_header[8];
    memcpy(json_chunk_header + 0, &json_chunk_len, 4);
    memcpy(json_chunk_header + 4, &json_chunk_type, 4);

    HANDLE h = CreateFileW(out_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD written = 0;
    WriteFile(h, header, 12, &written, NULL);
    WriteFile(h, json_chunk_header, 8, &written, NULL);
    WriteFile(h, json, (DWORD)json_len, &written, NULL);
    for (size_t i = 0; i < json_pad; ++i) { char sp = ' '; WriteFile(h, &sp, 1, &written, NULL); }

    if (bin_len > 0) {
        unsigned int bin_chunk_len = (unsigned int)(bin_len + bin_pad);
        unsigned int bin_chunk_type = 0x004E4942;
        unsigned char bin_chunk_header[8];
        memcpy(bin_chunk_header + 0, &bin_chunk_len, 4);
        memcpy(bin_chunk_header + 4, &bin_chunk_type, 4);
        WriteFile(h, bin_chunk_header, 8, &written, NULL);
        WriteFile(h, bin, (DWORD)bin_len, &written, NULL);
        for (size_t i = 0; i < bin_pad; ++i) { char z = 0; WriteFile(h, &z, 1, &written, NULL); }
    }
    CloseHandle(h);
}

static void write_big_dummy_file(const wchar_t *ext, size_t total_bytes, wchar_t *out_path)
{
    wchar_t dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    GetTempFileNameW(dir, L"tmp", 0, out_path);
    size_t n = wcslen(out_path);
    wcsncpy(out_path + n - 3, ext, 3);

    char *buf = (char *)malloc(total_bytes);
    memset(buf, 'x', total_bytes);

    HANDLE h = CreateFileW(out_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD written = 0;
    WriteFile(h, buf, (DWORD)total_bytes, &written, NULL);
    CloseHandle(h);
    free(buf);
}

static void write_gltf_json(const char *json, wchar_t *out_path)
{
    wchar_t dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    GetTempFileNameW(dir, L"tmp", 0, out_path);
    /* GetTempFileNameW da extensao .tmp (3 letras) - "gltf" tem 4, entao
       anexa em vez de tentar substituir no lugar como os outros helpers */
    wcscat(out_path, L".gltf");

    HANDLE h = CreateFileW(out_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD written = 0;
    WriteFile(h, json, (DWORD)strlen(json), &written, NULL);
    CloseHandle(h);
}

void run_mesh_import_tests(void)
{
    /* triangulo OBJ simples, sem normais -> normal calculada por face */
    {
        const char *obj = "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
        wchar_t path[MAX_PATH];
        write_text_file(obj, L"obj", path);

        MeshData md;
        int ok = mesh_import_load(path, 1.0f, &md);
        DeleteFileW(path);

        EXPECT(ok);
        EXPECT(md.nverts == 3);
        EXPECT(md.nidx == 3);
        EXPECT(fabsf(md.verts[0].nz) > 0.9f);   /* normal da face plana no XY -> +-Z */
        mesh_data_free(&md);
    }

    /* quad OBJ (n-gon) -> triangulado por leque em 2 triangulos */
    {
        const char *obj = "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf 1 2 3 4\n";
        wchar_t path[MAX_PATH];
        write_text_file(obj, L"obj", path);

        MeshData md;
        int ok = mesh_import_load(path, 1.0f, &md);
        DeleteFileW(path);

        EXPECT(ok);
        EXPECT(md.nverts == 6);    /* 2 triangulos * 3 vertices, sem compartilhamento */
        EXPECT(md.nidx == 6);
        mesh_data_free(&md);
    }

    /* OBJ com normais explicitas -> usa a normal do arquivo, nao a calculada */
    {
        const char *obj =
            "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
            "vn 0 0 -1\nvn 0 0 -1\nvn 0 0 -1\n"
            "f 1//1 2//2 3//3\n";
        wchar_t path[MAX_PATH];
        write_text_file(obj, L"obj", path);

        MeshData md;
        int ok = mesh_import_load(path, 1.0f, &md);
        DeleteFileW(path);

        EXPECT(ok);
        EXPECT(md.verts[0].nz < -0.9f);   /* normal do arquivo aponta pra -Z */
        mesh_data_free(&md);
    }

    /* STL binario: mesmo triangulo do primeiro teste */
    {
        wchar_t path[MAX_PATH];
        write_binary_stl_triangle(path);

        MeshData md;
        int ok = mesh_import_load(path, 1.0f, &md);
        DeleteFileW(path);

        EXPECT(ok);
        EXPECT(md.nverts == 3);
        EXPECT(md.verts[0].nz > 0.9f);
        mesh_data_free(&md);
    }

    /* STL ASCII: mesmo triangulo, deve produzir geometria equivalente ao binario */
    {
        const char *stl =
            "solid teste\n"
            "facet normal 0 0 1\n"
            "outer loop\n"
            "vertex 0 0 0\n"
            "vertex 1 0 0\n"
            "vertex 0 1 0\n"
            "endloop\n"
            "endfacet\n"
            "endsolid teste\n";
        wchar_t path[MAX_PATH];
        write_text_file(stl, L"stl", path);

        MeshData md;
        int ok = mesh_import_load(path, 1.0f, &md);
        DeleteFileW(path);

        EXPECT(ok);
        EXPECT(md.nverts == 3);
        EXPECT(md.verts[0].nz > 0.9f);
        mesh_data_free(&md);
    }

    /* normalizacao: centroide perto de zero, raio da bbox perto do alvo */
    {
        const char *obj = "v 10 10 10\nv 11 10 10\nv 10 11 10\nf 1 2 3\n";
        wchar_t path[MAX_PATH];
        write_text_file(obj, L"obj", path);

        MeshData md;
        int ok = mesh_import_load(path, 2.0f, &md);
        DeleteFileW(path);

        EXPECT(ok);
        float cx = 0, cy = 0, cz = 0;
        for (int i = 0; i < md.nverts; ++i) { cx += md.verts[i].px; cy += md.verts[i].py; cz += md.verts[i].pz; }
        cx /= md.nverts; cy /= md.nverts; cz /= md.nverts;
        EXPECT(fabsf(cx) < 1e-3f && fabsf(cy) < 1e-3f && fabsf(cz) < 1e-3f);
        float maxr = 0.0f;
        for (int i = 0; i < md.nverts; ++i) {
            float r = sqrtf(md.verts[i].px*md.verts[i].px + md.verts[i].py*md.verts[i].py + md.verts[i].pz*md.verts[i].pz);
            if (r > maxr) maxr = r;
        }
        EXPECT(nearf(maxr, 2.0f));   /* size_scale=2.0 -> raio da esfera envolvente = 2.0 */
        mesh_data_free(&md);
    }

    /* arquivo inexistente -> falha graciosa, sem crash */
    {
        MeshData md;
        int ok = mesh_import_load(L"C:\\caminho\\que\\nao\\existe.obj", 1.0f, &md);
        EXPECT(!ok);
    }

    /* extensao desconhecida -> falha graciosa */
    {
        MeshData md;
        int ok = mesh_import_load(L"C:\\arquivo.xyz", 1.0f, &md);
        EXPECT(!ok);
    }

    /* .obj com 2 materiais reais (.mtl de verdade) -> 2 pecas com cores distintas */
    {
        const char *mtl = "newmtl vermelho\nKd 1.0 0.0 0.0\nnewmtl azul\nKd 0.0 0.0 1.0\n";
        const char *obj_fmt =
            "mtllib %s\n"
            "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
            "v 2 0 0\nv 3 0 0\nv 2 1 0\n"
            "usemtl vermelho\nf 1 2 3\n"
            "usemtl azul\nf 4 5 6\n";
        wchar_t obj_path[MAX_PATH], mtl_path[MAX_PATH];
        write_obj_with_mtl(obj_fmt, mtl, obj_path, mtl_path);

        MeshPieceSet ps;
        int ok = mesh_import_load_pieces(obj_path, 1.0f, &ps);
        DeleteFileW(obj_path);
        DeleteFileW(mtl_path);

        EXPECT(ok);
        EXPECT(ps.count == 2);
        if (ps.count == 2) {
            /* ordem de declaracao no .mtl: vermelho (indice 0) primeiro */
            EXPECT(ps.pieces[0].r > 0.9f && ps.pieces[0].g < 0.1f && ps.pieces[0].b < 0.1f);
            EXPECT(ps.pieces[1].r < 0.1f && ps.pieces[1].g < 0.1f && ps.pieces[1].b > 0.9f);
        }
        mesh_import_pieces_free(&ps);
    }

    /* .obj sem mtllib/usemtl -> nenhum material real -> falha graciosa */
    {
        const char *obj = "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
        wchar_t path[MAX_PATH];
        write_text_file(obj, L"obj", path);

        MeshPieceSet ps;
        int ok = mesh_import_load_pieces(path, 1.0f, &ps);
        DeleteFileW(path);

        EXPECT(!ok);
    }

    /* .glb: triangulo sem normais/material -> normal calculada, cor fallback branca */
    {
        const char *json =
            "{\"asset\":{\"version\":\"2.0\"},"
            "\"buffers\":[{\"byteLength\":36}],"
            "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36}],"
            "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
                            "\"min\":[0.0,0.0,0.0],\"max\":[1.0,1.0,0.0]}],"
            "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"mode\":4}]}],"
            "\"nodes\":[{\"mesh\":0}],"
            "\"scenes\":[{\"nodes\":[0]}],"
            "\"scene\":0}";
        float bin[9] = { 0,0,0, 1,0,0, 0,1,0 };
        wchar_t path[MAX_PATH];
        write_glb(json, bin, sizeof bin, path);

        MeshData md;
        int ok = mesh_import_load(path, 1.0f, &md);
        DeleteFileW(path);

        EXPECT(ok);
        EXPECT(md.nverts == 3);
        EXPECT(fabsf(md.verts[0].nz) > 0.9f);   /* normal calculada, triangulo plano em XY */
        mesh_data_free(&md);
    }

    /* .glb: 2 nos com 2 materiais (baseColorFactor diferentes) -> 2 pecas com cores distintas */
    {
        const char *json =
            "{\"asset\":{\"version\":\"2.0\"},"
            "\"buffers\":[{\"byteLength\":72}],"
            "\"bufferViews\":["
              "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
              "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":36}],"
            "\"accessors\":["
              "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
               "\"min\":[0.0,0.0,0.0],\"max\":[1.0,1.0,0.0]},"
              "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
               "\"min\":[2.0,0.0,0.0],\"max\":[3.0,1.0,0.0]}],"
            "\"materials\":["
              "{\"pbrMetallicRoughness\":{\"baseColorFactor\":[1.0,0.0,0.0,1.0]}},"
              "{\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.0,0.0,1.0,1.0]}}],"
            "\"meshes\":["
              "{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"material\":0,\"mode\":4}]},"
              "{\"primitives\":[{\"attributes\":{\"POSITION\":1},\"material\":1,\"mode\":4}]}],"
            "\"nodes\":[{\"mesh\":0},{\"mesh\":1}],"
            "\"scenes\":[{\"nodes\":[0,1]}],"
            "\"scene\":0}";
        float bin[18] = { 0,0,0, 1,0,0, 0,1,0,   2,0,0, 3,0,0, 2,1,0 };
        wchar_t path[MAX_PATH];
        write_glb(json, bin, sizeof bin, path);

        MeshPieceSet ps;
        int ok = mesh_import_load_pieces(path, 1.0f, &ps);
        DeleteFileW(path);

        EXPECT(ok);
        EXPECT(ps.count == 2);
        if (ps.count == 2) {
            EXPECT(ps.pieces[0].r > 0.9f && ps.pieces[0].b < 0.1f);
            EXPECT(ps.pieces[1].r < 0.1f && ps.pieces[1].b > 0.9f);
        }
        mesh_import_pieces_free(&ps);
    }

    /* .glb: no com translation aplicada corretamente (cgltf_node_transform_world) */
    {
        const char *json =
            "{\"asset\":{\"version\":\"2.0\"},"
            "\"buffers\":[{\"byteLength\":72}],"
            "\"bufferViews\":["
              "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
              "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":36}],"
            "\"accessors\":["
              "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
               "\"min\":[0.0,0.0,0.0],\"max\":[1.0,1.0,0.0]},"
              "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
               "\"min\":[0.0,0.0,0.0],\"max\":[1.0,1.0,0.0]}],"
            "\"meshes\":["
              "{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"mode\":4}]},"
              "{\"primitives\":[{\"attributes\":{\"POSITION\":1},\"mode\":4}]}],"
            "\"nodes\":[{\"mesh\":0},{\"mesh\":1,\"translation\":[100.0,0.0,0.0]}],"
            "\"scenes\":[{\"nodes\":[0,1]}],"
            "\"scene\":0}";
        /* mesmo triangulo local nos dois nos - o node 1 so' difere pela translation */
        float bin[18] = { 0,0,0, 1,0,0, 0,1,0,   0,0,0, 1,0,0, 0,1,0 };
        wchar_t path[MAX_PATH];
        write_glb(json, bin, sizeof bin, path);

        MeshData md;
        int ok = mesh_import_load(path, 1.0f, &md);
        DeleteFileW(path);

        EXPECT(ok);
        EXPECT(md.nverts == 6);
        float minx = 1e30f, maxx = -1e30f;
        for (int i = 0; i < md.nverts; ++i) {
            if (md.verts[i].px < minx) minx = md.verts[i].px;
            if (md.verts[i].px > maxx) maxx = md.verts[i].px;
        }
        /* sem a translacao de 100 unidades no node 1, os dois triangulos
           coincidiriam e o espalhamento em X seria ~1 unidade (o tamanho
           de um triangulo isolado); com ela aplicada antes da normalizacao,
           a separacao original domina e o espalhamento final fica bem maior */
        EXPECT((maxx - minx) > 1.5f);
        mesh_data_free(&md);
    }

    /* .glb inexistente -> falha graciosa */
    {
        MeshData md;
        int ok = mesh_import_load(L"C:\\caminho\\que\\nao\\existe.glb", 1.0f, &md);
        EXPECT(!ok);
    }

    /* .obj acima de 10MB -> recusado antes de tentar parsear */
    {
        wchar_t path[MAX_PATH];
        write_big_dummy_file(L"obj", (size_t)11 * 1024 * 1024, path);

        MeshData md;
        int ok = mesh_import_load(path, 1.0f, &md);
        DeleteFileW(path);

        EXPECT(!ok);
    }

    /* .glb acima de 10MB -> recusado antes de tentar parsear */
    {
        wchar_t path[MAX_PATH];
        write_big_dummy_file(L"glb", (size_t)11 * 1024 * 1024, path);

        MeshData md;
        int ok = mesh_import_load(path, 1.0f, &md);
        DeleteFileW(path);

        EXPECT(!ok);
    }

    /* .gltf pequeno no disco, mas com byteLength declarado acima do limite
       -> recusado sem tentar resolver o buffer (que nem existe de verdade) */
    {
        const char *json =
            "{\"asset\":{\"version\":\"2.0\"},"
            "\"buffers\":[{\"byteLength\":20000000}],"
            "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36}],"
            "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
                            "\"min\":[0.0,0.0,0.0],\"max\":[1.0,1.0,0.0]}],"
            "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"mode\":4}]}],"
            "\"nodes\":[{\"mesh\":0}],"
            "\"scenes\":[{\"nodes\":[0]}],"
            "\"scene\":0}";
        wchar_t path[MAX_PATH];
        write_gltf_json(json, path);

        MeshData md;
        int ok = mesh_import_load(path, 1.0f, &md);
        DeleteFileW(path);

        EXPECT(!ok);
    }

    /* mesh_import_file_too_big: arquivo pequeno normal -> aceito (0) */
    {
        const char *obj = "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
        wchar_t path[MAX_PATH];
        write_text_file(obj, L"obj", path);

        int too_big = mesh_import_file_too_big(path);
        DeleteFileW(path);

        EXPECT(!too_big);
    }

    /* mesh_import_file_too_big: arquivo grande -> recusado (1) */
    {
        wchar_t path[MAX_PATH];
        write_big_dummy_file(L"obj", (size_t)11 * 1024 * 1024, path);

        int too_big = mesh_import_file_too_big(path);
        DeleteFileW(path);

        EXPECT(too_big);
    }
}
