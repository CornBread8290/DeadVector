#ifndef DEFS_H
#define DEFS_H

#include <math.h>
#include <windows.h>
#include <gl/gl.h>

extern float aspectRatio;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef short int int16_t;

typedef struct {
    float x, y;
} Vec2;
typedef union {
    struct { float x, y, z; };
    float data[3];
} Vec3;
typedef Vec3 Normal;
typedef Vec3 Vertex;
typedef struct { float x, y, z, w; } Quat;
typedef union {
    struct { float r, g, b; };
    float data[3];
} Color3;
typedef union {
    struct { float r, g, b, a; };
    float data[4];
} Color4;
typedef struct {
    Color4 ambient;
    Color4 diffuse;
    Color4 specular;
    float shininess;
} Material;
typedef struct { float u, v; } UV;
typedef struct {
    Vertex* vertices;
    Normal* normals;
    Color4* colors;
    UV* uvs;
    unsigned int* indices;

    int vertex_count;
    int vertex_capacity;

    int index_count;
    int index_capacity;

    unsigned int flags;

    Material material;
} Mesh;
#define MESH_HAS_NORMALS (1 << 0)
#define MESH_HAS_COLORS  (1 << 1)
#define MESH_HAS_UVS     (1 << 2)
typedef struct {
    Vec3 position;
    Quat rotation;
    Vec3 scale;
    Mesh mesh;
    unsigned int flags;
    void** components;
} Object;
#define OBJ_FLAG_NORMALS_DIRTY (1 << 0)
#define OBJ_FLAG_NEEDS_REBUILD  (1 << 1)

#define pi 3.14159265358979323846f
#define pi2 (pi * 2.0f)
#define DEG2RAD (3.14159265f / 180.0f)

#define NUM_STARS 80000


enum SoundType {
    SND_BEEP = 0,
    SND_ENGINE,
    SND_GUN,
    SND_HARSH
};


uint32_t xorshift32(void);
float xorshift32f(void);
uint32_t wangHash(uint32_t seed);
//float lerp(float a, float b, float t);
//float smoothstep(float edge0, float edge1, float x);
//float hashf3(Vec3 p);
float noise3f(Vec3 p);
void playSoundEffect(int type, float pitch, float volume, float pan, int channel);
void stopSound(int channel);
float fbm(Vec3 p, int octaves, float persistence, float lacunarity);
Quat quat_mul(Quat a, Quat b);
Quat quat_axis_angle(float x, float y, float z, float angle_rad);
Quat quat_normalize(Quat q);
void quat_to_matrix(const Quat* q, float* m);
Vec3 quat_rotate_vec3(Quat q, Vec3 v);
Quat quat_conjugate(Quat q);
void SetProjectionMatrix(float zNear, float zFar);
Vec3 vec3_normalize(Vec3 v);
Vec3 vec3_sub(Vec3 a, Vec3 b);
Vec3 vec3_cross(Vec3 a, Vec3 b);
Vec3 vec3_add(Vec3 a, Vec3 b);
Vec3 vec3_scale(Vec3 v, float s);

Vec3 direction_between(Vec3 a, Vec3 b);
void* normalize(void* v);
void* multiply3f(void* v, float scalar);

void draw_mesh(const Mesh* mesh, GLenum primitive_type);
void init_mesh(Mesh* mesh, int initial_capacity);

void debug_mesh(const Mesh* mesh);
void draw_object(const Object* obj, GLenum primitive_type);
void debug_object(const Object* obj);
int find_or_add_vertex(Mesh* mesh, Vec3 pos, Color4 color, UV uv);
typedef struct {
    Vec3 normal_sum;
    int count;
} NormalAccumulator;
#endif // DEFS_H
