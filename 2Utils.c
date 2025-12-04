#include "defs.h"
#include <time.h>
static uint32_t seed = 12355;
#define EPSILON 0.0001f

uint32_t xorshift32() {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}
float xorshift32f() {
    return (float)xorshift32() / 4294967295.0f;
}
uint32_t wangHash(uint32_t seed) {
    seed = (seed ^ 61) ^ (seed >> 16);
    seed = seed + (seed << 3);
    seed = seed ^ (seed >> 4);
    seed = seed * 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}
float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

float smoothstep(const float edge0, const float edge1, const float x) {
    float t = (x - edge0) / (edge1 - edge0);
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    return t * t * (3.0f - 2.0f * t);
}

float hashf3(Vec3 p) {
    uint32_t h = (uint32_t)(p.x * 374761393.0f + p.y * 668265263.0f + p.z * 982451653.0f);
    h ^= h >> 13;
    h *= 1274126177;
    h ^= h >> 16;
    return (0xFFFFFF & h) / (float)0xFFFFFF;
}

float noise3f(Vec3 p) {
    Vec3 ip = { floorf(p.x), floorf(p.y), floorf(p.z) };
    Vec3 fp = { p.x - ip.x, p.y - ip.y, p.z - ip.z };

    float acc = 0.0f;
    for (int dz = 0; dz <= 1; dz++) {
        for (int dy = 0; dy <= 1; dy++) {
            for (int dx = 0; dx <= 1; dx++) {
                Vec3 corner = { ip.x + dx, ip.y + dy, ip.z + dz };
                float val = hashf3(corner);
                float wx = smoothstep(0, 1, dx ? fp.x : 1 - fp.x);
                float wy = smoothstep(0, 1, dy ? fp.y : 1 - fp.y);
                float wz = smoothstep(0, 1, dz ? fp.z : 1 - fp.z);
                acc += val * wx * wy * wz;
            }
        }
    }
    return acc;
}
float fbm(Vec3 p, int octaves, float persistence, float lacunarity) {
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float total = 0.0f;

    for (int i = 0; i < octaves; i++) {
        const Vec3 sample = {
            p.x * frequency,
            p.y * frequency,
            p.z * frequency
        };

        total += noise3f(sample) * amplitude;

        amplitude *= persistence;
        frequency *= lacunarity;
    }

    return total;
}


Vec3 vec3_normalize(Vec3 v) {
    float length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length > 0.00001f) {
        v.x /= length;
        v.y /= length;
        v.z /= length;
    } else {
        v = (Vec3){0, 1, 0}; // fallback
    }
    return v;
}
Vec3 vec3_sub(Vec3 a, Vec3 b) {
    return (Vec3){ a.x - b.x, a.y - b.y, a.z - b.z };
}
Vec3 vec3_cross(Vec3 a, Vec3 b) {
    return (Vec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}
Vec3 vec3_add(Vec3 a, Vec3 b) {
    return (Vec3){ a.x + b.x, a.y + b.y, a.z + b.z };
}
Vec3 vec3_scale(Vec3 v, float s) {
    return (Vec3){ v.x * s, v.y * s, v.z * s };
}
float vec3_dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
Vec3 vec3_invert(Vec3 v) {
    return (Vec3){ -v.x, -v.y, -v.z };
}
Vec3 quat_rotate_vec3(Quat q, Vec3 v) {
    // q * v * conj(q)
    Vec3 out;

    Vec3 u = { q.x, q.y, q.z };

    Vec3 uv = {
        u.y * v.z - u.z * v.y,
        u.z * v.x - u.x * v.z,
        u.x * v.y - u.y * v.x
    };

    Vec3 uuv = {
        u.y * uv.z - u.z * uv.y,
        u.z * uv.x - u.x * uv.z,
        u.x * uv.y - u.y * uv.x
    };

    uv.x *= 2.0f * q.w;
    uv.y *= 2.0f * q.w;
    uv.z *= 2.0f * q.w;

    uuv.x *= 2.0f;
    uuv.y *= 2.0f;
    uuv.z *= 2.0f;

    out.x = v.x + uv.x + uuv.x;
    out.y = v.y + uv.y + uuv.y;
    out.z = v.z + uv.z + uuv.z;

    return out;
}
Quat quat_mul(Quat a, Quat b) {
    Quat q;
    q.w = a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z;
    q.x = a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y;
    q.y = a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x;
    q.z = a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w;
    return q;
}
Quat quat_axis_angle(float x, float y, float z, float angle_rad) {
    float s = sinf(angle_rad * 0.5f);
    Quat q;
    q.x = x * s;
    q.y = y * s;
    q.z = z * s;
    q.w = cosf(angle_rad * 0.5f);
    return q;
}
Quat quat_normalize(Quat q) {
    float mag = sqrtf(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
    q.x /= mag;
    q.y /= mag;
    q.z /= mag;
    q.w /= mag;
    return q;
}
Quat quat_conjugate(Quat q) {
    return (Quat){ -q.x, -q.y, -q.z, q.w };
}
Quat quat_from_euler(float pitch, float yaw) {
    Quat qx = quat_axis_angle(1, 0, 0, pitch);
    Quat qy = quat_axis_angle(0, 1, 0, yaw);
    return quat_mul(qy, qx); // yaw first, then pitch
}


static inline int float_equals(float a, float b) {
    return fabsf(a - b) < EPSILON;
}

int vec3_equals(Vec3 a, Vec3 b) {
    return float_equals(a.x, b.x) &&
           float_equals(a.y, b.y) &&
           float_equals(a.z, b.z);
}

int vec4_equals(Vec4 a, Vec4 b) {
    return float_equals(a.x, b.x) &&
           float_equals(a.y, b.y) &&
           float_equals(a.z, b.z) &&
           float_equals(a.w, b.w);
}

int uv_equals(UV a, UV b) {
    return float_equals(a.u, b.u) &&
           float_equals(a.v, b.v);
}

int color4_equals(Color4 a, Color4 b) {
    return float_equals(a.r, b.r) &&
           float_equals(a.g, b.g) &&
           float_equals(a.b, b.b) &&
           float_equals(a.a, b.a);
}

int vertex_equals(const VertexFormat* a, const VertexFormat* b) {
    return vec3_equals(a->position, b->position) &&
           vec3_equals(a->normal,   b->normal)   &&
           color4_equals(a->color,  b->color)    &&
           uv_equals(a->uv,         b->uv);
}
void mat4_perspective(float* out, float fovY_deg, float aspect, float zNear, float zFar) {
    float f = 1.0f / tanf((fovY_deg * 3.14159265f / 180.0f) / 2.0f);

    out[0] = f / aspect;
    out[1] = 0.0f;
    out[2] = 0.0f;
    out[3] = 0.0f;

    out[4] = 0.0f;
    out[5] = f;
    out[6] = 0.0f;
    out[7] = 0.0f;

    out[8] = 0.0f;
    out[9] = 0.0f;
    out[10] = (zFar + zNear) / (zNear - zFar);
    out[11] = -1.0f;

    out[12] = 0.0f;
    out[13] = 0.0f;
    out[14] = (2.0f * zFar * zNear) / (zNear - zFar);
    out[15] = 0.0f;
}
void mat4_identity(Mat4 m) {
    memset(m, 0, sizeof(Mat4));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}
void mat4_translate(Mat4 m, float x, float y, float z) {
    m[12] += x;
    m[13] += y;
    m[14] += z;
}
void mat4_scale(Mat4 m, float sx, float sy, float sz) {
    m[0] *= sx; m[4] *= sx; m[8]  *= sx; m[12] *= sx;
    m[1] *= sy; m[5] *= sy; m[9]  *= sy; m[13] *= sy;
    m[2] *= sz; m[6] *= sz; m[10] *= sz; m[14] *= sz;
}
void mat4_multiply(Mat4 out, const Mat4 a, const Mat4 b) {
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            out[col * 4 + row] =
                a[0 * 4 + row] * b[col * 4 + 0] +
                a[1 * 4 + row] * b[col * 4 + 1] +
                a[2 * 4 + row] * b[col * 4 + 2] +
                a[3 * 4 + row] * b[col * 4 + 3];
        }
    }
}
void mat4_ortho(Mat4 out, float left, float right, float bottom, float top, float nearf, float farf) {
    memset(out, 0, sizeof(Mat4));
    out[0] = 2.0f / (right - left);
    out[5] = 2.0f / (top - bottom);
    out[10] = -2.0f / (farf - nearf);
    out[12] = -(right + left) / (right - left);
    out[13] = -(top + bottom) / (top - bottom);
    out[14] = -(farf + nearf) / (farf - nearf);
    out[15] = 1.0f;
}
void mat4_lookat(Mat4 m, Vec3 eye, Vec3 center, Vec3 up) {
    Vec3 f = vec3_normalize(vec3_sub(center, eye));
    Vec3 s = vec3_normalize(vec3_cross(f, up));
    Vec3 u = vec3_cross(s, f);

    mat4_identity(m);
    m[0] = s.x; m[4] = s.y; m[8]  = s.z; m[12] = -vec3_dot(s, eye);
    m[1] = u.x; m[5] = u.y; m[9]  = u.z; m[13] = -vec3_dot(u, eye);
    m[2] = -f.x; m[6] = -f.y; m[10] = -f.z; m[14] = vec3_dot(f, eye);
}

void quat_to_matrix(const Quat* q, Mat4 m) {
    float x = q->x, y = q->y, z = q->z, w = q->w;

    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, yy = y * y2, zz = z * z2;
    float xy = x * y2, xz = x * z2, yz = y * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;

    m[0] = 1.0f - (yy + zz);  m[1] = xy + wz;        m[2] = xz - wy;        m[3] = 0.0f;
    m[4] = xy - wz;           m[5] = 1.0f - (xx + zz); m[6] = yz + wx;        m[7] = 0.0f;
    m[8] = xz + wy;           m[9] = yz - wx;        m[10] = 1.0f - (xx + yy); m[11] = 0.0f;
    m[12] = 0.0f;             m[13] = 0.0f;          m[14] = 0.0f;          m[15] = 1.0f;
}


void init_mesh(Mesh* mesh, int vertex_capacity, int index_capacity) {
    mesh->vertex_count = 0;
    mesh->index_count = 0;
    mesh->vertex_capacity = vertex_capacity;
    mesh->index_capacity = index_capacity;

    mesh->vertices = malloc(sizeof(VertexFormat) * vertex_capacity);
    mesh->indices  = malloc(sizeof(unsigned) * index_capacity);

    mesh->submeshes = NULL;
    mesh->submesh_count = 0;

    mesh->vao = 0; mesh->vbo = 0; mesh->ibo = 0;
    glGenVertexArrays(1, &mesh->vao);
    glGenBuffers(1, &mesh->vbo);
    glGenBuffers(1, &mesh->ibo);
}

int find_or_add_vertex(Mesh* mesh, VertexFormat v) {
    for (int i = 0; i < mesh->vertex_count; i++) {
        if (vertex_equals(&mesh->vertices[i], &v)) {
            return i;
        }
    }

    if (mesh->vertex_count >= mesh->vertex_capacity) {
        mesh->vertex_capacity *= 2;
        mesh->vertices = realloc(mesh->vertices, sizeof(VertexFormat) * mesh->vertex_capacity);
    }

    int index = mesh->vertex_count++;
    mesh->vertices[index] = v;
    return index;
}

int add_vertex(Mesh* mesh, VertexFormat v) {
    ensure_v(mesh, 1);
    int index = mesh->vertex_count++;
    mesh->vertices[index] = v;
    return index;
}

void add_triangle(Mesh* mesh, unsigned int i0, unsigned int i1, unsigned int i2) {
    ensure_i(mesh, 3);
    mesh->indices[mesh->index_count++] = i0;
    mesh->indices[mesh->index_count++] = i1;
    mesh->indices[mesh->index_count++] = i2;
}

void draw_mesh(Mesh* mesh, GLenum primitive_type) {
    if (!mesh || !mesh->vao || mesh->index_count == 0) return;

    glBindVertexArray(mesh->vao);
    glDrawElements(primitive_type, mesh->index_count, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}
void upload_mesh(Mesh* mesh) {
    if (!mesh->vao) glGenVertexArrays(1, &mesh->vao);
    if (!mesh->vbo) glGenBuffers(1, &mesh->vbo);
    if (!mesh->ibo) glGenBuffers(1, &mesh->ibo);

    glBindVertexArray(mesh->vao);

    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(mesh->vertex_count * sizeof(VertexFormat)),
                 mesh->vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 (GLsizeiptr)(mesh->index_count * sizeof(unsigned int)),
                 mesh->indices, GL_STATIC_DRAW);

    GLsizei stride = sizeof(VertexFormat);
    size_t offset = 0;

    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offset);
    offset += sizeof(Vec3);

    glEnableVertexAttribArray(1); // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offset);
    offset += sizeof(Vec3);

    glEnableVertexAttribArray(3); // color
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)offset);
    offset += sizeof(Color4);

    glEnableVertexAttribArray(4); // uv
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, stride, (void*)offset);
    offset += sizeof(UV);

    glDisableVertexAttribArray(2); // tangent
    glDisableVertexAttribArray(5); // roughness
    glDisableVertexAttribArray(6); // metallic
    glDisableVertexAttribArray(7); // emissive

    glBindVertexArray(0);
}

void draw_object(const Object* obj, const ShaderProgram* shader, Mesh* mesh_pool[], const GLenum primitive_type) {

    const Mesh* mesh = mesh_pool[obj->mesh_id];
    if (!mesh || !(obj->flags & OBJ_FLAG_VISIBLE)) return;

    Mat4 model, trans, rot, scale, trs;
    mat4_identity(model);
    mat4_identity(trans);
    mat4_translate(trans, obj->position.x, obj->position.y, obj->position.z);
    quat_to_matrix(&obj->rotation, rot);
    mat4_identity(scale);
    mat4_scale(scale, obj->scale.x, obj->scale.y, obj->scale.z);
    mat4_multiply(trs, rot, scale);
    mat4_multiply(model, trans, trs);
    glUniformMatrix4fv(shader->u_model_loc, 1, GL_FALSE, model);

    if (mesh->submesh_count <= 0) {
        const Material* mat = material_pool[(unsigned)mesh->material_id];
        if (mat) {
            glUniform4f(shader->u_material_albedo_loc,    mat->albedo.r, mat->albedo.g, mat->albedo.b, mat->albedo.a);
            glUniform1f(shader->u_material_roughness_loc, mat->roughness);
            glUniform1f(shader->u_material_metallic_loc,  mat->metallic);
            glUniform1f(shader->u_material_emissive_loc,  mat->emissive);
        }
        draw_mesh((Mesh*)mesh, primitive_type);
        return;
    }

    glBindVertexArray(mesh->vao);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (int pass = 0; pass < 2; ++pass) {
        const int transp = (pass == 1);
        glDepthMask(transp ? GL_FALSE : GL_TRUE);
        if (transp) glEnable(GL_BLEND); else glDisable(GL_BLEND);

        for (int i = 0; i < mesh->submesh_count; ++i) {
            const SubMesh* s = &mesh->submeshes[i];
            const Material* mat = material_pool[s->material_id];
            if (!mat) continue;

            const int isT = (mat->albedo.a < 0.999f);
            if (isT != transp) continue;

            glUniform4f(shader->u_material_albedo_loc,    mat->albedo.r, mat->albedo.g, mat->albedo.b, mat->albedo.a);
            glUniform1f(shader->u_material_roughness_loc, mat->roughness);
            glUniform1f(shader->u_material_metallic_loc,  mat->metallic);
            glUniform1f(shader->u_material_emissive_loc,  mat->emissive);

            glDrawElements(primitive_type, s->index_count, GL_UNSIGNED_INT,
                           (void*)(sizeof(unsigned) * s->index_offset));
        }
    }

    glDepthMask(GL_TRUE);
    glBindVertexArray(0);

}




void ensure_v(Mesh* m, int add){
    int need = m->vertex_count + add;
    if (need > m->vertex_capacity){
        m->vertex_capacity = need*2 + 8;
        m->vertices = realloc(m->vertices, sizeof(VertexFormat)*m->vertex_capacity);
    }
}
void ensure_i(Mesh* m, int add){
    int need = m->index_count + add;
    if (need > m->index_capacity){
        m->index_capacity = need*2 + 8;
        m->indices = realloc(m->indices, sizeof(unsigned)*m->index_capacity);
    }
}

static Vec3 v3(float x,float y,float z){ return (Vec3){x,y,z}; }
static Vec3 vadd(Vec3 a, Vec3 b){ return (Vec3){a.x+b.x,a.y+b.y,a.z+b.z}; }
static Vec3 vsub(Vec3 a, Vec3 b){ return (Vec3){a.x-b.x,a.y-b.y,a.z-b.z}; }
static Vec3 vmad(Vec3 a, Vec3 b, float s){ return (Vec3){a.x+b.x*s,a.y+b.y*s,a.z+b.z*s}; }

Mask mask_all(const Mesh* m){ Mask s=(Mask)malloc((size_t)m->vertex_count); memset(s,1,(size_t)m->vertex_count); return s; }
Mask mask_box(const Mesh* m, Vec3 mn, Vec3 mx){
    Mask s=(Mask)calloc((size_t)m->vertex_count,1);
    for(int i=0;i<m->vertex_count;i++){
        Vec3 p=m->vertices[i].position;
        s[i]=(p.x>=mn.x&&p.y>=mn.y&&p.z>=mn.z&&p.x<=mx.x&&p.y<=mx.y&&p.z<=mx.z);
    } return s;
}
Mask mask_sphere(const Mesh* m, Vec3 c, float r){
    float r2=r*r; Mask s=(Mask)calloc((size_t)m->vertex_count,1);
    for(int i=0;i<m->vertex_count;i++){ Vec3 p=vsub(m->vertices[i].position,c);
        s[i]=(p.x*p.x+p.y*p.y+p.z*p.z)<=r2;
    } return s;
}
int  mask_count(const Mesh* m, const Mask s){ int n=0; for(int i=0;i<m->vertex_count;i++) n+=s[i]!=0; return n; }

Mask mask_grow(const Mesh* m, const Mask in){
    Mask out=(Mask)malloc((size_t)m->vertex_count); memcpy(out,in,(size_t)m->vertex_count);
    for(int i=0;i<m->index_count;i+=3){
        unsigned a=m->indices[i],b=m->indices[i+1],c=m->indices[i+2];
        if(in[a]||in[b]||in[c]){ out[a]=out[b]=out[c]=1; }
    } return out;
}
Mask mask_shrink_fulltri(const Mesh* m, const Mask in){
    Mask keep=(Mask)calloc((size_t)m->vertex_count,1);
    for(int i=0;i<m->index_count;i+=3){
        unsigned a=m->indices[i],b=m->indices[i+1],c=m->indices[i+2];
        if(in[a]&&in[b]&&in[c]) keep[a]=keep[b]=keep[c]=1;
    } return keep;
}
Mask mask_not(const Mesh* m, const Mask a){
    Mask o=(Mask)malloc((size_t)m->vertex_count);
    for(int i=0;i<m->vertex_count;i++) o[i]=!a[i];
    return o;
}
Mask mask_and(const Mesh* m, const Mask a, const Mask b){
    Mask o=(Mask)malloc((size_t)m->vertex_count);
    for(int i=0;i<m->vertex_count;i++) o[i]=a[i]&&b[i];
    return o;
}
Mask mask_andnot(const Mesh* m, const Mask a, const Mask b){
    Mask o=(Mask)malloc((size_t)m->vertex_count);
    for(int i=0;i<m->vertex_count;i++) o[i]=a[i]&&!b[i];
    return o;
}

void sel_translate(Mesh* m, Mask s, Vec3 d){
    for(int i=0;i<m->vertex_count;i++) if(s[i]) m->vertices[i].position=vadd(m->vertices[i].position,d);
}
void sel_scale(Mesh* m, Mask s, Vec3 pivot, Vec3 k){
    for(int i=0;i<m->vertex_count;i++) if(s[i]){
        Vec3 p=vsub(m->vertices[i].position,pivot);
        p.x*=k.x; p.y*=k.y; p.z*=k.z;
        m->vertices[i].position=vadd(p,pivot);
    }
}
void sel_rotate(Mesh* m, const Mask s, Vec3 pivot, Vec3 axis, float ang){
    Quat q=quat_axis_angle(axis.x,axis.y,axis.z,ang);
    for(int i=0;i<m->vertex_count;i++) if(s[i]){
        Vec3 p=vsub(m->vertices[i].position,pivot);
        p=quat_rotate_vec3(q,p);
        m->vertices[i].position=vadd(p,pivot);
        m->vertices[i].normal = vec3_normalize(quat_rotate_vec3(q,m->vertices[i].normal));
    }
}

// whole-mesh transforms
void mesh_scale_all(Mesh* m, Vec3 k){
    Mask s=mask_all(m); sel_scale(m,s,(Vec3){0,0,0},k); free(s);
}
void mesh_translate_all(Mesh* m, Vec3 d){
    Mask s=mask_all(m); sel_translate(m,s,d); free(s);
}

static Vec3 reflect(Vec3 p, Vec3 n, float d){
    float t=(n.x*p.x+n.y*p.y+n.z*p.z+d)*2.0f;
    return (Vec3){p.x-n.x*t,p.y-n.y*t,p.z-n.z*t};
}

Mask sel_mirror(Mesh* m, const Mask s, Vec3 n, float d, char duplicate){
    int i;

    if (!duplicate) {
        if (s) {
            for (i = 0; i < m->vertex_count; ++i)
                if (s[i])
                    m->vertices[i].position = reflect(m->vertices[i].position, n, d);
        } else {
            for (i = 0; i < m->vertex_count; ++i)
                m->vertices[i].position = reflect(m->vertices[i].position, n, d);
        }
        return NULL;
    }

    const int vc = m->vertex_count;
    const int ic = m->index_count;
    const int all = (s == NULL);

    int *map = (int*)malloc(sizeof(int) * vc);
    for (i = 0; i < vc; ++i) map[i] = -1;

    for (i = 0; i < vc; ++i) if (all || s[i]) {
        ensure_v(m, 1);
        map[i] = m->vertex_count++;
        m->vertices[map[i]] = m->vertices[i];
        m->vertices[map[i]].position = reflect(m->vertices[i].position, n, d);
        m->vertices[map[i]].normal   = vec3_normalize(reflect(m->vertices[i].normal, n, 0));
    }

    for (i = 0; i < ic; i += 3) {
        unsigned a = m->indices[i], b = m->indices[i+1], c = m->indices[i+2];

        if (all || s[a] || s[b] || s[c]) {
            ensure_i(m, 3);

            unsigned na = (all || s[a]) ? (unsigned)map[a] : a;
            unsigned nb = (all || s[b]) ? (unsigned)map[b] : b;
            unsigned nc = (all || s[c]) ? (unsigned)map[c] : c;

            m->indices[m->index_count++] = na;
            m->indices[m->index_count++] = nc;  // mirrored winding
            m->indices[m->index_count++] = nb;
        }
    }

    Mask out = (Mask)calloc((size_t)m->vertex_count, 1);
    for (i = 0; i < vc; ++i)
        if (map[i] >= 0)
            out[map[i]] = 1;

    free(map);
    return out;
}





Mask sel_extrude_tris(Mesh* m, Mask s, Vec3 dir, float dist, int keep_base){
    int vc=m->vertex_count, ic=m->index_count;
    int *map=(int*)malloc(sizeof(int)*vc);
    for(int i=0;i<vc;i++) map[i]=-1;

    // Create extruded vertices
    for(int i=0;i<ic;i+=3){
        unsigned a=m->indices[i],b=m->indices[i+1],c=m->indices[i+2];
        if(!(s[a]||s[b]||s[c])) continue;
        if(map[a]<0){ ensure_v(m,1); map[a]=m->vertex_count++; m->vertices[map[a]]=m->vertices[a]; m->vertices[map[a]].position=vmad(m->vertices[a].position,dir,dist); }
        if(map[b]<0){ ensure_v(m,1); map[b]=m->vertex_count++; m->vertices[map[b]]=m->vertices[b]; m->vertices[map[b]].position=vmad(m->vertices[b].position,dir,dist); }
        if(map[c]<0){ ensure_v(m,1); map[c]=m->vertex_count++; m->vertices[map[c]]=m->vertices[c]; m->vertices[map[c]].position=vmad(m->vertices[c].position,dir,dist); }
    }

    int new_index_start = m->index_count;
    for(int i=0;i<ic;i+=3){
        unsigned a=m->indices[i],b=m->indices[i+1],c=m->indices[i+2];
        if(!(s[a]&&s[b]&&s[c])) continue;

        unsigned na=(unsigned)map[a], nb=(unsigned)map[b], nc=(unsigned)map[c];

        // Top face
        ensure_i(m,15);
        m->indices[m->index_count++]=na;
        m->indices[m->index_count++]=nb;
        m->indices[m->index_count++]=nc;

        // Side faces
        m->indices[m->index_count++]=a; m->indices[m->index_count++]=b; m->indices[m->index_count++]=nb;
        m->indices[m->index_count++]=a; m->indices[m->index_count++]=nb; m->indices[m->index_count++]=na;

        m->indices[m->index_count++]=b; m->indices[m->index_count++]=c; m->indices[m->index_count++]=nc;
        m->indices[m->index_count++]=b; m->indices[m->index_count++]=nc; m->indices[m->index_count++]=nb;

        m->indices[m->index_count++]=c; m->indices[m->index_count++]=a; m->indices[m->index_count++]=na;
        m->indices[m->index_count++]=c; m->indices[m->index_count++]=na; m->indices[m->index_count++]=nc;
    }

    // Remove base triangles if requested
    if(!keep_base) {
        int write = 0;
        for(int read=0; read<ic; read+=3){
            unsigned a=m->indices[read],b=m->indices[read+1],c=m->indices[read+2];
            // Keep triangles that are NOT fully selected
            if(!(s[a]&&s[b]&&s[c])) {
                m->indices[write++]=a;
                m->indices[write++]=b;
                m->indices[write++]=c;
            }
        }
        for(int i=new_index_start; i<m->index_count; i++) {
            m->indices[write++] = m->indices[i];
        }
        m->index_count = write;
    }

    // Set normals for extruded vertices
    for(int i=0;i<vc;i++) if(map[i]>=0) m->vertices[map[i]].normal=vec3_normalize(dir);

    Mask out=(Mask)calloc((size_t)m->vertex_count,1);
    for(int i=0;i<vc;i++) if(map[i]>=0) out[map[i]]=1;
    free(map);
    return out;
}
int assign_submesh_from_mask(Mesh* m, Mask s, unsigned material_id){
    if(!m || !m->indices || !s) return -1;

    int start = m->index_count;
    int ic    = m->index_count;
    int added = 0;

    for (int i = 0; i < ic; i += 3) {
        unsigned a = m->indices[i], b = m->indices[i+1], c = m->indices[i+2];
        if (s[a] && s[b] && s[c]) {
            ensure_i(m, 3);
            m->indices[m->index_count++] = a;
            m->indices[m->index_count++] = b;
            m->indices[m->index_count++] = c;
            added += 3;
        }
    }

    if (!added) return -1;

    m->submeshes = (SubMesh*)realloc(m->submeshes, sizeof(SubMesh)*(m->submesh_count+1));
    int idx = m->submesh_count++;
    m->submeshes[idx] = (SubMesh){ .index_offset = start, .index_count = added, .material_id = material_id };
    return idx;
}

void mesh_clear(Mesh* m){ m->vertex_count=0; m->index_count=0; m->submesh_count=0; }

// 8-vert box, 12 tris
void make_box(Mesh* m, Vec3 mn, Vec3 mx){
    mesh_clear(m); ensure_v(m,8); ensure_i(m,36);
    Color4 white={1,1,1,1}; UV zuv={0,0};
    int base=m->vertex_count;
    Vec3 p[8]={ v3(mn.x,mn.y,mn.z), v3(mx.x,mn.y,mn.z), v3(mx.x,mx.y,mn.z), v3(mn.x,mx.y,mn.z),
                v3(mn.x,mn.y,mx.z), v3(mx.x,mn.y,mx.z), v3(mx.x,mx.y,mx.z), v3(mn.x,mx.y,mx.z) };
    for(int i=0;i<8;i++){ m->vertices[m->vertex_count++]=(VertexFormat){p[i],v3(0,0,1),white,zuv}; }
    unsigned q[36]={ 0,1,2,0,2,3, 4,6,5,4,7,6, 0,4,5,0,5,1, 1,5,6,1,6,2, 2,6,7,2,7,3, 3,7,4,3,4,0 };
    for(int i=0;i<36;i++) m->indices[m->index_count++]=base+q[i];
}

// regular N-gon prism with caps
void make_prism(Mesh* m, int sides, float r, float h){
    if(sides<3) sides=3;
    mesh_clear(m);
    ensure_v(m, (sides*2)+2); // rings + cap centers
    ensure_i(m, sides*12);
    Color4 white={1,1,1,1}; UV zuv={0,0};
    float z0=-h*0.5f, z1=h*0.5f;
    int base=m->vertex_count;
    // ring verts
    for(int i=0;i<sides;i++){
        float a=(float)i*(2.0f*3.14159265f)/sides; float c=cosf(a), s=sinf(a);
        m->vertices[m->vertex_count++]=(VertexFormat){ v3(r*c,r*s,z0), v3(c,s,0), white, zuv };
        m->vertices[m->vertex_count++]=(VertexFormat){ v3(r*c,r*s,z1), v3(c,s,0), white, zuv };
    }
    int c0=m->vertex_count++; m->vertices[c0]=(VertexFormat){ v3(0,0,z0), v3(0,0,-1), white, zuv };
    int c1=m->vertex_count++; m->vertices[c1]=(VertexFormat){ v3(0,0,z1), v3(0,0, 1), white, zuv };
    // sides
    for(int i=0;i<sides;i++){
        int i0=(base + 2*i), i1=(base + 2*((i+1)%sides));
        int j0=i0+1, j1=i1+1;
        ensure_i(m,6); m->indices[m->index_count++]=i0; m->indices[m->index_count++]=i1; m->indices[m->index_count++]=j1;
                       m->indices[m->index_count++]=i0; m->indices[m->index_count++]=j1; m->indices[m->index_count++]=j0;
        // cap bottom (fan)
        ensure_i(m,3); m->indices[m->index_count++]=c0; m->indices[m->index_count++]=i1; m->indices[m->index_count++]=i0;
        // cap top (fan)
        ensure_i(m,3); m->indices[m->index_count++]=c1; m->indices[m->index_count++]=j0; m->indices[m->index_count++]=j1;
    }
}

void make_cubesphere(Mesh* m, Vec3 center, float r, int subdivs){
    if(subdivs<1) subdivs=1;
    mesh_clear(m);
    const int faces=6;
    const int N=subdivs+1;
    ensure_v(m, faces*N*N); ensure_i(m, faces*subdivs*subdivs*6);
    Vec3 normals[6]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    Color4 white={1,1,1,1}; UV zuv={0,0};
    float step=2.0f/subdivs;
    for(int f=0;f<6;f++){
        const Vec3 n=normals[f];
        Vec3 ax=v3(0,0,0), ay=v3(0,0,0);
        if(f==0||f==1){ ax.z=1; ay.y=1; } else if(f==2||f==3){ ax.x=1; ay.z=1; } else { ax.x=1; ay.y=1; }
        int idx[N][N];
        for(int i=0;i<=subdivs;i++){
            for(int j=0;j<=subdivs;j++){
                float x=-1.0f+step*i, y=-1.0f+step*j;
                Vec3 p=vadd(n, vadd( (Vec3){ax.x*x,ax.y*x,ax.z*x}, (Vec3){ay.x*y,ay.y*y,ay.z*y} ));
                Vec3 dir=vec3_normalize(p);
                VertexFormat v={ v3(center.x+dir.x*r, center.y+dir.y*r, center.z+dir.z*r),
                                 dir, white, zuv };
                idx[i][j]=find_or_add_vertex(m, v);
            }
        }
        for(int i=0;i<subdivs;i++) for(int j=0;j<subdivs;j++){
            int a=idx[i][j], b=idx[i+1][j], c=idx[i][j+1], d=idx[i+1][j+1];
            ensure_i(m,6); m->indices[m->index_count++]=a; m->indices[m->index_count++]=b; m->indices[m->index_count++]=c;
                           m->indices[m->index_count++]=c; m->indices[m->index_count++]=b; m->indices[m->index_count++]=d;
        }
    }
}

void make_icosahedron_sphere(Mesh* m, Vec3 c, float r){
    mesh_clear(m);
    const float t=0.525731112f, u=0.850650808f;
    const Vec3 V[12]={
        {-t, 0,  u},{ t, 0,  u},{-t, 0, -u},{ t, 0, -u},
        { 0, u,  t},{ 0, u, -t},{ 0,-u,  t},{ 0,-u, -t},
        { u, t,  0},{-u, t,  0},{ u,-t,  0},{-u,-t,  0}
    };
    const unsigned F[60]={
        0,4,1, 0,9,4, 9,5,4, 4,5,8, 4,8,1,
        8,10,1, 8,3,10, 5,3,8, 5,2,3, 2,7,3,
        7,10,3, 7,6,10, 7,11,6, 11,0,6, 0,1,6,
        6,1,10, 9,0,11, 9,11,2, 9,2,5, 7,2,11
    };
    ensure_v(m,12); ensure_i(m,60);
    const Color4 white={1,1,1,1}; const UV zuv={0,0};
    int base=m->vertex_count;
    for(int i=0;i<12;i++){
        Vec3 d=vec3_normalize(V[i]);
        m->vertices[m->vertex_count++]=(VertexFormat){ v3(c.x+d.x*r, c.y+d.y*r, c.z+d.z*r), d, white, zuv };
    }
    for(int i=0;i<60;i++) m->indices[m->index_count++]=base+F[i];
}
