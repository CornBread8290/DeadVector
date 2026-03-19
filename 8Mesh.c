#include "8Mesh.h"
#include "3utils.h"
#include <stdlib.h>
#include <string.h>

#define MB_MAX_DEPTH 8
#define MB_SEL_STACK 8

typedef struct {
    Mesh* m;
    unsigned char* sel;      // per-vertex selection flag
    int   sel_cap;
    unsigned char* mat;      // per-triangle material id
    int   mat_cap;
    unsigned char cur_mat;
    unsigned char* stack[MB_SEL_STACK];
    int   sp;
} MB;

static int   rd_u8 (const unsigned char** p) { return *(*p)++; }
static int   rd_s16(const unsigned char** p) { int v = (*p)[0] | ((*p)[1] << 8); *p += 2; return (short)v; }
static float rd_f  (const unsigned char** p) { return (float)rd_s16(p) * (1.0f / 256.0f); }
static Vec3  rd_v  (const unsigned char** p) { Vec3 v; v.x = rd_f(p); v.y = rd_f(p); v.z = rd_f(p); return v; }

static void sel_fit(MB* b) {
    if (b->m->vertex_count <= b->sel_cap) return;
    int cap = b->m->vertex_count * 2 + 16;
    b->sel = (unsigned char*)realloc(b->sel, (size_t)cap);
    memset(b->sel + b->sel_cap, 0, (size_t)(cap - b->sel_cap));
    for (int i = 0; i < b->sp; ++i)
        if (b->stack[i]) {
            b->stack[i] = (unsigned char*)realloc(b->stack[i], (size_t)cap);
            memset(b->stack[i] + b->sel_cap, 0, (size_t)(cap - b->sel_cap));
        }
    b->sel_cap = cap;
}

static void mat_fit(MB* b) {
    int tris = b->m->index_count / 3;
    if (tris <= b->mat_cap) return;
    int cap = tris * 2 + 16;
    b->mat = (unsigned char*)realloc(b->mat, (size_t)cap);
    memset(b->mat + b->mat_cap, b->cur_mat, (size_t)(cap - b->mat_cap));
    b->mat_cap = cap;
}

static void sync(MB* b) { sel_fit(b); mat_fit(b); }

static void select_from(MB* b, int first_vertex) {
    memset(b->sel, 0, (size_t)b->sel_cap);
    for (int i = first_vertex; i < b->m->vertex_count; ++i) b->sel[i] = 1;
}

static int tri_selected(const MB* b, int t) {
    const unsigned* ix = b->m->indices + t * 3;
    return b->sel[ix[0]] && b->sel[ix[1]] && b->sel[ix[2]];
}

static void emit_vertex(MB* b, VertexFormat v) { add_vertex(b->m, v); }

static void prim_box(MB* b, Vec3 mn, Vec3 mx) {
    const Color4 w = {1, 1, 1, 1};
    static const signed char F[6][3] = {{0,0,-1},{0,0,1},{-1,0,0},{1,0,0},{0,-1,0},{0,1,0}};
    static const unsigned char C[6][4] = {
        {0,2,6,4},  // -Z
        {1,5,7,3},  // +Z
        {0,1,3,2},  // -X
        {4,6,7,5},  // +X
        {0,4,5,1},  // -Y
        {2,3,7,6}   // +Y
    };
    int base = b->m->vertex_count;
    for (int f = 0; f < 6; ++f) {
        for (int k = 0; k < 4; ++k) {
            int c = C[f][k];
            Vec3 p = { (c & 4) ? mx.x : mn.x, (c & 2) ? mx.y : mn.y, (c & 1) ? mx.z : mn.z };
            emit_vertex(b, (VertexFormat){ p, {F[f][0], F[f][1], F[f][2]}, w, {(float)(k & 1), (float)(k >> 1)} });
        }
        int o = base + f * 4;
        add_triangle(b->m, o, o + 1, o + 2);
        add_triangle(b->m, o, o + 2, o + 3);
    }
    sync(b);
    select_from(b, base);
}

static void prim_prism(MB* b, int sides, float r, float h) {
    if (sides < 3) sides = 3;
    const Color4 w = {1, 1, 1, 1};
    float z0 = -h * 0.5f, z1 = h * 0.5f;
    int base = b->m->vertex_count;

    for (int i = 0; i < sides; ++i) {
        float a = (float)i * pi2 / (float)sides;
        float c = cosf(a), s = sinf(a);
        emit_vertex(b, (VertexFormat){ {r * c, r * s, z0}, {c, s, 0}, w, {(float)i / sides, 0} });
        emit_vertex(b, (VertexFormat){ {r * c, r * s, z1}, {c, s, 0}, w, {(float)i / sides, 1} });
    }
    int c0 = b->m->vertex_count;
    emit_vertex(b, (VertexFormat){ {0, 0, z0}, {0, 0, -1}, w, {0.5f, 0.5f} });
    int c1 = b->m->vertex_count;
    emit_vertex(b, (VertexFormat){ {0, 0, z1}, {0, 0, 1}, w, {0.5f, 0.5f} });

    for (int i = 0; i < sides; ++i) {
        int i0 = base + 2 * i, i1 = base + 2 * ((i + 1) % sides);
        add_triangle(b->m, i0, i1, i1 + 1);
        add_triangle(b->m, i0, i1 + 1, i0 + 1);
        add_triangle(b->m, c0, i1, i0);
        add_triangle(b->m, c1, i0 + 1, i1 + 1);
    }
    sync(b);
    select_from(b, base);
}

static void prim_quad(MB* b, Vec3 mn, Vec3 mx) {
    const Color4 w = {1, 1, 1, 1};
    Vec3 n = {0, 0, 1};
    int base = b->m->vertex_count;
    emit_vertex(b, (VertexFormat){ {mn.x, mn.y, mn.z}, n, w, {0, 0} });
    emit_vertex(b, (VertexFormat){ {mx.x, mn.y, mn.z}, n, w, {1, 0} });
    emit_vertex(b, (VertexFormat){ {mx.x, mx.y, mx.z}, n, w, {1, 1} });
    emit_vertex(b, (VertexFormat){ {mn.x, mx.y, mx.z}, n, w, {0, 1} });
    add_triangle(b->m, base, base + 1, base + 2);
    add_triangle(b->m, base, base + 2, base + 3);
    sync(b);
    select_from(b, base);
}

static void duplicate_selection(MB* b, int count, Quat rot, Vec3 offset, int rotate_normals) {
    int vc = b->m->vertex_count;
    int tc = b->m->index_count / 3;

    int* map = (int*)malloc(sizeof(int) * vc);
    int first_new = vc;

    for (int k = 1; k < count; ++k) {
        for (int i = 0; i < vc; ++i) map[i] = -1;

        Quat q = (Quat){0, 0, 0, 1};
        for (int r = 0; r < k; ++r) q = quat_mul(rot, q);   // rot^k
        Vec3 off = vec3_scale(offset, (float)k);

        for (int t = 0; t < tc; ++t) {
            if (!tri_selected(b, t)) continue;
            unsigned src[3] = { b->m->indices[t*3], b->m->indices[t*3+1], b->m->indices[t*3+2] };
            unsigned dst[3];
            for (int e = 0; e < 3; ++e) {
                unsigned s = src[e];
                if (map[s] < 0) {
                    VertexFormat v = b->m->vertices[s];
                    v.position = vec3_add(quat_rotate_vec3(q, v.position), off);
                    if (rotate_normals) v.normal = quat_rotate_vec3(q, v.normal);
                    map[s] = add_vertex(b->m, v);
                }
                dst[e] = (unsigned)map[s];
            }
            add_triangle(b->m, dst[0], dst[1], dst[2]);
            mat_fit(b);
            b->mat[b->m->index_count / 3 - 1] = b->mat[t];
        }
    }
    free(map);

    sync(b);
    for (int i = first_new; i < b->m->vertex_count; ++i) b->sel[i] = 1;  // originals stay selected too
}

static void op_extrude(MB* b, float dist, int mode, Vec3 dir) {
    int vc = b->m->vertex_count;
    int tc = b->m->index_count / 3;

    int* map = (int*)malloc(sizeof(int) * vc);
    for (int i = 0; i < vc; ++i) map[i] = -1;

    for (int t = 0; t < tc; ++t) {
        if (!tri_selected(b, t)) continue;
        for (int e = 0; e < 3; ++e) {
            unsigned s = b->m->indices[t*3+e];
            if (map[s] >= 0) continue;
            VertexFormat v = b->m->vertices[s];
            Vec3 d = (mode == MB_ALONG_DIR) ? dir : v.normal;
            v.position = vec3_add(v.position, vec3_scale(d, dist));
            map[s] = add_vertex(b->m, v);
        }
    }

    int cap = tc * 3, ne = 0;
    int* ea = (int*)malloc(sizeof(int) * cap);
    int* eb = (int*)malloc(sizeof(int) * cap);
    int* en = (int*)calloc((size_t)cap, sizeof(int));
    for (int t = 0; t < tc; ++t) {
        if (!tri_selected(b, t)) continue;
        for (int e = 0; e < 3; ++e) {
            int x = (int)b->m->indices[t*3+e], y = (int)b->m->indices[t*3+(e+1)%3];
            int lo = x < y ? x : y, hi = x < y ? y : x;
            int f = -1;
            for (int i = 0; i < ne; ++i) if (ea[i] == lo && eb[i] == hi) { f = i; break; }
            if (f < 0) { ea[ne] = lo; eb[ne] = hi; en[ne] = 1; ne++; }
            else en[f]++;
        }
    }

    for (int t = 0; t < tc; ++t) {
        if (!tri_selected(b, t)) continue;
        unsigned a = b->m->indices[t*3], c = b->m->indices[t*3+1], d = b->m->indices[t*3+2];
        unsigned na = (unsigned)map[a], nc = (unsigned)map[c], nd = (unsigned)map[d];

        add_triangle(b->m, na, nc, nd);
        mat_fit(b); b->mat[b->m->index_count/3 - 1] = b->mat[t];

        unsigned tri[3] = {a, c, d};
        for (int e = 0; e < 3; ++e) {
            int x = (int)tri[e], y = (int)tri[(e+1)%3];
            int lo = x < y ? x : y, hi = x < y ? y : x;
            int shared = 0;
            for (int i = 0; i < ne; ++i) if (ea[i] == lo && eb[i] == hi) { shared = en[i]; break; }
            if (shared != 1) continue;

            unsigned nx = (unsigned)map[x], ny = (unsigned)map[y];
            add_triangle(b->m, (unsigned)x, (unsigned)y, ny);
            mat_fit(b); b->mat[b->m->index_count/3 - 1] = b->mat[t];
            add_triangle(b->m, (unsigned)x, ny, nx);
            mat_fit(b); b->mat[b->m->index_count/3 - 1] = b->mat[t];
        }
    }

    int w = 0;
    for (int t = 0; t < b->m->index_count / 3; ++t) {
        if (t < tc && tri_selected(b, t)) continue;
        b->m->indices[w*3]   = b->m->indices[t*3];
        b->m->indices[w*3+1] = b->m->indices[t*3+1];
        b->m->indices[w*3+2] = b->m->indices[t*3+2];
        b->mat[w] = b->mat[t];
        w++;
    }
    b->m->index_count = w * 3;

    sync(b);
    memset(b->sel, 0, (size_t)b->sel_cap);
    for (int i = vc; i < b->m->vertex_count; ++i) b->sel[i] = 1;   // the cap

    free(map); free(ea); free(eb); free(en);
}

static void finalise_materials(MB* b) {
    int tc = b->m->index_count / 3;
    if (tc <= 0) return;

    int count[256] = {0}, start[256];
    for (int t = 0; t < tc; ++t) count[b->mat[t]]++;

    int used = 0, run = 0;
    for (int i = 0; i < 256; ++i) { start[i] = run; run += count[i]; if (count[i]) used++; }

    unsigned* out = (unsigned*)malloc(sizeof(unsigned) * (size_t)tc * 3);
    int cursor[256];
    memcpy(cursor, start, sizeof cursor);
    for (int t = 0; t < tc; ++t) {
        int slot = cursor[b->mat[t]]++;
        out[slot*3]   = b->m->indices[t*3];
        out[slot*3+1] = b->m->indices[t*3+1];
        out[slot*3+2] = b->m->indices[t*3+2];
    }
    memcpy(b->m->indices, out, sizeof(unsigned) * (size_t)tc * 3);
    free(out);

    free(b->m->submeshes);
    b->m->submeshes = (SubMesh*)malloc(sizeof(SubMesh) * (size_t)used);
    b->m->submesh_count = 0;
    for (int i = 0; i < 256; ++i) {
        if (!count[i]) continue;
        b->m->submeshes[b->m->submesh_count++] = (SubMesh){
            .index_offset = start[i] * 3,
            .index_count  = count[i] * 3,
            .material_id  = (unsigned)i
        };
    }
}

static void run(MB* b, const unsigned char* pc, const unsigned char* const* lib, int depth) {
    if (depth > MB_MAX_DEPTH) return;

    for (;;) {
        int op = rd_u8(&pc);
        switch (op) {
        case MB_END: return;

        case MB_BOX:   { Vec3 mn = rd_v(&pc), mx = rd_v(&pc); prim_box(b, mn, mx); } break;
        case MB_QUAD:  { Vec3 mn = rd_v(&pc), mx = rd_v(&pc); prim_quad(b, mn, mx); } break;
        case MB_PRISM: { int n = rd_u8(&pc); float r = rd_f(&pc), h = rd_f(&pc); prim_prism(b, n, r, h); } break;

        case MB_SEL_ALL:  sync(b); memset(b->sel, 1, (size_t)b->m->vertex_count); break;
        case MB_SEL_NONE: sync(b); memset(b->sel, 0, (size_t)b->sel_cap); break;

        case MB_SEL_BOX: {
            Vec3 mn = rd_v(&pc), mx = rd_v(&pc);
            sync(b);
            for (int i = 0; i < b->m->vertex_count; ++i) {
                Vec3 p = b->m->vertices[i].position;
                b->sel[i] &= (p.x >= mn.x && p.y >= mn.y && p.z >= mn.z &&
                              p.x <= mx.x && p.y <= mx.y && p.z <= mx.z);
            }
        } break;

        case MB_SEL_SPHERE: {
            Vec3 c = rd_v(&pc); float r = rd_f(&pc);
            sync(b);
            for (int i = 0; i < b->m->vertex_count; ++i) {
                Vec3 d = vec3_sub(b->m->vertices[i].position, c);
                b->sel[i] &= vec3_dot(d, d) <= r * r;
            }
        } break;

        case MB_SEL_NORMAL: {
            Vec3 d = vec3_normalize(rd_v(&pc)); float md = rd_f(&pc);
            sync(b);
            for (int i = 0; i < b->m->vertex_count; ++i)
                b->sel[i] &= vec3_dot(b->m->vertices[i].normal, d) >= md;
        } break;

        case MB_SEL_GROW: {
            sync(b);
            unsigned char* g = (unsigned char*)malloc((size_t)b->sel_cap);
            memcpy(g, b->sel, (size_t)b->sel_cap);
            for (int t = 0; t < b->m->index_count / 3; ++t) {
                const unsigned* ix = b->m->indices + t * 3;
                if (b->sel[ix[0]] || b->sel[ix[1]] || b->sel[ix[2]])
                    g[ix[0]] = g[ix[1]] = g[ix[2]] = 1;
            }
            memcpy(b->sel, g, (size_t)b->sel_cap);
            free(g);
        } break;

        case MB_PUSH_SEL:
            if (b->sp < MB_SEL_STACK) {
                b->stack[b->sp] = (unsigned char*)malloc((size_t)b->sel_cap);
                memcpy(b->stack[b->sp], b->sel, (size_t)b->sel_cap);
                b->sp++;
            }
            break;

        case MB_POP_SEL:
            if (b->sp > 0) {
                b->sp--;
                memcpy(b->sel, b->stack[b->sp], (size_t)b->sel_cap);
                free(b->stack[b->sp]);
                b->stack[b->sp] = 0;
            }
            break;

        case MB_TRANSLATE: {
            Vec3 d = rd_v(&pc);
            sync(b);
            for (int i = 0; i < b->m->vertex_count; ++i)
                if (b->sel[i]) b->m->vertices[i].position = vec3_add(b->m->vertices[i].position, d);
        } break;

        case MB_SCALE: {
            Vec3 pv = rd_v(&pc), k = rd_v(&pc);
            sync(b);
            for (int i = 0; i < b->m->vertex_count; ++i) {
                if (!b->sel[i]) continue;
                Vec3 p = vec3_sub(b->m->vertices[i].position, pv);
                b->m->vertices[i].position = vec3_add(pv, (Vec3){p.x * k.x, p.y * k.y, p.z * k.z});
            }
        } break;

        case MB_ROTATE: {
            Vec3 ax = vec3_normalize(rd_v(&pc));
            float ang = (float)rd_s16(&pc) * DEG2RAD;
            Quat q = quat_axis_angle(ax.x, ax.y, ax.z, ang);
            sync(b);
            for (int i = 0; i < b->m->vertex_count; ++i) {
                if (!b->sel[i]) continue;
                b->m->vertices[i].position = quat_rotate_vec3(q, b->m->vertices[i].position);
                b->m->vertices[i].normal   = quat_rotate_vec3(q, b->m->vertices[i].normal);
            }
        } break;

        case MB_MIRROR: {
            Vec3 n = vec3_normalize(rd_v(&pc));
            float d = rd_f(&pc);
            int dup = rd_u8(&pc);
            sync(b);
            if (!dup) {
                for (int i = 0; i < b->m->vertex_count; ++i) {
                    if (!b->sel[i]) continue;
                    VertexFormat* v = &b->m->vertices[i];
                    float t = vec3_dot(v->position, n) - d;
                    v->position = vec3_sub(v->position, vec3_scale(n, 2.0f * t));
                    v->normal   = vec3_sub(v->normal, vec3_scale(n, 2.0f * vec3_dot(v->normal, n)));
                }
            } else {
                int vc = b->m->vertex_count, tc = b->m->index_count / 3;
                int* map = (int*)malloc(sizeof(int) * vc);
                for (int i = 0; i < vc; ++i) map[i] = -1;
                for (int t = 0; t < tc; ++t) {
                    if (!tri_selected(b, t)) continue;
                    unsigned src[3] = { b->m->indices[t*3], b->m->indices[t*3+1], b->m->indices[t*3+2] };
                    unsigned dst[3];
                    for (int e = 0; e < 3; ++e) {
                        unsigned s = src[e];
                        if (map[s] < 0) {
                            VertexFormat v = b->m->vertices[s];
                            float tt = vec3_dot(v.position, n) - d;
                            v.position = vec3_sub(v.position, vec3_scale(n, 2.0f * tt));
                            v.normal   = vec3_sub(v.normal, vec3_scale(n, 2.0f * vec3_dot(v.normal, n)));
                            map[s] = add_vertex(b->m, v);
                        }
                        dst[e] = (unsigned)map[s];
                    }
                    add_triangle(b->m, dst[0], dst[2], dst[1]);
                    mat_fit(b); b->mat[b->m->index_count/3 - 1] = b->mat[t];
                }
                free(map);
                sync(b);
                for (int i = vc; i < b->m->vertex_count; ++i) b->sel[i] = 1;
            }
        } break;

        case MB_EXTRUDE: {
            float d = rd_f(&pc);
            int mode = rd_u8(&pc);
            Vec3 dir = {0, 0, 1};
            if (mode == MB_ALONG_DIR) dir = vec3_normalize(rd_v(&pc));
            op_extrude(b, d, mode, dir);
        } break;

        case MB_ARRAY_CIRCLE: {
            int n = rd_u8(&pc);
            Vec3 ax = vec3_normalize(rd_v(&pc));
            if (n > 1) {
                Quat step = quat_axis_angle(ax.x, ax.y, ax.z, pi2 / (float)n);
                duplicate_selection(b, n, step, (Vec3){0,0,0}, 1);
            }
        } break;

        case MB_ARRAY_LINEAR: {
            int n = rd_u8(&pc);
            Vec3 off = rd_v(&pc);
            if (n > 1) duplicate_selection(b, n, (Quat){0,0,0,1}, off, 0);
        } break;

        case MB_MATERIAL: {
            int id = rd_u8(&pc);
            sync(b);
            b->cur_mat = (unsigned char)id;
            for (int t = 0; t < b->m->index_count / 3; ++t)
                if (tri_selected(b, t)) b->mat[t] = (unsigned char)id;
        } break;

        case MB_COLOR: {
            Vec3 c = rd_v(&pc);
            sync(b);
            for (int i = 0; i < b->m->vertex_count; ++i)
                if (b->sel[i]) b->m->vertices[i].color = (Color4){c.x, c.y, c.z, 1.0f};
        } break;

        case MB_CALL: {
            int idx = rd_u8(&pc);
            if (lib && lib[idx]) {
                int before = b->m->vertex_count;
                run(b, lib[idx], lib, depth + 1);
                sync(b);
                select_from(b, before);
            }
        } break;

        default: return;
        }
    }
}

void mesh_build(Mesh* m, const unsigned char* code, const unsigned char* const* lib) {
    if (!m || !code) return;

    MB b = {0};
    b.m = m;
    sync(&b);

    run(&b, code, lib, 0);

    finalise_materials(&b);

    for (int i = 0; i < b.sp; ++i) free(b.stack[i]);
    free(b.sel);
    free(b.mat);
}
