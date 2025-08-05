#include "defs.h"
#include <time.h>
static uint32_t seed = 12355;
#define EPSILON 0.0001f

uint32_t xorshift32() {
    seed += clock();
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

float smoothstep(float edge0, float edge1, float x) {
    float t = (x - edge0) / (edge1 - edge0);
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    return t * t * (3.0f - 2.0f * t);
}

float hashf3(Vec3 p) {
    uint32_t h = (uint32_t)(p.x * 374761393.0f + p.y * 668265263.0f + p.z * 982451653.0f);
    h ^= h >> 13;
    h *= 1274126177;
    h ^= h >> 16;
    return (h & 0xFFFFFF) / (float)0xFFFFFF;
}

float noise3f(Vec3 p) {
    Vec3 ip = { floorf(p.x), floorf(p.y), floorf(p.z) };
    Vec3 fp = { p.x - ip.x, p.y - ip.y, p.z - ip.z };

    float acc = 0.0f;
    for (int dz = 0; dz <= 1; dz++)
    for (int dy = 0; dy <= 1; dy++)
    for (int dx = 0; dx <= 1; dx++) {
        Vec3 corner = { ip.x + dx, ip.y + dy, ip.z + dz };
        float val = hashf3(corner);
        float wx = smoothstep(0, 1, dx ? fp.x : 1 - fp.x);
        float wy = smoothstep(0, 1, dy ? fp.y : 1 - fp.y);
        float wz = smoothstep(0, 1, dz ? fp.z : 1 - fp.z);
        acc += val * wx * wy * wz;
    }
    return acc;
}
float fbm(Vec3 p, int octaves, float persistence, float lacunarity) {
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float total = 0.0f;

    for (int i = 0; i < octaves; i++) {
        Vec3 sample = {
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

int vertex_equals(VertexFormat* a, VertexFormat* b) {
    return vec3_equals(a->position, b->position) &&
           vec3_equals(a->normal, b->normal) &&
           vec4_equals(a->tangent, b->tangent) &&
           color4_equals(a->color, b->color) &&
           uv_equals(a->uv, b->uv) &&
           float_equals(a->roughness, b->roughness) &&
           float_equals(a->metallic, b->metallic) &&
           float_equals(a->emissive, b->emissive);
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
    mesh->indices = malloc(sizeof(unsigned int) * index_capacity);

    mesh->vao = 0;
    mesh->vbo = 0;
    mesh->ibo = 0;

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
    glBufferData(GL_ARRAY_BUFFER, mesh->vertex_count * sizeof(VertexFormat), mesh->vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->index_count * sizeof(unsigned int), mesh->indices, GL_STATIC_DRAW);

    size_t stride = sizeof(VertexFormat);
    size_t offset = 0;

    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offset);
    offset += sizeof(Vec3);

    glEnableVertexAttribArray(1); // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offset);
    offset += sizeof(Vec3);

    glEnableVertexAttribArray(2); // tangent
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)offset);
    offset += sizeof(Vec4);

    glEnableVertexAttribArray(3); // color
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)offset);
    offset += sizeof(Color4);

    glEnableVertexAttribArray(4); // uv
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, stride, (void*)offset);
    offset += sizeof(UV);

    glEnableVertexAttribArray(5); // roughness
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, stride, (void*)offset);
    offset += sizeof(float);

    glEnableVertexAttribArray(6); // metallic
    glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, stride, (void*)offset);
    offset += sizeof(float);

    glEnableVertexAttribArray(7); // emissive
    glVertexAttribPointer(7, 1, GL_FLOAT, GL_FALSE, stride, (void*)offset);
    offset += sizeof(float);

    glBindVertexArray(0); // 🔐 Good practice
}


void draw_object(const Object* obj, ShaderProgram* shader, Mesh* mesh_pool[], GLenum primitive_type) {
    if (!obj || !shader || !mesh_pool || !(obj->flags & OBJ_FLAG_VISIBLE)) return;

    Mesh* mesh = mesh_pool[obj->mesh_id];
    if (!mesh) return;

    const Material* m = material_pool[mesh->material_id];
    if (!m) return;

    Mat4 T, R, S, TR, model;
    mat4_identity(T);
    mat4_translate(T, obj->position.x, obj->position.y, obj->position.z);
    quat_to_matrix(&obj->rotation, R);
    mat4_identity(S);
    mat4_scale(S, obj->scale.x, obj->scale.y, obj->scale.z);
    mat4_multiply(TR, T, R);
    mat4_multiply(model, TR, S);

    glUniformMatrix4fv(shader->u_model_loc, 1, GL_FALSE, model);
    glUniform4fv(shader->u_material_albedo_loc, 1, &m->albedo.r);
    glUniform1f(shader->u_material_roughness_loc, m->roughness);
    glUniform1f(shader->u_material_metallic_loc,  m->metallic);
    glUniform1f(shader->u_material_emissive_loc,  m->emissive);

    draw_mesh(mesh, primitive_type);
}

