#include "defs.h"
#include <gl/gl.h>
#include <time.h>
static uint32_t seed = 12355;

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
float meTanf(float num){
    return sin(num) / cos(num);
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
void quat_to_matrix(const Quat* q, float* m) {
    float x2 = q->x + q->x, y2 = q->y + q->y, z2 = q->z + q->z;
    float xx = q->x * x2, yy = q->y * y2, zz = q->z * z2;
    float xy = q->x * y2, xz = q->x * z2, yz = q->y * z2;
    float wx = q->w * x2, wy = q->w * y2, wz = q->w * z2;

    m[0] = 1.0f - (yy + zz);
    m[1] = xy + wz;
    m[2] = xz - wy;
    m[3] = 0.0f;

    m[4] = xy - wz;
    m[5] = 1.0f - (xx + zz);
    m[6] = yz + wx;
    m[7] = 0.0f;

    m[8]  = xz + wy;
    m[9]  = yz - wx;
    m[10] = 1.0f - (xx + yy);
    m[11] = 0.0f;

    m[12] = m[13] = m[14] = 0.0f;
    m[15] = 1.0f;
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
Quat quat_conjugate(Quat q) {
    return (Quat){ -q.x, -q.y, -q.z, q.w };
}
void SetProjectionMatrix(float zNear, float zFar) {
    float fovY = 90.0f;
    float f = 1.0f / meTanf((fovY * pi / 180.0f) / 2.0f); // Convert degrees to radians
    float mat[16] = {0};

    mat[0] = f / aspectRatio;
    mat[5] = f;
    mat[10] = (zFar + zNear) / (zNear - zFar);
    mat[11] = -1.0f;
    mat[14] = (2.0f * zFar * zNear) / (zNear - zFar);
    mat[15] = 0.0f;

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(mat);
    glMatrixMode(GL_MODELVIEW);
}
Vec3 direction_between(Vec3 a, Vec3 b) {
    Vec3 direction;
    direction.x = b.x - a.x;
    direction.y = b.y - a.y;
    direction.z = b.z - a.z;
    return direction;
}
void* normalize(void* v) {
    if (v == NULL) return v;
    Vec3* vec = (Vec3*)v; 
    float length = sqrtf(vec->x * vec->x + vec->y * vec->y + vec->z * vec->z);
    if (length > 0) {
        vec->x /= length;
        vec->y /= length;
        vec->z /= length;
    }
    return v;
}
void* multiply3f(void* v, float scalar) {
    if (v == NULL || scalar == 0.0f) return v;
    
    Vec3* vec = (Vec3*)v;
    vec->x *= scalar;
    vec->y *= scalar;
    vec->z *= scalar;

    return v;
}

void init_mesh(Mesh* mesh, int initial_capacity) {
    mesh->vertex_count = 0;
    mesh->vertex_capacity = initial_capacity;

    mesh->index_count = 0;
    mesh->index_capacity = initial_capacity * 3; // rough estimate

    mesh->vertices = malloc(sizeof(Vec3) * mesh->vertex_capacity);
    mesh->normals = malloc(sizeof(Vec3) * mesh->vertex_capacity);
    mesh->colors = malloc(sizeof(Color4) * mesh->vertex_capacity);
    mesh->uvs = malloc(sizeof(UV) * mesh->vertex_capacity);
    mesh->indices = malloc(sizeof(unsigned int) * mesh->index_capacity);
    mesh->flags = (1 << 0) | (1 << 1) | (1 << 2); // normals, colors, uvs

    mesh->material.ambient  = (Color4){ 0.2f, 0.2f, 0.2f, 1.0f };
    mesh->material.diffuse  = (Color4){ 0.6f, 0.6f, 0.6f, 1.0f };
    mesh->material.specular = (Color4){ 1.0f, 1.0f, 1.0f, 1.0f };
    mesh->material.shininess = 32.0f;

}
void draw_mesh(const Mesh* mesh, GLenum primitive_type) {
    
    if (!mesh || mesh->index_count <= 0 || !mesh->vertices || !mesh->indices) return;

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, mesh->vertices);

    if (mesh->flags & (1 << 0)) {
        glEnableClientState(GL_NORMAL_ARRAY);
        glNormalPointer(GL_FLOAT, 0, mesh->normals);
    }

    if (mesh->flags & (1 << 1)) {
        glEnableClientState(GL_COLOR_ARRAY);
        glColorPointer(4, GL_FLOAT, 0, mesh->colors);
    }

    if (mesh->flags & (1 << 2)) {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, 0, mesh->uvs);
    }

    glDrawElements(primitive_type, mesh->index_count, GL_UNSIGNED_INT, mesh->indices);

    glDisableClientState(GL_VERTEX_ARRAY);
    if (mesh->flags & (1 << 0)) glDisableClientState(GL_NORMAL_ARRAY);
    if (mesh->flags & (1 << 1)) glDisableClientState(GL_COLOR_ARRAY);
    if (mesh->flags & (1 << 2)) glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}
int find_or_add_vertex(Mesh* mesh, Vec3 pos, Color4 color, UV uv) {
    for (int i = 0; i < mesh->vertex_count; i++) {
        if (memcmp(&mesh->vertices[i], &pos, sizeof(Vec3)) == 0 &&
            memcmp(&mesh->colors[i], &color, sizeof(Color4)) == 0 &&
            memcmp(&mesh->uvs[i], &uv, sizeof(UV)) == 0) {
            return i;
        }
    }

    if (mesh->vertex_count >= mesh->vertex_capacity) {
        mesh->vertex_capacity *= 2;
        mesh->vertices = realloc(mesh->vertices, sizeof(Vec3) * mesh->vertex_capacity);
        mesh->normals = realloc(mesh->normals, sizeof(Vec3) * mesh->vertex_capacity);
        mesh->colors = realloc(mesh->colors, sizeof(Color4) * mesh->vertex_capacity);
        mesh->uvs = realloc(mesh->uvs, sizeof(UV) * mesh->vertex_capacity);
    }

    int index = mesh->vertex_count++;
    mesh->vertices[index] = pos;
    mesh->colors[index] = color;
    mesh->uvs[index] = uv;
    mesh->normals[index] = (Vec3){0}; // zero for now
    return index;
}


void draw_object(const Object* obj, GLenum primitive_type) {

    glPushMatrix();

    // Apply transformations (Position, Rotation, Scale)
    glTranslatef(obj->position.x, obj->position.y, obj->position.z);  // Translation (position)
    
    Quat normalized = quat_normalize(obj->rotation);
    float mat[16];
    quat_to_matrix(&normalized, mat);
    glMultMatrixf(mat);

    // Scale the object
    glScalef(obj->scale.x, obj->scale.y, obj->scale.z);  // Scale

    GLfloat mat_specular[] = { 1.0, 1.0, 1.0, 1.0 }; // sparkle sparkle
    GLfloat mat_shininess[] = { 64.0f };

    const Material* m = &obj->mesh.material;

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT,  m->ambient.data);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE,  m->diffuse.data);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, m->specular.data);
    glMaterialf (GL_FRONT_AND_BACK, GL_SHININESS, m->shininess);



    if (obj->flags & OBJ_FLAG_NORMALS_DIRTY) {
    glDisable(GL_LIGHTING);
    }
    //if (!(obj->flags & OBJ_FLAG_NEEDS_REBUILD)) {
        draw_mesh(&obj->mesh, primitive_type);
    //}

    glPopMatrix();
    glEnable(GL_LIGHTING);
}

void debug_object(const Object* obj) {
    if (!obj || obj->mesh.vertex_count <= 0 || !obj->mesh.vertices) return;

    glDisable(GL_LIGHTING);
    glColor3f(1.0f, 0.0f, 1.0f);

    glPushMatrix();

    // Apply translation (position)
    glTranslatef(obj->position.x, obj->position.y, obj->position.z);  

    // Apply rotation (using quaternion to matrix)
    Quat normalized = quat_normalize(obj->rotation);
    float mat[16];
    quat_to_matrix(&normalized, mat);
    glMultMatrixf(mat);

    // Apply scaling
    glScalef(obj->scale.x, obj->scale.y, obj->scale.z);  

    // Iterate over all triangles in the mesh
    for (int i = 0; i < obj->mesh.vertex_count; i += 3) {
        Vec3 a = obj->mesh.vertices[i];
        Vec3 b = obj->mesh.vertices[i + 1];
        Vec3 c = obj->mesh.vertices[i + 2];

        Vec3 u = { b.x - a.x, b.y - a.y, b.z - a.z };
        Vec3 v = { c.x - a.x, c.y - a.y, c.z - a.z };
        Vec3 n = {
            u.y * v.z - u.z * v.y,
            u.z * v.x - u.x * v.z,
            u.x * v.y - u.y * v.x
        };

        // Normalize the normal
        float len = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
        if (len > 0.0f) {
            n.x /= len; n.y /= len; n.z /= len;
        }

        // Render the normal lines
        glBegin(GL_LINES);
            glVertex3f(a.x, a.y, a.z);
            glVertex3f(a.x + n.x * 0.1f, a.y + n.y * 0.1f, a.z + n.z * 0.1f);  // Scale normal for visibility
        glEnd();
        glBegin(GL_LINES);
            glVertex3f(b.x, b.y, b.z);
            glVertex3f(b.x + n.x * 0.1f, b.y + n.y * 0.1f, b.z + n.z * 0.1f);
        glEnd();
        glBegin(GL_LINES);
            glVertex3f(c.x, c.y, c.z);
            glVertex3f(c.x + n.x * 0.1f, c.y + n.y * 0.1f, c.z + n.z * 0.1f);
        glEnd();
    }

    glPopMatrix();

    // Re-enable lighting and reset color
    glEnable(GL_LIGHTING);
    glColor3f(1.0f, 1.0f, 1.0f);
}
