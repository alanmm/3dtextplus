#include "geometry/mesh_import.h"
#include "util/log.h"

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <wctype.h>
#include <math.h>

#include "fast_obj.h"

static int has_ext(const wchar_t *path, const wchar_t *ext)
{
    size_t pl = wcslen(path), el = wcslen(ext);
    if (pl < el) return 0;
    const wchar_t *suffix = path + (pl - el);
    for (size_t i = 0; i < el; ++i)
        if (towlower(suffix[i]) != towlower(ext[i])) return 0;
    return 1;
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
    out->has_sdf = 0;
    memset(&out->sdf, 0, sizeof out->sdf);
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
        pieces[pi].data.has_sdf = 0;
        memset(&pieces[pi].data.sdf, 0, sizeof pieces[pi].data.sdf);
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
    out->has_sdf = 0;
    memset(&out->sdf, 0, sizeof out->sdf);
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

    int ok;
    if (has_ext(path, L".obj"))
        ok = load_obj(path, out);
    else if (has_ext(path, L".stl"))
        ok = load_stl(path, out);
    else {
        log_errorf("mesh_import: extensao desconhecida");
        return 0;
    }
    if (!ok) return 0;

    normalize_mesh(out, size_scale);
    return 1;
}
