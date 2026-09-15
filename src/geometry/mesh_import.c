#include "geometry/mesh_import.h"
#include "util/log.h"

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <wctype.h>
#include <math.h>

#include "fast_obj.h"
#include "cgltf.h"

static int has_ext(const wchar_t *path, const wchar_t *ext)
{
    size_t pl = wcslen(path), el = wcslen(ext);
    if (pl < el) return 0;
    const wchar_t *suffix = path + (pl - el);
    for (size_t i = 0; i < el; ++i)
        if (towlower(suffix[i]) != towlower(ext[i])) return 0;
    return 1;
}

static int load_gltf(const wchar_t *path, MeshData *out);
static int load_gltf_pieces(const wchar_t *path, float size_scale, MeshPieceSet *out);

static int mesh_file_too_big(const wchar_t *path, unsigned long long max_bytes)
{
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &fad)) return 0;
    unsigned long long size = ((unsigned long long)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
    return size > max_bytes;
}

int mesh_import_file_too_big(const wchar_t *path)
{
    if (!path || !path[0]) return 0;
    return mesh_file_too_big(path, MESH_IMPORT_MAX_BYTES);
}

static int load_obj(const wchar_t *path, MeshData *out)
{
    char u8[1024];
    WideCharToMultiByte(CP_UTF8, 0, path, -1, u8, (int)sizeof u8, NULL, NULL);

    fastObjMesh *m = fast_obj_read(u8);
    if (!m) return 0;

    unsigned int ntris = 0;
    for (unsigned int f = 0; f < m->face_count; ++f)
        if (m->face_vertices[f] >= 3) ntris += m->face_vertices[f] - 2;

    if (ntris == 0) { fast_obj_destroy(m); return 0; }

    MeshVertex *verts = (MeshVertex *)malloc((size_t)ntris * 3 * sizeof(MeshVertex));
    unsigned *idx = (unsigned *)malloc((size_t)ntris * 3 * sizeof(unsigned));
    if (!verts || !idx) {
        free(verts); free(idx);
        fast_obj_destroy(m);
        return 0;
    }

    unsigned int vi = 0;
    unsigned int cursor = 0;

    for (unsigned int f = 0; f < m->face_count; ++f) {
        unsigned int fv = m->face_vertices[f];
        if (fv < 3) { cursor += fv; continue; }

        fastObjIndex face_idx[64];
        unsigned int use = fv > 64 ? 64 : fv;
        for (unsigned int k = 0; k < use; ++k) face_idx[k] = m->indices[cursor + k];

        v3 p0 = { m->positions[face_idx[0].p*3+0], m->positions[face_idx[0].p*3+1], m->positions[face_idx[0].p*3+2] };
        v3 p1 = { m->positions[face_idx[1].p*3+0], m->positions[face_idx[1].p*3+1], m->positions[face_idx[1].p*3+2] };
        v3 p2 = { m->positions[face_idx[2].p*3+0], m->positions[face_idx[2].p*3+1], m->positions[face_idx[2].p*3+2] };
        v3 fn = v3_norm(v3_cross(v3_sub(p1, p0), v3_sub(p2, p0)));

        for (unsigned int k = 1; k + 1 < use; ++k) {
            unsigned int tri[3] = { 0, k, k + 1 };
            for (int c = 0; c < 3; ++c) {
                fastObjIndex fi = face_idx[tri[c]];
                v3 pos = { m->positions[fi.p*3+0], m->positions[fi.p*3+1], m->positions[fi.p*3+2] };
                v3 nrm = fn;
                if (fi.n != 0 && m->normal_count > fi.n) {
                    v3 fnrm = { m->normals[fi.n*3+0], m->normals[fi.n*3+1], m->normals[fi.n*3+2] };
                    if (v3_len(fnrm) > 1e-6f) nrm = fnrm;
                }
                verts[vi] = (MeshVertex){ pos.x, pos.y, pos.z, nrm.x, nrm.y, nrm.z, 0.0f };
                idx[vi] = vi;
                vi++;
            }
        }
        cursor += fv;
    }

    fast_obj_destroy(m);

    out->verts = verts; out->nverts = (int)vi;
    out->idx = idx; out->nidx = (int)vi;
    return 1;
}

typedef struct { MeshVertex *v; int n, cap; unsigned *idx; int ni, icap; } ObjGroupBuf;

static void group_push_tri(ObjGroupBuf *g, MeshVertex a, MeshVertex b, MeshVertex c)
{
    if (g->n + 3 > g->cap) {
        g->cap = g->cap ? g->cap * 2 : 64;
        g->v = (MeshVertex *)realloc(g->v, (size_t)g->cap * sizeof(MeshVertex));
    }
    if (g->ni + 3 > g->icap) {
        g->icap = g->icap ? g->icap * 2 : 64;
        g->idx = (unsigned *)realloc(g->idx, (size_t)g->icap * sizeof(unsigned));
    }
    g->v[g->n] = a; g->idx[g->ni] = (unsigned)g->n; g->n++; g->ni++;
    g->v[g->n] = b; g->idx[g->ni] = (unsigned)g->n; g->n++; g->ni++;
    g->v[g->n] = c; g->idx[g->ni] = (unsigned)g->n; g->n++; g->ni++;
}

static void normalize_pieces(MeshPieceSet *out, float size_scale)
{
    v3 centroid = { 0, 0, 0 };
    int total = 0;
    for (int p = 0; p < out->count; ++p) {
        MeshData *d = &out->pieces[p].data;
        for (int i = 0; i < d->nverts; ++i) {
            centroid = v3_add(centroid, (v3){ d->verts[i].px, d->verts[i].py, d->verts[i].pz });
            total++;
        }
    }
    if (total == 0) return;
    centroid = v3_scale(centroid, 1.0f / (float)total);

    float max_r = 0.0f;
    for (int p = 0; p < out->count; ++p) {
        MeshData *d = &out->pieces[p].data;
        for (int i = 0; i < d->nverts; ++i) {
            v3 pos = v3_sub((v3){ d->verts[i].px, d->verts[i].py, d->verts[i].pz }, centroid);
            float r = v3_len(pos);
            if (r > max_r) max_r = r;
        }
    }
    if (max_r < 1e-6f) max_r = 1.0f;
    float scale = size_scale / max_r;

    for (int p = 0; p < out->count; ++p) {
        MeshData *d = &out->pieces[p].data;
        float minx = 1e30f, miny = 1e30f, minz = 1e30f, maxx = -1e30f, maxy = -1e30f, maxz = -1e30f;
        for (int i = 0; i < d->nverts; ++i) {
            MeshVertex *v = &d->verts[i];
            v->px = (v->px - centroid.x) * scale;
            v->py = (v->py - centroid.y) * scale;
            v->pz = (v->pz - centroid.z) * scale;
            if (v->px < minx) minx = v->px;
            if (v->px > maxx) maxx = v->px;
            if (v->py < miny) miny = v->py;
            if (v->py > maxy) maxy = v->py;
            if (v->pz < minz) minz = v->pz;
            if (v->pz > maxz) maxz = v->pz;
        }
        d->minx = minx; d->miny = miny; d->minz = minz;
        d->maxx = maxx; d->maxy = maxy; d->maxz = maxz;
    }
}

int mesh_import_load_pieces(const wchar_t *path, float size_scale, MeshPieceSet *out)
{
    memset(out, 0, sizeof *out);

    if (mesh_file_too_big(path, MESH_IMPORT_MAX_BYTES)) {
        log_errorf("mesh_import: arquivo excede o limite de %.0f MB",
                   MESH_IMPORT_MAX_BYTES / (1024.0 * 1024.0));
        return 0;
    }

    if (has_ext(path, L".glb") || has_ext(path, L".gltf"))
        return load_gltf_pieces(path, size_scale, out);

    char u8[1024];
    WideCharToMultiByte(CP_UTF8, 0, path, -1, u8, (int)sizeof u8, NULL, NULL);

    fastObjMesh *m = fast_obj_read(u8);
    if (!m) return 0;

    int has_real_material = 0;
    for (unsigned int i = 0; i < m->material_count; ++i)
        if (!m->materials[i].fallback) { has_real_material = 1; break; }
    if (!has_real_material) { fast_obj_destroy(m); return 0; }

    ObjGroupBuf *groups = (ObjGroupBuf *)calloc(m->material_count, sizeof(ObjGroupBuf));
    if (!groups) { fast_obj_destroy(m); return 0; }

    unsigned int cursor = 0;
    for (unsigned int f = 0; f < m->face_count; ++f) {
        unsigned int fv = m->face_vertices[f];
        if (fv < 3) { cursor += fv; continue; }
        unsigned int matidx = m->face_materials[f];
        if (matidx >= m->material_count) { cursor += fv; continue; }

        fastObjIndex face_idx[64];
        unsigned int use = fv > 64 ? 64 : fv;
        for (unsigned int k = 0; k < use; ++k) face_idx[k] = m->indices[cursor + k];

        v3 p0 = { m->positions[face_idx[0].p*3+0], m->positions[face_idx[0].p*3+1], m->positions[face_idx[0].p*3+2] };
        v3 p1 = { m->positions[face_idx[1].p*3+0], m->positions[face_idx[1].p*3+1], m->positions[face_idx[1].p*3+2] };
        v3 p2 = { m->positions[face_idx[2].p*3+0], m->positions[face_idx[2].p*3+1], m->positions[face_idx[2].p*3+2] };
        v3 fn = v3_norm(v3_cross(v3_sub(p1, p0), v3_sub(p2, p0)));

        for (unsigned int k = 1; k + 1 < use; ++k) {
            unsigned int tri[3] = { 0, k, k + 1 };
            MeshVertex tv[3];
            for (int c = 0; c < 3; ++c) {
                fastObjIndex fi = face_idx[tri[c]];
                v3 pos = { m->positions[fi.p*3+0], m->positions[fi.p*3+1], m->positions[fi.p*3+2] };
                v3 nrm = fn;
                if (fi.n != 0 && m->normal_count > fi.n) {
                    v3 fnrm = { m->normals[fi.n*3+0], m->normals[fi.n*3+1], m->normals[fi.n*3+2] };
                    if (v3_len(fnrm) > 1e-6f) nrm = fnrm;
                }
                tv[c] = (MeshVertex){ pos.x, pos.y, pos.z, nrm.x, nrm.y, nrm.z, 0.0f };
            }
            group_push_tri(&groups[matidx], tv[0], tv[1], tv[2]);
        }
        cursor += fv;
    }

    int npieces = 0;
    for (unsigned int i = 0; i < m->material_count; ++i)
        if (groups[i].n > 0) npieces++;

    if (npieces == 0) {
        for (unsigned int i = 0; i < m->material_count; ++i) { free(groups[i].v); free(groups[i].idx); }
        free(groups);
        fast_obj_destroy(m);
        return 0;
    }

    MeshPiece *pieces = (MeshPiece *)calloc((size_t)npieces, sizeof(MeshPiece));
    if (!pieces) {
        for (unsigned int i = 0; i < m->material_count; ++i) { free(groups[i].v); free(groups[i].idx); }
        free(groups);
        fast_obj_destroy(m);
        return 0;
    }

    int pi = 0;
    for (unsigned int i = 0; i < m->material_count; ++i) {
        if (groups[i].n == 0) { free(groups[i].v); free(groups[i].idx); continue; }
        pieces[pi].data.verts = groups[i].v;
        pieces[pi].data.nverts = groups[i].n;
        pieces[pi].data.idx = groups[i].idx;
        pieces[pi].data.nidx = groups[i].ni;
        pieces[pi].r = m->materials[i].Kd[0];
        pieces[pi].g = m->materials[i].Kd[1];
        pieces[pi].b = m->materials[i].Kd[2];
        pi++;
    }
    free(groups);
    fast_obj_destroy(m);

    out->pieces = pieces;
    out->count = npieces;
    normalize_pieces(out, size_scale);
    return 1;
}

void mesh_import_pieces_free(MeshPieceSet *s)
{
    if (!s) return;
    for (int i = 0; i < s->count; ++i) {
        free(s->pieces[i].data.verts);
        free(s->pieces[i].data.idx);
    }
    free(s->pieces);
    memset(s, 0, sizeof *s);
}

static int load_stl(const wchar_t *path, MeshData *out)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    DWORD fsize = GetFileSize(h, NULL);
    unsigned char *buf = (unsigned char *)malloc(fsize ? fsize : 1);
    DWORD rd = 0;
    if (!buf || (fsize > 0 && (!ReadFile(h, buf, fsize, &rd, NULL) || rd != fsize))) {
        CloseHandle(h);
        free(buf);
        return 0;
    }
    CloseHandle(h);

    int is_binary = 0;
    unsigned int ntris_bin = 0;
    if (fsize >= 84) {
        memcpy(&ntris_bin, buf + 80, 4);
        unsigned long long expected = 84ull + (unsigned long long)ntris_bin * 50ull;
        if (expected == (unsigned long long)fsize) is_binary = 1;
    }

    unsigned int ntris = 0;
    MeshVertex *verts = NULL;
    unsigned *idx = NULL;

    if (is_binary) {
        ntris = ntris_bin;
        if (ntris == 0) { free(buf); return 0; }
        verts = (MeshVertex *)malloc((size_t)ntris * 3 * sizeof(MeshVertex));
        idx = (unsigned *)malloc((size_t)ntris * 3 * sizeof(unsigned));
        if (!verts || !idx) { free(verts); free(idx); free(buf); return 0; }

        const unsigned char *p = buf + 84;
        for (unsigned int t = 0; t < ntris; ++t) {
            float rec[12];
            memcpy(rec, p, 48);
            p += 50;
            v3 n = { rec[0], rec[1], rec[2] };
            v3 pos[3];
            for (int c = 0; c < 3; ++c)
                pos[c] = (v3){ rec[3 + c*3 + 0], rec[3 + c*3 + 1], rec[3 + c*3 + 2] };
            if (v3_len(n) < 1e-6f)
                n = v3_norm(v3_cross(v3_sub(pos[1], pos[0]), v3_sub(pos[2], pos[0])));
            for (int c = 0; c < 3; ++c) {
                unsigned int v = t * 3 + (unsigned)c;
                verts[v] = (MeshVertex){ pos[c].x, pos[c].y, pos[c].z, n.x, n.y, n.z, 0.0f };
                idx[v] = v;
            }
        }
    } else {
        unsigned int vertex_count = 0;
        for (DWORD i = 0; i + 6 <= fsize; ++i)
            if (memcmp(buf + i, "vertex", 6) == 0) vertex_count++;
        if (vertex_count == 0 || vertex_count % 3 != 0) { free(buf); return 0; }
        ntris = vertex_count / 3;

        verts = (MeshVertex *)malloc((size_t)ntris * 3 * sizeof(MeshVertex));
        idx = (unsigned *)malloc((size_t)ntris * 3 * sizeof(unsigned));
        if (!verts || !idx) { free(verts); free(idx); free(buf); return 0; }

        char *text = (char *)malloc((size_t)fsize + 1);
        if (!text) { free(verts); free(idx); free(buf); return 0; }
        memcpy(text, buf, fsize);
        text[fsize] = 0;

        unsigned int vi = 0;
        char *cursor = text;
        while (*cursor && vi < ntris * 3) {
            char *facet = strstr(cursor, "facet normal");
            if (!facet) break;
            v3 n = { 0, 0, 0 };
            sscanf(facet, "facet normal %f %f %f", &n.x, &n.y, &n.z);
            cursor = facet + 12;

            v3 pos[3];
            int got = 0;
            for (int c = 0; c < 3; ++c) {
                char *vtx = strstr(cursor, "vertex");
                if (!vtx) break;
                pos[c] = (v3){ 0, 0, 0 };
                sscanf(vtx, "vertex %f %f %f", &pos[c].x, &pos[c].y, &pos[c].z);
                cursor = vtx + 6;
                got++;
            }
            if (got != 3) break;
            if (v3_len(n) < 1e-6f)
                n = v3_norm(v3_cross(v3_sub(pos[1], pos[0]), v3_sub(pos[2], pos[0])));
            for (int c = 0; c < 3; ++c) {
                verts[vi] = (MeshVertex){ pos[c].x, pos[c].y, pos[c].z, n.x, n.y, n.z, 0.0f };
                idx[vi] = vi;
                vi++;
            }
        }
        free(text);
        if (vi != ntris * 3) { free(verts); free(idx); free(buf); return 0; }
    }

    free(buf);
    out->verts = verts; out->nverts = (int)(ntris * 3);
    out->idx = idx; out->nidx = (int)(ntris * 3);
    return 1;
}

typedef struct { MeshVertex *v; int n; unsigned *idx; int ni; } GltfPrimGeom;

/* transforma so' a parte 3x3 (rotacao+escala) da matriz de mundo -
   sem translacao, adequado pra normais. Nao trata escala nao-uniforme
   corretamente (exigiria a inversa-transposta), mas essa fase nao tem
   textura/PBR fino que dependa disso. */
static v3 gltf_transform_normal(m4 world, v3 n)
{
    return (v3){
        world.m[0]*n.x + world.m[4]*n.y + world.m[8]*n.z,
        world.m[1]*n.x + world.m[5]*n.y + world.m[9]*n.z,
        world.m[2]*n.x + world.m[6]*n.y + world.m[10]*n.z
    };
}

/* extrai geometria (posicoes+normais+indices) de UM primitive, ja
   transformada pela matriz de mundo do no que a contem. Retorna 0 se
   o primitive nao e' TRIANGLES ou nao tem POSITION. */
static int gltf_extract_primitive(cgltf_primitive *prim, m4 world, GltfPrimGeom *out)
{
    memset(out, 0, sizeof *out);
    if (prim->type != cgltf_primitive_type_triangles) return 0;

    cgltf_accessor *pos_acc = NULL, *nrm_acc = NULL;
    for (cgltf_size i = 0; i < prim->attributes_count; ++i) {
        if (prim->attributes[i].type == cgltf_attribute_type_position)
            pos_acc = prim->attributes[i].data;
        else if (prim->attributes[i].type == cgltf_attribute_type_normal)
            nrm_acc = prim->attributes[i].data;
    }
    if (!pos_acc || pos_acc->count == 0) return 0;

    cgltf_size nverts = pos_acc->count;
    float *positions = (float *)malloc(nverts * 3 * sizeof(float));
    if (!positions) return 0;
    cgltf_accessor_unpack_floats(pos_acc, positions, nverts * 3);

    float *normals = NULL;
    if (nrm_acc && nrm_acc->count == nverts) {
        normals = (float *)malloc(nverts * 3 * sizeof(float));
        if (normals) cgltf_accessor_unpack_floats(nrm_acc, normals, nverts * 3);
    }

    unsigned int nidx;
    unsigned int *idx;
    if (prim->indices) {
        nidx = (unsigned int)prim->indices->count;
        idx = (unsigned int *)malloc((size_t)nidx * sizeof(unsigned int));
        if (!idx) { free(positions); free(normals); return 0; }
        cgltf_accessor_unpack_indices(prim->indices, idx, sizeof(unsigned int), nidx);
    } else {
        nidx = (unsigned int)nverts;
        idx = (unsigned int *)malloc((size_t)nidx * sizeof(unsigned int));
        if (!idx) { free(positions); free(normals); return 0; }
        for (unsigned int i = 0; i < nidx; ++i) idx[i] = i;
    }
    if (nidx < 3 || nidx % 3 != 0) { free(positions); free(normals); free(idx); return 0; }

    MeshVertex *verts = (MeshVertex *)malloc((size_t)nverts * sizeof(MeshVertex));
    if (!verts) { free(positions); free(normals); free(idx); return 0; }
    for (cgltf_size i = 0; i < nverts; ++i) {
        v3 p = { positions[i*3+0], positions[i*3+1], positions[i*3+2] };
        p = m4_mul_point(world, p);
        v3 n = { 0.0f, 0.0f, 1.0f };
        if (normals) {
            v3 nraw = { normals[i*3+0], normals[i*3+1], normals[i*3+2] };
            n = gltf_transform_normal(world, nraw);
            float ln = v3_len(n);
            if (ln > 1e-6f) n = v3_scale(n, 1.0f / ln);
        }
        verts[i] = (MeshVertex){ p.x, p.y, p.z, n.x, n.y, n.z, 0.0f };
    }
    free(positions);
    free(normals);

    if (!normals) {
        for (unsigned int t = 0; t + 2 < nidx; t += 3) {
            unsigned int i0 = idx[t], i1 = idx[t+1], i2 = idx[t+2];
            v3 p0 = { verts[i0].px, verts[i0].py, verts[i0].pz };
            v3 p1 = { verts[i1].px, verts[i1].py, verts[i1].pz };
            v3 p2 = { verts[i2].px, verts[i2].py, verts[i2].pz };
            v3 fn = v3_norm(v3_cross(v3_sub(p1, p0), v3_sub(p2, p0)));
            verts[i0].nx = fn.x; verts[i0].ny = fn.y; verts[i0].nz = fn.z;
            verts[i1].nx = fn.x; verts[i1].ny = fn.y; verts[i1].nz = fn.z;
            verts[i2].nx = fn.x; verts[i2].ny = fn.y; verts[i2].nz = fn.z;
        }
    }

    out->v = verts; out->n = (int)nverts;
    out->idx = idx; out->ni = (int)nidx;
    return 1;
}

typedef struct { MeshVertex *v; int n; unsigned *idx; int ni; float r, g, b; } GltfChunk;
typedef struct { GltfChunk *c; int n, cap; } GltfChunkList;

static void gltf_chunklist_push(GltfChunkList *l, GltfChunk c)
{
    if (l->n == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 8;
        l->c = (GltfChunk *)realloc(l->c, (size_t)l->cap * sizeof(GltfChunk));
    }
    l->c[l->n++] = c;
}

static void gltf_visit_node(cgltf_node *node, GltfChunkList *out)
{
    if (node->mesh) {
        float wraw[16];
        cgltf_node_transform_world(node, wraw);
        m4 world; memcpy(world.m, wraw, sizeof world.m);

        for (cgltf_size p = 0; p < node->mesh->primitives_count; ++p) {
            cgltf_primitive *prim = &node->mesh->primitives[p];
            GltfPrimGeom g;
            if (!gltf_extract_primitive(prim, world, &g)) continue;

            float r = 1.0f, gg = 1.0f, b = 1.0f;
            if (prim->material && prim->material->has_pbr_metallic_roughness) {
                r = prim->material->pbr_metallic_roughness.base_color_factor[0];
                gg = prim->material->pbr_metallic_roughness.base_color_factor[1];
                b = prim->material->pbr_metallic_roughness.base_color_factor[2];
            }
            GltfChunk c = { g.v, g.n, g.idx, g.ni, r, gg, b };
            gltf_chunklist_push(out, c);
        }
    }
    for (cgltf_size i = 0; i < node->children_count; ++i)
        gltf_visit_node(node->children[i], out);
}

/* percorre a cena default (ou a primeira, se nenhuma for marcada como
   default) e devolve uma lista de pedacos de geometria ja
   transformados por seus respectivos nos - usada tanto pelo caminho
   fundido quanto pelo de pecas. Retorna 0 se o parse/load falhar ou
   nao houver nenhum triangulo valido. */
static int gltf_collect_chunks(const wchar_t *path, GltfChunkList *out)
{
    memset(out, 0, sizeof *out);

    char u8[1024];
    WideCharToMultiByte(CP_UTF8, 0, path, -1, u8, (int)sizeof u8, NULL, NULL);

    cgltf_options options;
    memset(&options, 0, sizeof options);
    cgltf_data *data = NULL;
    if (cgltf_parse_file(&options, u8, &data) != cgltf_result_success) return 0;

    unsigned long long total_buf_bytes = 0;
    for (cgltf_size i = 0; i < data->buffers_count; ++i)
        total_buf_bytes += data->buffers[i].size;
    if (total_buf_bytes > MESH_IMPORT_MAX_BYTES) {
        log_errorf("mesh_import: buffers do .gltf/.glb excedem o limite de %.0f MB",
                   MESH_IMPORT_MAX_BYTES / (1024.0 * 1024.0));
        cgltf_free(data);
        return 0;
    }

    if (cgltf_load_buffers(&options, data, u8) != cgltf_result_success) {
        cgltf_free(data);
        return 0;
    }

    if (data->scenes_count == 0) { cgltf_free(data); return 0; }
    cgltf_scene *scene = data->scene ? data->scene : &data->scenes[0];

    for (cgltf_size i = 0; i < scene->nodes_count; ++i)
        gltf_visit_node(scene->nodes[i], out);

    cgltf_free(data);

    if (out->n == 0) { free(out->c); out->c = NULL; return 0; }
    return 1;
}

static void gltf_chunklist_free(GltfChunkList *l)
{
    for (int i = 0; i < l->n; ++i) { free(l->c[i].v); free(l->c[i].idx); }
    free(l->c);
    memset(l, 0, sizeof *l);
}

static int load_gltf(const wchar_t *path, MeshData *out)
{
    GltfChunkList list;
    if (!gltf_collect_chunks(path, &list)) return 0;

    unsigned int total_v = 0, total_i = 0;
    for (int i = 0; i < list.n; ++i) {
        total_v += (unsigned)list.c[i].n;
        total_i += (unsigned)list.c[i].ni;
    }

    MeshVertex *verts = (MeshVertex *)malloc((size_t)total_v * sizeof(MeshVertex));
    unsigned *idx = (unsigned *)malloc((size_t)total_i * sizeof(unsigned));
    if (!verts || !idx) {
        free(verts); free(idx);
        gltf_chunklist_free(&list);
        return 0;
    }

    unsigned int vbase = 0, ibase = 0;
    for (int i = 0; i < list.n; ++i) {
        GltfChunk *c = &list.c[i];
        memcpy(verts + vbase, c->v, (size_t)c->n * sizeof(MeshVertex));
        for (int k = 0; k < c->ni; ++k) idx[ibase + (unsigned)k] = c->idx[k] + vbase;
        vbase += (unsigned)c->n;
        ibase += (unsigned)c->ni;
    }
    gltf_chunklist_free(&list);

    out->verts = verts; out->nverts = (int)total_v;
    out->idx = idx; out->nidx = (int)total_i;
    return 1;
}

static int load_gltf_pieces(const wchar_t *path, float size_scale, MeshPieceSet *out)
{
    memset(out, 0, sizeof *out);

    GltfChunkList list;
    if (!gltf_collect_chunks(path, &list)) return 0;

    MeshPiece *pieces = (MeshPiece *)calloc((size_t)list.n, sizeof(MeshPiece));
    if (!pieces) { gltf_chunklist_free(&list); return 0; }

    for (int i = 0; i < list.n; ++i) {
        pieces[i].data.verts = list.c[i].v;
        pieces[i].data.nverts = list.c[i].n;
        pieces[i].data.idx = list.c[i].idx;
        pieces[i].data.nidx = list.c[i].ni;
        pieces[i].r = list.c[i].r;
        pieces[i].g = list.c[i].g;
        pieces[i].b = list.c[i].b;
    }
    free(list.c);   /* os buffers v/idx de cada chunk foram passados pras pecas, nao liberar aqui */

    out->pieces = pieces;
    out->count = list.n;
    normalize_pieces(out, size_scale);
    return 1;
}

static void normalize_mesh(MeshData *out, float size_scale)
{
    if (out->nverts == 0) return;

    v3 centroid = { 0, 0, 0 };
    for (int i = 0; i < out->nverts; ++i)
        centroid = v3_add(centroid, (v3){ out->verts[i].px, out->verts[i].py, out->verts[i].pz });
    centroid = v3_scale(centroid, 1.0f / (float)out->nverts);

    float max_r = 0.0f;
    for (int i = 0; i < out->nverts; ++i) {
        v3 p = v3_sub((v3){ out->verts[i].px, out->verts[i].py, out->verts[i].pz }, centroid);
        float r = v3_len(p);
        if (r > max_r) max_r = r;
    }
    if (max_r < 1e-6f) max_r = 1.0f;

    float scale = size_scale / max_r;
    float minx = 1e30f, miny = 1e30f, minz = 1e30f, maxx = -1e30f, maxy = -1e30f, maxz = -1e30f;
    for (int i = 0; i < out->nverts; ++i) {
        MeshVertex *v = &out->verts[i];
        v->px = (v->px - centroid.x) * scale;
        v->py = (v->py - centroid.y) * scale;
        v->pz = (v->pz - centroid.z) * scale;
        if (v->px < minx) minx = v->px;
        if (v->px > maxx) maxx = v->px;
        if (v->py < miny) miny = v->py;
        if (v->py > maxy) maxy = v->py;
        if (v->pz < minz) minz = v->pz;
        if (v->pz > maxz) maxz = v->pz;
    }
    out->minx = minx; out->miny = miny; out->minz = minz;
    out->maxx = maxx; out->maxy = maxy; out->maxz = maxz;
}

int mesh_import_load(const wchar_t *path, float size_scale, MeshData *out)
{
    memset(out, 0, sizeof *out);
    if (!path || !path[0]) return 0;
    if (mesh_file_too_big(path, MESH_IMPORT_MAX_BYTES)) {
        log_errorf("mesh_import: arquivo excede o limite de %.0f MB",
                   MESH_IMPORT_MAX_BYTES / (1024.0 * 1024.0));
        return 0;
    }

    int ok;
    if (has_ext(path, L".obj"))
        ok = load_obj(path, out);
    else if (has_ext(path, L".stl"))
        ok = load_stl(path, out);
    else if (has_ext(path, L".glb") || has_ext(path, L".gltf"))
        ok = load_gltf(path, out);
    else {
        log_errorf("mesh_import: extensao desconhecida");
        return 0;
    }
    if (!ok) return 0;

    normalize_mesh(out, size_scale);
    return 1;
}
